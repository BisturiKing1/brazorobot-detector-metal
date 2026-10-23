#include "state_machine.h"

RobotStateMachine::RobotStateMachine(RobotArm& arm, SensorManager& sensors)
    : _arm(arm), _sensors(sensors) {}

void RobotStateMachine::begin() {
    _telemetry.mode                  = SystemMode::AUTO;
    _telemetry.metalCount            = 0;
    _telemetry.nonMetalCount         = 0;
    _telemetry.faultCount            = 0;
    _telemetry.activeRouteStep       = 0;
    _telemetry.lastResult            = "Sistema inicializando...";
    _telemetry.safetyConditionActive = false;

    enterState(FsmState::INIT_SAFE_CHECK, "Inicializando: verificando seguridad y posicionando en HOME...");
}

void RobotStateMachine::enterState(FsmState newState, const String& statusMsg) {
    _telemetry.state      = newState;
    _telemetry.statusText = statusMsg;
    _stateEnterMs         = millis();
    _waitingStepDwell     = false;

    switch (newState) {
        case FsmState::INIT_SAFE_CHECK:         _telemetry.stateName = "INIT_SAFE_CHECK"; break;
        case FsmState::IDLE_WAIT_PART:          _telemetry.stateName = "IDLE_WAIT_PART"; break;
        case FsmState::VERIFY_MATERIAL:         _telemetry.stateName = "VERIFY_MATERIAL"; break;
        case FsmState::EXECUTE_METAL_ROUTE:     _telemetry.stateName = "EXECUTE_METAL_ROUTE"; break;
        case FsmState::EXECUTE_NON_METAL_ROUTE: _telemetry.stateName = "EXECUTE_NON_METAL_ROUTE"; break;
        case FsmState::RETURN_TO_HOME:          _telemetry.stateName = "RETURN_TO_HOME"; break;
        case FsmState::WAIT_PART_REMOVAL:       _telemetry.stateName = "WAIT_PART_REMOVAL"; break;
        case FsmState::MANUAL_READY:            _telemetry.stateName = "MANUAL_READY"; break;
        case FsmState::HOMING_SEQUENCE:         _telemetry.stateName = "HOMING_SEQUENCE"; break;
        case FsmState::SAFETY_STOP:             _telemetry.stateName = "SAFETY_STOP"; break;
    }
}

void RobotStateMachine::enterSafetyFault(const String& faultReason) {
    _arm.lockSafetyStop();
    _telemetry.mode                  = SystemMode::FAULT_LOCKED;
    _telemetry.safetyConditionActive = true;
    _telemetry.faultCount++;
    _telemetry.lastResult            = "FALLA: " + faultReason;
    enterState(FsmState::SAFETY_STOP, "¡PARO DE SEGURIDAD! " + faultReason + " (Requiere revisión manual)");
}

bool RobotStateMachine::isManualMovementAllowed() const {
    if (_telemetry.mode != SystemMode::MANUAL) return false;
    if (_telemetry.safetyConditionActive || _arm.isSafetyLocked()) return false;
    return true;
}

void RobotStateMachine::update() {
    const SensorState& s = _sensors.getState();
    uint32_t now = millis();

    // Si está en paro de seguridad enclavado, NUNCA reanuda movimientos automáticamente
    if (_telemetry.mode == SystemMode::FAULT_LOCKED || _telemetry.state == FsmState::SAFETY_STOP) {
        _telemetry.safetyConditionActive = true;
        return;
    }

    // 1. Verificación continua de atasco de servomotores (tiempo máximo de movimiento superado)
    uint32_t maxMoveMs = _arm.getCalibration().maxMoveTimeoutMs;
    if (_arm.isMoving() && (_arm.getCurrentMoveElapsedMs(now) > maxMoveMs)) {
        enterSafetyFault("Tiempo máximo de movimiento superado (posible servo atascado)");
        return;
    }

    // 2. Verificación de finales de carrera fuera de Homing
    if (_telemetry.state != FsmState::HOMING_SEQUENCE && _arm.wasBlockedByLimitSwitch()) {
        if (_telemetry.mode == SystemMode::AUTO) {
            enterSafetyFault("Final de carrera de Base activado durante ciclo automático");
            return;
        } else {
            _telemetry.statusText = "Movimiento detenido: Final de carrera de Base alcanzado";
        }
    }

    // 3. Ejecución estricta según el estado actual
    switch (_telemetry.state) {
        case FsmState::INIT_SAFE_CHECK:
            updateInitSafeCheck(s, now);
            break;

        case FsmState::IDLE_WAIT_PART:
            updateIdleWaitPart(s, now);
            break;

        case FsmState::VERIFY_MATERIAL:
            updateVerifyMaterial(s, now);
            break;

        case FsmState::EXECUTE_METAL_ROUTE:
            updateRouteExecution(_arm.getMetalRoute(), true, s, now);
            break;

        case FsmState::EXECUTE_NON_METAL_ROUTE:
            updateRouteExecution(_arm.getNonMetalRoute(), false, s, now);
            break;

        case FsmState::RETURN_TO_HOME:
            updateReturnToHome(s, now);
            break;

        case FsmState::WAIT_PART_REMOVAL:
            updateWaitPartRemoval(s, now);
            break;

        case FsmState::HOMING_SEQUENCE:
            updateHoming(s, now);
            break;

        case FsmState::MANUAL_READY:
            if (s.limitLeft || s.limitRight) {
                _telemetry.statusText = "Modo Manual: Tope físico activo (movimiento en ese sentido bloqueado)";
            }
            break;

        case FsmState::SAFETY_STOP:
        default:
            break;
    }
}

void RobotStateMachine::updateInitSafeCheck(const SensorState& s, uint32_t now) {
    // Verificar condiciones iniciales de seguridad
    if (s.limitLeft && s.limitRight) {
        enterSafetyFault("Ambos finales de carrera (GPIO 5 y 6) activos simultáneamente");
        return;
    }

    if (now - _stateEnterMs < 150) {
        _arm.setTargetPose(_arm.getHomePose());
        return;
    }

    if (!_arm.isMoving()) {
        if (_telemetry.mode == SystemMode::AUTO) {
            if (s.partPresent) {
                enterState(FsmState::WAIT_PART_REMOVAL, "Retire la pieza presente antes de iniciar el ciclo");
            } else {
                enterState(FsmState::IDLE_WAIT_PART, "Posición inicial segura establecida - Esperando pieza (HC-SR04)");
            }
        } else {
            enterState(FsmState::MANUAL_READY, "Posición inicial segura lista - Modo Manual y Calibración");
        }
    }
}

void RobotStateMachine::updateIdleWaitPart(const SensorState& s, uint32_t now) {
    // Espera confirmación de presencia del HC-SR04
    if (s.partPresent) {
        enterState(FsmState::VERIFY_MATERIAL, "Pieza detectada por HC-SR04 -> Verificando material con sensor inductivo...");
    }
}

void RobotStateMachine::updateVerifyMaterial(const SensorState& s, uint32_t now) {
    // Si la pieza se retira antes de cumplir el tiempo de verificación, volver a espera
    if (!s.partPresent && !s.anyMetalDetected) {
        enterState(FsmState::IDLE_WAIT_PART, "Lectura transitoria descartada - Esperando pieza estable");
        return;
    }

    uint16_t verifyMs = _arm.getCalibration().verifyDelayMs;

    // Esperar ventana de confirmación o disparo inductivo firme
    if ((now - _stateEnterMs) >= verifyMs) {
        if (s.anyMetalDetected) {
            // Es METAL: Iniciar ruta de clasificación de metal
            const ProgrammableRoute& mr = _arm.getMetalRoute();
            if (mr.stepCount == 0) {
                enterSafetyFault("Ruta de Metal vacía: configure al menos 1 paso en el Portal");
                return;
            }
            _routeStepIdx = 0;
            _telemetry.activeRouteStep = 1;
            _waitingStepDwell = false;
            _arm.setTargetPose(mr.steps[0].pose);
            _telemetry.lastResult = s.inductive1 ? "Metal confirmado (Sensor 1 - GPIO 7)" : "Metal confirmado (Sensor 2 - GPIO 4)";
            enterState(FsmState::EXECUTE_METAL_ROUTE, String("Metal detectado -> Ejecutando: ") + mr.steps[0].label);
        } else {
            // NO ES METÁLICA (Detectada por HC-SR04 pero sensores inductivos inactivos)
            // Toma la pieza y la coloca en el destino de No-Metal
            const ProgrammableRoute& nmr = _arm.getNonMetalRoute();
            if (nmr.stepCount > 0) {
                _routeStepIdx = 0;
                _telemetry.activeRouteStep = 1;
                _waitingStepDwell = false;
                _arm.setTargetPose(nmr.steps[0].pose);
                _telemetry.lastResult = "Pieza No Metálica (HC-SR04) -> Trasladando a destino No-Metal";
                enterState(FsmState::EXECUTE_NON_METAL_ROUTE, String("No-Metal detectado -> Ejecutando: ") + nmr.steps[0].label);
            } else {
                enterSafetyFault("Ruta de No-Metal vacía: configure al menos 1 paso en el Portal");
            }
        }
    }
}

void RobotStateMachine::updateRouteExecution(const ProgrammableRoute& route, bool isMetal, const SensorState& s, uint32_t now) {
    if (_arm.isMoving()) {
        return;
    }

    if (!_waitingStepDwell) {
        _waitingStepDwell = true;
        _stepArrivalMs    = now;
        return;
    }

    uint16_t dwell = route.steps[_routeStepIdx].dwellMs;
    if ((now - _stepArrivalMs) < dwell) {
        return;
    }

    // Avanzar al siguiente paso de la ruta grabada
    _routeStepIdx++;
    _waitingStepDwell = false;

    if (_routeStepIdx < route.stepCount) {
        _telemetry.activeRouteStep = _routeStepIdx + 1;
        _arm.setTargetPose(route.steps[_routeStepIdx].pose);
        _telemetry.statusText = String(isMetal ? "[Ruta Metal] " : "[Ruta No-Metal] ") +
                                "Paso " + String(_telemetry.activeRouteStep) + "/" + String(route.stepCount) +
                                ": " + route.steps[_routeStepIdx].label;
    } else {
        // Terminó la ruta -> Registrar resultado e iniciar retorno seguro en 2 fases:
        // Fase 0: Subir verticalmente a la altura segura de HOME manteniendo el ángulo actual de la Base
        // Fase 1: Ya con el brazo arriba, rotar la Base hacia HOME
        if (isMetal) {
            _telemetry.metalCount++;
            _telemetry.lastResult = "Ciclo Metal completado exitosamente";
        } else {
            _telemetry.nonMetalCount++;
            _telemetry.lastResult = "Ciclo No-Metal completado exitosamente";
        }
        _telemetry.activeRouteStep = 0;

        ArmPose liftFirst = _arm.getHomePose();
        liftFirst.base    = _arm.getCurrentAngle(ServoCfg::BASE);
        liftFirst.gripper = _arm.getCurrentAngle(ServoCfg::GRIPPER);
        _arm.setTargetPose(liftFirst);
        _returnHomePhase = 0;
        enterState(FsmState::RETURN_TO_HOME, "Pieza liberada -> 1/2 Subiendo a altura segura antes de rotar...");
    }
}

void RobotStateMachine::updateReturnToHome(const SensorState& s, uint32_t now) {
    // Fase 0: Esperar a que el brazo termine de subir en el sitio de descarga antes de girar la Base
    if (_returnHomePhase == 0) {
        if (!_arm.isMoving() && (now - _stateEnterMs >= 200)) {
            _arm.setTargetPose(_arm.getHomePose());
            _returnHomePhase = 1;
            _stateEnterMs    = now;
            _telemetry.statusText = "En altura segura -> 2/2 Rotando Base hacia HOME...";
        }
        return;
    }

    // Fase 1: Esperar a que termine de rotar a HOME
    if (!_arm.isMoving() && (now - _stateEnterMs >= 300)) {
        if (s.partPresent || s.anyMetalDetected) {
            enterState(FsmState::WAIT_PART_REMOVAL, "En posición inicial -> Esperando retiro de la pieza en zona de sensores");
        } else {
            if (_telemetry.mode == SystemMode::AUTO) {
                enterState(FsmState::IDLE_WAIT_PART, "Listo en HOME - Esperando nueva pieza (HC-SR04)");
            } else {
                enterState(FsmState::MANUAL_READY, "Prueba de ruta finalizada - Modo Manual");
            }
        }
    }
}

void RobotStateMachine::updateWaitPartRemoval(const SensorState& s, uint32_t now) {
    // Espera a que el sensor ultrasónico e inductivos confirmen que la pieza fue retirada
    if (!s.partPresent && !s.anyMetalDetected) {
        if ((now - _stateEnterMs) >= 600) {
            enterState(FsmState::IDLE_WAIT_PART, "Pieza retirada -> Sistema listo esperando siguiente pieza");
        }
    } else {
        _stateEnterMs = now; // Reinicia temporizador mientras siga presente la pieza
    }
}

void RobotStateMachine::updateHoming(const SensorState& s, uint32_t now) {
    if (_homingPhase == 0) {
        ArmPose safePose = _arm.getHomePose();
        safePose.base = _arm.getCurrentAngle(ServoCfg::BASE);
        _arm.setTargetPose(safePose);
        _homingPhase = 1;
        _stateEnterMs = now;
        _telemetry.statusText = "Homing: Elevando brazo a altura segura...";
        return;
    }

    if (_homingPhase == 1) {
        if (!_arm.isMoving()) {
            _savedSpeed = _arm.getSpeedDps();
            _arm.setSpeedDps(ServoCfg::HOMING_SPEED_DPS);

            float targetBase = ServoCfg::LEFT_IS_DECREASING
                               ? _arm.getLimits().minAngle[ServoCfg::BASE]
                               : _arm.getLimits().maxAngle[ServoCfg::BASE];
            _arm.setJointTarget(ServoCfg::BASE, targetBase);
            _homingPhase = 2;
            _stateEnterMs = now;
            _telemetry.statusText = "Homing: Buscando final de carrera de Base (GPIO 5 / 6)...";
        }
        return;
    }

    if (_homingPhase == 2) {
        bool reachedLimit = s.limitLeft || s.limitRight;
        bool timedOut     = (now - _stateEnterMs) > Timing::HOMING_TIMEOUT_MS;

        if (reachedLimit || !_arm.isMoving() || timedOut) {
            _arm.setSpeedDps(_savedSpeed);
            _arm.setTargetPose(_arm.getHomePose());
            _homingPhase = 3;
            _stateEnterMs = now;
            _telemetry.statusText = "Homing: Llevando brazo a posición inicial segura (HOME)...";
        }
        return;
    }

    if (_homingPhase == 3) {
        if (!_arm.isMoving()) {
            _telemetry.mode = SystemMode::MANUAL;
            enterState(FsmState::MANUAL_READY, "Homing completado - Posición inicial establecida");
        }
    }
}

bool RobotStateMachine::requestModeChange(SystemMode newMode) {
    // No permite cambiar de modo automáticamente si existe una condición de seguridad activa
    if (_telemetry.safetyConditionActive || _arm.isSafetyLocked()) {
        return false;
    }

    _telemetry.mode = newMode;
    _telemetry.activeRouteStep = 0;

    if (newMode == SystemMode::AUTO) {
        _arm.setTargetPose(_arm.getHomePose());
        enterState(FsmState::INIT_SAFE_CHECK, "Iniciando Modo Automático: yendo a posición inicial segura...");
    } else if (newMode == SystemMode::MANUAL) {
        enterState(FsmState::MANUAL_READY, "Modo Manual / Calibración Activo");
    }
    return true;
}

bool RobotStateMachine::startHoming() {
    if (_telemetry.safetyConditionActive || _arm.isSafetyLocked()) {
        return false;
    }
    _savedSpeed = _arm.getSpeedDps();
    _homingPhase = 0;
    _telemetry.mode = SystemMode::HOMING;
    enterState(FsmState::HOMING_SEQUENCE, "Iniciando secuencia de Homing...");
    return true;
}

bool RobotStateMachine::triggerTestRoute(bool isMetal) {
    if (_telemetry.safetyConditionActive || _arm.isSafetyLocked()) {
        return false;
    }
    const ProgrammableRoute& rt = isMetal ? _arm.getMetalRoute() : _arm.getNonMetalRoute();
    if (rt.stepCount == 0) return false;

    _routeStepIdx = 0;
    _telemetry.activeRouteStep = 1;
    _waitingStepDwell = false;
    _arm.setTargetPose(rt.steps[0].pose);

    if (isMetal) {
        enterState(FsmState::EXECUTE_METAL_ROUTE, String("Probando Ruta Metal -> ") + rt.steps[0].label);
    } else {
        enterState(FsmState::EXECUTE_NON_METAL_ROUTE, String("Probando Ruta No-Metal -> ") + rt.steps[0].label);
    }
    return true;
}

void RobotStateMachine::triggerEmergencyStop(const String& reason) {
    enterSafetyFault(reason);
}

bool RobotStateMachine::acknowledgeAndClearFault() {
    const SensorState& s = _sensors.getState();
    if (s.limitLeft && s.limitRight) {
        _telemetry.statusText = "No se puede liberar: Ambos finales de carrera siguen presionados";
        return false;
    }

    _arm.unlockSafetyStop();
    _telemetry.safetyConditionActive = false;
    _telemetry.mode                  = SystemMode::MANUAL;
    _telemetry.activeRouteStep       = 0;
    _telemetry.lastResult            = "Falla revisada y liberada por el operador";
    enterState(FsmState::MANUAL_READY, "Seguridad restablecida -> Modo Manual Activo (Verifique posición antes de pasar a Auto)");
    return true;
}

void RobotStateMachine::resetCounters() {
    _telemetry.metalCount    = 0;
    _telemetry.nonMetalCount = 0;
    _telemetry.faultCount    = 0;
    _telemetry.lastResult    = "Contadores reiniciados";
}
