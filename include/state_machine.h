#pragma once
#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "robot_arm.h"

enum class SystemMode : uint8_t {
    AUTO = 0,
    MANUAL = 1,
    HOMING = 2,
    FAULT_LOCKED = 3
};

enum class FsmState : uint8_t {
    INIT_SAFE_CHECK = 0,     // Verificando condiciones de seguridad e yendo a HOME
    IDLE_WAIT_PART,          // Esperando pieza frente al HC-SR04
    VERIFY_MATERIAL,         // Confirmando presencia con HC-SR04 y leyendo sensor inductivo
    EXECUTE_METAL_ROUTE,     // Ejecutando los pasos grabados para pieza metálica
    EXECUTE_NON_METAL_ROUTE, // Ejecutando los pasos grabados para pieza no metálica
    RETURN_TO_HOME,          // Regresando a la posición inicial segura
    WAIT_PART_REMOVAL,       // Solicitando/esperando retiro de la pieza de la zona de detección
    MANUAL_READY,            // Modo manual / calibración de límites y rutas
    HOMING_SEQUENCE,         // Búsqueda de referencia con finales de carrera
    SAFETY_STOP              // Falla por servo atascado, final de carrera o paro de emergencia
};

struct SystemTelemetry {
    SystemMode mode;
    FsmState   state;
    String     stateName;
    String     statusText;
    String     lastResult;
    bool       safetyConditionActive;
    uint8_t    activeRouteStep;
    uint32_t   metalCount;
    uint32_t   nonMetalCount;
    uint32_t   faultCount;
};

class RobotStateMachine {
public:
    RobotStateMachine(RobotArm& arm, SensorManager& sensors);

    void begin();
    void update();

    // Acciones del operador desde el Portal Cautivo ("bot-detector")
    bool requestModeChange(SystemMode newMode);
    bool startHoming();
    bool triggerTestRoute(bool isMetal);
    void triggerEmergencyStop(const String& reason = "Paro de emergencia manual desde Portal");
    bool acknowledgeAndClearFault();
    void resetCounters();

    bool isManualMovementAllowed() const;
    const SystemTelemetry& getTelemetry() const { return _telemetry; }

private:
    RobotArm&      _arm;
    SensorManager& _sensors;

    SystemTelemetry _telemetry;
    uint32_t _stateEnterMs     = 0;
    uint32_t _stepArrivalMs    = 0;
    bool     _waitingStepDwell = false;
    uint8_t  _routeStepIdx     = 0;
    uint8_t  _returnHomePhase  = 0;
    uint8_t  _homingPhase      = 0;
    float    _savedSpeed       = ServoCfg::DEFAULT_SPEED_DPS;

    void enterState(FsmState newState, const String& statusMsg);
    void enterSafetyFault(const String& faultReason);

    void updateInitSafeCheck(const SensorState& s, uint32_t now);
    void updateIdleWaitPart(const SensorState& s, uint32_t now);
    void updateVerifyMaterial(const SensorState& s, uint32_t now);
    void updateRouteExecution(const ProgrammableRoute& route, bool isMetal, const SensorState& s, uint32_t now);
    void updateReturnToHome(const SensorState& s, uint32_t now);
    void updateWaitPartRemoval(const SensorState& s, uint32_t now);
    void updateHoming(const SensorState& s, uint32_t now);
};
