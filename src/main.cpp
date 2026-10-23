#include <Arduino.h>
#include <nvs_flash.h>
#include "config.h"
#include "sensors.h"
#include "robot_arm.h"
#include "state_machine.h"
#include "captive_portal.h"

static SemaphoreHandle_t    gStateMutex = nullptr;
static SensorManager        gSensors;
static RobotArm             gArm;
static RobotStateMachine    gStateMachine(gArm, gSensors);
static CaptivePortalServer* gPortal = nullptr;

// ============================================================================
// TAREA NÚCLEO 1: Control en Tiempo Real (50 Hz) - Ultrasonido, Inductivos, FSM y PCA9685
// ============================================================================
void taskRobotControl(void* parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t periodTicks = pdMS_TO_TICKS(Timing::CONTROL_LOOP_MS);
    const float dtSeconds = Timing::CONTROL_LOOP_MS / 1000.0f;

    for (;;) {
        if (xSemaphoreTake(gStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            // 1. Leer HC-SR04 (GPIO 10/11), Inductivos (GPIO 7/4) y Finales de Carrera (GPIO 5/6)
            float thresholdCm = gArm.getCalibration().partDetectDistCm;
            bool indActiveLow = gArm.getCalibration().inductiveActiveLow;
            gSensors.update(thresholdCm, indActiveLow);
            const SensorState& s = gSensors.getState();

            // 2. Actualizar Máquina de Estados Estricta (FSM)
            gStateMachine.update();

            // 3. Interpolar movimiento suave vigilando límites y finales de carrera
            gArm.update(dtSeconds, s.limitLeft, s.limitRight);

            xSemaphoreGive(gStateMutex);
        }
        vTaskDelayUntil(&lastWakeTime, periodTicks);
    }
}

// ============================================================================
// TAREA NÚCLEO 0: Portal Cautivo ("bot-detector") y Telemetría WebSocket
// ============================================================================
void taskNetworkPortal(void* parameter) {
    uint32_t lastBroadcastMs = 0;

    for (;;) {
        if (gPortal) {
            gPortal->processDns();

            uint32_t now = millis();
            if (now - lastBroadcastMs >= Timing::TELEMETRY_PUSH_MS) {
                lastBroadcastMs = now;
                gPortal->broadcastTelemetry();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);
    delay(400);

    Serial.println("======================================================");
    Serial.println("  BOT-DETECTOR | BRAZO CLASIFICADOR ESP32-S3");
    Serial.println("======================================================");

    // Inicializar partición NVS Flash explícitamente para garantizar persistencia al apagar
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    gStateMutex = xSemaphoreCreateMutex();

    // 1. Inicializar Sensores (HC-SR04 GPIO 10/11, Inductivos GPIO 7/4, Finales GPIO 5/6)
    gSensors.begin();
    Serial.println("[OK] Sensores inicializados (HC-SR04 + Inductivos + Finales de Carrera)");

    // 2. Inicializar PCA9685 (I2C SDA=8, SCL=9), límites seguros (15°-165°) y rutas desde NVS
    bool pcaOk = gArm.begin();
    if (pcaOk) {
        Serial.println("[OK] Driver PCA9685 en línea (0x40) - Límites y rutas cargados");
    } else {
        Serial.println("[WARN] PCA9685 no detectado en 0x40. Verifique alimentación y pines GPIO 8/9");
    }

    // 3. Inicializar Máquina de Estados en verificación inicial de seguridad
    gStateMachine.begin();

    // 4. Iniciar Punto de Acceso Wi-Fi "bot-detector" y Portal Cautivo
    gPortal = new CaptivePortalServer(gArm, gSensors, gStateMachine, gStateMutex);
    gPortal->begin();
    Serial.printf("[OK] Red Wi-Fi activa: \"%s\" -> Portal Cautivo: http://192.168.4.1\n", NetCfg::AP_SSID);

    // 5. Crear tareas en núcleos separados del ESP32-S3
    xTaskCreatePinnedToCore(
        taskRobotControl,
        "RobotControlTask",
        8192,
        nullptr,
        3,
        nullptr,
        1
    );

    xTaskCreatePinnedToCore(
        taskNetworkPortal,
        "CaptivePortalTask",
        8192,
        nullptr,
        1,
        nullptr,
        0
    );
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
