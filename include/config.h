#pragma once
#include <Arduino.h>

// ============================================================================
// 1. MAPEO DE PINES (ESP32-S3) - Según documentaci_n_de_conexiones_de_hardware.md
// ============================================================================
namespace Pins {
    constexpr uint8_t I2C_SDA           = 8;   // Línea de datos I2C hacia PCA9685
    constexpr uint8_t I2C_SCL           = 9;   // Línea de reloj I2C hacia PCA9685

    constexpr uint8_t IND_SENSOR_1      = 7;   // Sensor Inductivo 1 (PC817 V1) - Activo LOW
    constexpr uint8_t IND_SENSOR_2      = 4;   // Sensor Inductivo 2 (PC817 V2) - Activo LOW

    constexpr uint8_t LIMIT_BASE_LEFT   = 5;   // Final de Carrera 1 (Base Izquierda) - Activo LOW
    constexpr uint8_t LIMIT_BASE_RIGHT  = 6;   // Final de Carrera 2 (Base Derecha) - Activo LOW

    constexpr uint8_t ULTRA_TRIG        = 10;  // HC-SR04 Pin TRIG (Disparo 10 us HIGH)
    constexpr uint8_t ULTRA_ECHO        = 11;  // HC-SR04 Pin ECHO (Medición de pulso)
}

// ============================================================================
// 2. CONFIGURACIÓN DEL DRIVER PCA9685 Y SERVOMOTORES MG90S
// ============================================================================
namespace ServoCfg {
    constexpr uint8_t  I2C_ADDR         = 0x40;
    constexpr uint16_t PWM_FREQ_HZ      = 50;
    constexpr uint32_t I2C_CLOCK_HZ     = 400000;

    enum Joint : uint8_t {
        BASE      = 0,  // CH0: Rotación Izquierda / Derecha
        EXTENSION = 1,  // CH1: Brazo Extensión (Adelante / Atrás)
        ELEVATION = 2,  // CH2: Brazo Elevación (Arriba / Abajo)
        GRIPPER   = 3,  // CH3: Pinza (Abrir / Cerrar)
        COUNT     = 4
    };

    constexpr uint16_t MIN_PULSE_US     = 540;
    constexpr uint16_t MAX_PULSE_US     = 2400;

    // Intervalo seguro inicial requerido por especificación: 15° a 165°
    constexpr float DEFAULT_MIN_ANGLE   = 15.0f;
    constexpr float DEFAULT_MAX_ANGLE   = 165.0f;

    constexpr float DEFAULT_SPEED_DPS   = 70.0f;
    constexpr float HOMING_SPEED_DPS    = 35.0f;
    constexpr bool  LEFT_IS_DECREASING  = true;

    // Máximo de pasos personalizables en una ruta (Metal o No-Metal)
    constexpr uint8_t MAX_ROUTE_STEPS   = 8;
}

// ============================================================================
// 3. PARÁMETROS DE FILTRADO, ULTRASONIDO Y SEGURIDAD
// ============================================================================
namespace Timing {
    constexpr uint32_t CONTROL_LOOP_MS          = 20;   // 50 Hz control e interpolación
    constexpr uint32_t TELEMETRY_PUSH_MS        = 150;  // ~6.6 Hz actualización WebSocket
    constexpr uint32_t ULTRASONIC_PERIOD_MS     = 60;   // Medición HC-SR04 cada 60 ms
    constexpr uint32_t DEBOUNCE_LIMIT_MS        = 15;   // Antirrebote finales de carrera
    constexpr uint32_t DEBOUNCE_INDUCT_MS       = 35;   // Antirrebote sensores inductivos
    constexpr uint32_t DEFAULT_VERIFY_DELAY_MS  = 700;  // Tiempo para confirmar pieza y material
    constexpr uint32_t DEFAULT_MOVE_TIMEOUT_MS  = 4500; // Tiempo máx por movimiento antes de declarar atasco
    constexpr uint32_t HOMING_TIMEOUT_MS        = 8000;
}

// ============================================================================
// 4. CONFIGURACIÓN DE RED WIFI Y PORTAL CAUTIVO ("bot-detector")
// ============================================================================
namespace NetCfg {
    constexpr const char* AP_SSID       = "bot-detector";
    constexpr const char* AP_PASS       = ""; // Abierto para lanzamiento directo del Portal Cautivo
    constexpr uint8_t     AP_CHANNEL    = 6;
    constexpr uint8_t     MAX_CLIENTS   = 4;
}

// ============================================================================
// 5. ESTRUCTURAS DE DATOS: POSES, LÍMITES Y RUTAS PROGRAMABLES
// ============================================================================
struct ArmPose {
    float base;
    float extension;
    float elevation;
    float gripper;
};

struct JointLimitsConfig {
    float minAngle[ServoCfg::COUNT];
    float maxAngle[ServoCfg::COUNT];
};

struct RouteStep {
    ArmPose  pose;
    uint16_t dwellMs;     // Tiempo de espera al llegar al punto (ms)
    char     label[24];   // Etiqueta descriptiva (ej. "Acercar", "Cerrar Pinza", "Soltar")
};

struct ProgrammableRoute {
    uint8_t   stepCount;
    bool      enabled;    // Para No-Metal: true = ejecuta ruta; false = solicita retiro de pieza
    RouteStep steps[ServoCfg::MAX_ROUTE_STEPS];
};

struct CalibrationConfig {
    float    partDetectDistCm;   // Umbral HC-SR04 (cm) para considerar pieza presente (ej. 12.0 cm)
    uint16_t verifyDelayMs;      // Tiempo (ms) de estabilización antes de decidir Metal vs No-Metal
    uint16_t maxMoveTimeoutMs;   // Timeout (ms) para detectar servos atascados
    bool     inductiveActiveLow; // true = Metal lee LOW (0V); false = Metal lee HIGH (3.3V)
};

// Puntos de referencia maestros para generar rutas fluidas automáticamente
struct ReferenceWaypoints {
    ArmPose pickPoint;           // Posición exacta donde toma la pieza frente a los sensores
    ArmPose dropMetalPoint;      // Posición donde deposita las piezas metálicas
    ArmPose dropNonMetalPoint;   // Posición donde deposita las piezas no metálicas
    float   gripperOpenAngle;    // Ángulo de la pinza abierta (ej. 65°)
    float   gripperCloseAngle;   // Ángulo de la pinza cerrada sujetando pieza (ej. 95°)
    float   safeTravelElevation; // Altura segura (CH2 Elevación) para girar sin chocar (ej. 115°)
    float   safeTravelExtension; // Extensión segura (CH1) durante el giro (ej. 80°)
};

namespace DefaultPoses {
    // Pinza centrada en 75° (evita forzar topes físicos al abrir/cerrar)
    constexpr ArmPose HOME = { 90.0f, 70.0f, 115.0f, 75.0f };
}
