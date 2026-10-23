#include "robot_arm.h"
#include <math.h>
#include <string.h>

RobotArm::RobotArm() : _pca(ServoCfg::I2C_ADDR, Wire) {
    for (uint8_t i = 0; i < ServoCfg::COUNT; ++i) {
        _currentAngle[i]    = 90.0f;
        _targetAngle[i]     = 90.0f;
        _limits.minAngle[i] = ServoCfg::DEFAULT_MIN_ANGLE; // 15° por defecto
        _limits.maxAngle[i] = ServoCfg::DEFAULT_MAX_ANGLE; // 165° por defecto
    }
}

bool RobotArm::begin() {
    loadAllFromNvs();

    Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
    Wire.setClock(ServoCfg::I2C_CLOCK_HZ);

    Wire.beginTransmission(ServoCfg::I2C_ADDR);
    _pcaOnline = (Wire.endTransmission() == 0);

    if (_pcaOnline) {
        _pca.begin();
        _pca.setOscillatorFrequency(27000000);
        _pca.setPWMFreq(ServoCfg::PWM_FREQ_HZ);
        delay(10);
    }

    _currentAngle[ServoCfg::BASE]      = _targetAngle[ServoCfg::BASE]      = clampAngle(0, _homePose.base);
    _currentAngle[ServoCfg::EXTENSION] = _targetAngle[ServoCfg::EXTENSION] = clampAngle(1, _homePose.extension);
    _currentAngle[ServoCfg::ELEVATION] = _targetAngle[ServoCfg::ELEVATION] = clampAngle(2, _homePose.elevation);
    _currentAngle[ServoCfg::GRIPPER]   = _targetAngle[ServoCfg::GRIPPER]   = clampAngle(3, _homePose.gripper);

    if (_pcaOnline) {
        for (uint8_t i = 0; i < ServoCfg::COUNT; ++i) {
            writeServoAngle(i, _currentAngle[i]);
        }
    }
    return _pcaOnline;
}

void RobotArm::update(float dtSeconds, bool limitLeftActive, bool limitRightActive) {
    if (_safetyLock) {
        _wasMoving = false;
        return;
    }

    _blockedByLimit = false;
    float maxStep = _speedDps * dtSeconds;
    if (maxStep <= 0.0f) maxStep = 0.5f;

    bool anyJointMoving = false;

    for (uint8_t i = 0; i < ServoCfg::COUNT; ++i) {
        float diff = _targetAngle[i] - _currentAngle[i];

        if (fabsf(diff) < 0.05f) {
            _currentAngle[i] = _targetAngle[i];
            continue;
        }

        // Detención inmediata si el movimiento amenaza con sobrepasar un final de carrera activo
        if (i == ServoCfg::BASE) {
            bool movingLeft  = ServoCfg::LEFT_IS_DECREASING ? (diff < 0.0f) : (diff > 0.0f);
            bool movingRight = !movingLeft;

            if ((movingLeft && limitLeftActive) || (movingRight && limitRightActive)) {
                _targetAngle[ServoCfg::BASE] = _currentAngle[ServoCfg::BASE];
                _blockedByLimit = true;
                continue;
            }
        }

        anyJointMoving = true;

        if (fabsf(diff) <= maxStep) {
            _currentAngle[i] = _targetAngle[i];
        } else {
            _currentAngle[i] += (diff > 0.0f) ? maxStep : -maxStep;
        }

        _currentAngle[i] = clampAngle(i, _currentAngle[i]);
        writeServoAngle(i, _currentAngle[i]);
    }

    uint32_t now = millis();
    if (anyJointMoving && !_wasMoving) {
        _moveStartMs = now;
    }
    _wasMoving = anyJointMoving;

    // Protección térmica (Auto-Relax) para los 4 servomotores MG90S (Base, Extensión, Elevación, Pinza):
    // Cuando cualquier servo alcanza su ángulo objetivo y permanece quieto por más de 600 ms,
    // se corta su pulso PWM en el PCA9685 para evitar zumbido y sobrecalentamiento.
    for (uint8_t i = 0; i < ServoCfg::COUNT; ++i) {
        float diff = fabsf(_targetAngle[i] - _currentAngle[i]);
        if (diff < 0.1f) {
            if (_jointIdleMs[i] == 0) {
                _jointIdleMs[i] = now;
            } else if (!_jointDetached[i] && (now - _jointIdleMs[i] > 600)) {
                if (_pcaOnline) {
                    _pca.setPin(i, 0, false); // Corta señal PWM del canal i
                }
                _jointDetached[i] = true;
            }
        } else {
            _jointIdleMs[i]   = 0;
            _jointDetached[i] = false;
        }
    }
}

bool RobotArm::setTargetPose(const ArmPose& pose) {
    if (_safetyLock) return false;
    _targetAngle[ServoCfg::BASE]      = clampAngle(ServoCfg::BASE,      pose.base);
    _targetAngle[ServoCfg::EXTENSION] = clampAngle(ServoCfg::EXTENSION, pose.extension);
    _targetAngle[ServoCfg::ELEVATION] = clampAngle(ServoCfg::ELEVATION, pose.elevation);
    _targetAngle[ServoCfg::GRIPPER]   = clampAngle(ServoCfg::GRIPPER,   pose.gripper);
    _moveStartMs = millis();
    for (uint8_t i = 0; i < ServoCfg::COUNT; ++i) {
        _jointIdleMs[i]   = 0;
        _jointDetached[i] = false;
    }
    return true;
}

bool RobotArm::setJointTarget(ServoCfg::Joint joint, float angleDeg, bool limitLeftActive, bool limitRightActive) {
    if (_safetyLock || joint >= ServoCfg::COUNT) return false;

    float clamped = clampAngle(joint, angleDeg);
    if (joint == ServoCfg::BASE) {
        float diff = clamped - _currentAngle[ServoCfg::BASE];
        bool movingLeft  = ServoCfg::LEFT_IS_DECREASING ? (diff < 0.0f) : (diff > 0.0f);
        bool movingRight = !movingLeft;
        if ((movingLeft && limitLeftActive) || (movingRight && limitRightActive)) {
            return false; // Bloquea intento de mover hacia final de carrera presionado
        }
    }

    _targetAngle[joint]   = clamped;
    _moveStartMs          = millis();
    _jointIdleMs[joint]   = 0;
    _jointDetached[joint] = false;
    writeServoAngle(joint, _currentAngle[joint]);
    return true;
}

void RobotArm::setSpeedDps(float speedDps) {
    _speedDps = constrain(speedDps, 15.0f, 220.0f);
}

bool RobotArm::isMoving() const {
    for (uint8_t i = 0; i < ServoCfg::COUNT; ++i) {
        if (fabsf(_targetAngle[i] - _currentAngle[i]) > 0.4f) {
            return true;
        }
    }
    return false;
}

uint32_t RobotArm::getCurrentMoveElapsedMs(uint32_t nowMs) const {
    if (!isMoving()) return 0;
    return nowMs - _moveStartMs;
}

ArmPose RobotArm::getCurrentPose() const {
    return {
        _currentAngle[ServoCfg::BASE],
        _currentAngle[ServoCfg::EXTENSION],
        _currentAngle[ServoCfg::ELEVATION],
        _currentAngle[ServoCfg::GRIPPER]
    };
}

ArmPose RobotArm::getTargetPose() const {
    return {
        _targetAngle[ServoCfg::BASE],
        _targetAngle[ServoCfg::EXTENSION],
        _targetAngle[ServoCfg::ELEVATION],
        _targetAngle[ServoCfg::GRIPPER]
    };
}

void RobotArm::lockSafetyStop() {
    _safetyLock = true;
    for (uint8_t i = 0; i < ServoCfg::COUNT; ++i) {
        _targetAngle[i] = _currentAngle[i];
    }
    if (_pcaOnline) {
        for (uint8_t i = 0; i < ServoCfg::COUNT; ++i) {
            _pca.setPin(i, 0, false); // Corta pulsos PWM por seguridad
        }
    }
}

void RobotArm::unlockSafetyStop() {
    _safetyLock = false;
    _moveStartMs = millis();
    if (_pcaOnline) {
        for (uint8_t i = 0; i < ServoCfg::COUNT; ++i) {
            writeServoAngle(i, _currentAngle[i]);
        }
    }
}

float RobotArm::clampAngle(uint8_t channel, float angleDeg) const {
    if (channel >= ServoCfg::COUNT) return angleDeg;
    return constrain(angleDeg, _limits.minAngle[channel], _limits.maxAngle[channel]);
}

void RobotArm::writeServoAngle(uint8_t channel, float angleDeg) {
    if (!_pcaOnline || _safetyLock) return;
    float clamped = clampAngle(channel, angleDeg);
    uint16_t pulseUs = (uint16_t)(ServoCfg::MIN_PULSE_US +
                       (clamped / 180.0f) * (ServoCfg::MAX_PULSE_US - ServoCfg::MIN_PULSE_US));
    _pca.writeMicroseconds(channel, pulseUs);
}

void RobotArm::setJointLimits(uint8_t joint, float minDeg, float maxDeg) {
    if (joint >= ServoCfg::COUNT) return;
    minDeg = constrain(minDeg, 0.0f, 170.0f);
    maxDeg = constrain(maxDeg, minDeg + 5.0f, 180.0f);
    _limits.minAngle[joint] = minDeg;
    _limits.maxAngle[joint] = maxDeg;

    _currentAngle[joint] = clampAngle(joint, _currentAngle[joint]);
    _targetAngle[joint]  = clampAngle(joint, _targetAngle[joint]);
}

void RobotArm::saveLimitsToNvs() {
    if (_prefs.begin("bot_cfg", false)) {
        _prefs.putBytes("limits", &_limits, sizeof(JointLimitsConfig));
        _nvsSaveCount = _prefs.getUInt("save_cnt", 0) + 1;
        _prefs.putUInt("save_cnt", _nvsSaveCount);
        _prefs.end();
        _nvsHasSavedData = true;
    }
}

void RobotArm::saveCurrentAsHome() {
    _homePose = getCurrentPose();
    _waypoints.safeTravelElevation = _homePose.elevation;
    _waypoints.safeTravelExtension = _homePose.extension;
    if (_prefs.begin("bot_cfg", false)) {
        _prefs.putBytes("home", &_homePose, sizeof(ArmPose));
        _prefs.putBytes("waypts", &_waypoints, sizeof(ReferenceWaypoints));
        _nvsSaveCount = _prefs.getUInt("save_cnt", 0) + 1;
        _prefs.putUInt("save_cnt", _nvsSaveCount);
        _prefs.end();
        _nvsHasSavedData = true;
    }
}

void RobotArm::captureWaypoint(const String& which) {
    ArmPose cur = getCurrentPose();
    if (which == "pick") {
        _waypoints.pickPoint = cur;
    } else if (which == "drop_metal") {
        _waypoints.dropMetalPoint = cur;
    } else if (which == "drop_nonmetal") {
        _waypoints.dropNonMetalPoint = cur;
    } else if (which == "grip_open") {
        _waypoints.gripperOpenAngle = cur.gripper;
        _homePose.gripper = cur.gripper;
    } else if (which == "grip_close") {
        _waypoints.gripperCloseAngle = cur.gripper;
    } else if (which == "safe_height") {
        _waypoints.safeTravelElevation = cur.elevation;
        _waypoints.safeTravelExtension = cur.extension;
    }

    if (_prefs.begin("bot_cfg", false)) {
        _prefs.putBytes("waypts", &_waypoints, sizeof(ReferenceWaypoints));
        _prefs.putBytes("home", &_homePose, sizeof(ArmPose));
        _nvsSaveCount = _prefs.getUInt("save_cnt", 0) + 1;
        _prefs.putUInt("save_cnt", _nvsSaveCount);
        _prefs.end();
        _nvsHasSavedData = true;
    }

    // Reconstruir automáticamente ambas rutas con transiciones aéreas fluidas y seguras
    rebuildRoutesFromWaypoints();
}

void RobotArm::rebuildRoutesFromWaypoints() {
    auto setStep = [this](RouteStep& step, float b, float ex, float el, float gr, uint16_t dwell, const char* lbl) {
        step.pose.base      = clampAngle(ServoCfg::BASE,      b);
        step.pose.extension = clampAngle(ServoCfg::EXTENSION, ex);
        step.pose.elevation = clampAngle(ServoCfg::ELEVATION, el);
        step.pose.gripper   = clampAngle(ServoCfg::GRIPPER,   gr);
        step.dwellMs        = dwell;
        strncpy(step.label, lbl, sizeof(step.label) - 1);
        step.label[sizeof(step.label) - 1] = '\0';
    };

    const float gOpen   = _waypoints.gripperOpenAngle;
    const float gClose  = _waypoints.gripperCloseAngle;
    const float safeEl  = _waypoints.safeTravelElevation;
    const float safeEx  = _waypoints.safeTravelExtension;
    const ArmPose& pick = _waypoints.pickPoint;
    const ArmPose& dMet = _waypoints.dropMetalPoint;
    const ArmPose& dNom = _waypoints.dropNonMetalPoint;

    // Secuencia fluida de 7 pasos generada automáticamente para METAL:
    // 1. Alinear Base sobre pieza en altura segura con pinza abierta
    // 2. Bajar verticalmente hacia la pieza
    // 3. Cerrar pinza (sujetar pieza)
    // 4. Subir verticalmente a altura segura (evita arrastrar la pieza)
    // 5. Girar en alto hacia la zona de Descarga Metal
    // 6. Bajar a posición de Descarga Metal
    // 7. Abrir pinza y luego elevar para retorno limpio a HOME
    _metalRoute.stepCount = 7;
    setStep(_metalRoute.steps[0], pick.base, safeEx,         safeEl,         gOpen,  250, "1. Alinear sobre pieza");
    setStep(_metalRoute.steps[1], pick.base, pick.extension, pick.elevation, gOpen,  350, "2. Bajar a pieza");
    setStep(_metalRoute.steps[2], pick.base, pick.extension, pick.elevation, gClose, 500, "3. Cerrar pinza");
    setStep(_metalRoute.steps[3], pick.base, safeEx,         safeEl,         gClose, 300, "4. Elevar pieza seguro");
    setStep(_metalRoute.steps[4], dMet.base, safeEx,         safeEl,         gClose, 300, "5. Girar a zona Metal");
    setStep(_metalRoute.steps[5], dMet.base, dMet.extension, dMet.elevation, gClose, 350, "6. Posicionar descarga");
    setStep(_metalRoute.steps[6], dMet.base, dMet.extension, dMet.elevation, gOpen,  450, "7. Soltar pieza");
    saveRouteToNvs(true);

    // Secuencia fluida de 7 pasos generada automáticamente para NO-METAL:
    _nonMetalRoute.stepCount = 7;
    setStep(_nonMetalRoute.steps[0], pick.base, safeEx,         safeEl,         gOpen,  250, "1. Alinear sobre pieza");
    setStep(_nonMetalRoute.steps[1], pick.base, pick.extension, pick.elevation, gOpen,  350, "2. Bajar a pieza");
    setStep(_nonMetalRoute.steps[2], pick.base, pick.extension, pick.elevation, gClose, 500, "3. Cerrar pinza");
    setStep(_nonMetalRoute.steps[3], pick.base, safeEx,         safeEl,         gClose, 300, "4. Elevar pieza seguro");
    setStep(_nonMetalRoute.steps[4], dNom.base, safeEx,         safeEl,         gClose, 300, "5. Girar a No-Metal");
    setStep(_nonMetalRoute.steps[5], dNom.base, dNom.extension, dNom.elevation, gClose, 350, "6. Posicionar descarga");
    setStep(_nonMetalRoute.steps[6], dNom.base, dNom.extension, dNom.elevation, gOpen,  450, "7. Soltar pieza");
    saveRouteToNvs(false);
}

bool RobotArm::addCurrentPoseToRoute(bool isMetal, const char* label, uint16_t dwellMs) {
    ProgrammableRoute& route = isMetal ? _metalRoute : _nonMetalRoute;
    if (route.stepCount >= ServoCfg::MAX_ROUTE_STEPS) return false;

    RouteStep& st = route.steps[route.stepCount];
    st.pose    = getCurrentPose();
    st.dwellMs = constrain(dwellMs, 100, 10000);
    strncpy(st.label, label ? label : "Paso", sizeof(st.label) - 1);
    st.label[sizeof(st.label) - 1] = '\0';

    route.stepCount++;
    saveRouteToNvs(isMetal);
    return true;
}

bool RobotArm::updateRouteStepWithCurrentPose(bool isMetal, uint8_t stepIndex, uint16_t dwellMs) {
    ProgrammableRoute& route = isMetal ? _metalRoute : _nonMetalRoute;
    if (stepIndex >= route.stepCount) return false;

    route.steps[stepIndex].pose    = getCurrentPose();
    route.steps[stepIndex].dwellMs = constrain(dwellMs, 100, 10000);
    saveRouteToNvs(isMetal);
    return true;
}

bool RobotArm::editRouteStepFull(bool isMetal, uint8_t stepIndex, const ArmPose& pose, uint16_t dwellMs, const char* label) {
    ProgrammableRoute& route = isMetal ? _metalRoute : _nonMetalRoute;
    if (stepIndex >= route.stepCount) return false;

    RouteStep& st = route.steps[stepIndex];
    st.pose.base      = clampAngle(ServoCfg::BASE,      pose.base);
    st.pose.extension = clampAngle(ServoCfg::EXTENSION, pose.extension);
    st.pose.elevation = clampAngle(ServoCfg::ELEVATION, pose.elevation);
    st.pose.gripper   = clampAngle(ServoCfg::GRIPPER,   pose.gripper);
    st.dwellMs        = constrain(dwellMs, 100, 10000);
    if (label && strlen(label) > 0) {
        strncpy(st.label, label, sizeof(st.label) - 1);
        st.label[sizeof(st.label) - 1] = '\0';
    }
    saveRouteToNvs(isMetal);
    return true;
}

bool RobotArm::deleteRouteStep(bool isMetal, uint8_t stepIndex) {
    ProgrammableRoute& route = isMetal ? _metalRoute : _nonMetalRoute;
    if (stepIndex >= route.stepCount) return false;

    for (uint8_t i = stepIndex; i + 1 < route.stepCount; ++i) {
        route.steps[i] = route.steps[i + 1];
    }
    route.stepCount--;
    saveRouteToNvs(isMetal);
    return true;
}

bool RobotArm::moveRouteStepOrder(bool isMetal, uint8_t stepIndex, int8_t direction) {
    ProgrammableRoute& route = isMetal ? _metalRoute : _nonMetalRoute;
    int16_t targetIdx = (int16_t)stepIndex + direction;
    if (stepIndex >= route.stepCount || targetIdx < 0 || targetIdx >= route.stepCount) {
        return false;
    }
    RouteStep temp = route.steps[stepIndex];
    route.steps[stepIndex] = route.steps[targetIdx];
    route.steps[targetIdx] = temp;
    saveRouteToNvs(isMetal);
    return true;
}

void RobotArm::clearRoute(bool isMetal) {
    ProgrammableRoute& route = isMetal ? _metalRoute : _nonMetalRoute;
    route.stepCount = 0;
    saveRouteToNvs(isMetal);
}

void RobotArm::setNonMetalRouteEnabled(bool enabled) {
    _nonMetalRoute.enabled = enabled;
    saveRouteToNvs(false);
}

void RobotArm::setCalibration(float distCm, uint16_t verifyMs, uint16_t timeoutMs, bool inductiveActiveLow) {
    _calib.partDetectDistCm   = constrain(distCm, 2.0f, 80.0f);
    _calib.verifyDelayMs      = constrain(verifyMs, 150, 5000);
    _calib.maxMoveTimeoutMs   = constrain(timeoutMs, 1000, 15000);
    _calib.inductiveActiveLow = inductiveActiveLow;

    if (_prefs.begin("bot_cfg", false)) {
        _prefs.putBytes("calib", &_calib, sizeof(CalibrationConfig));
        _nvsSaveCount = _prefs.getUInt("save_cnt", 0) + 1;
        _prefs.putUInt("save_cnt", _nvsSaveCount);
        _prefs.end();
        _nvsHasSavedData = true;
    }
}

void RobotArm::saveRouteToNvs(bool isMetal) {
    if (_prefs.begin("bot_cfg", false)) {
        if (isMetal) {
            _prefs.putBytes("rt_metal", &_metalRoute, sizeof(ProgrammableRoute));
        } else {
            _prefs.putBytes("rt_nomet", &_nonMetalRoute, sizeof(ProgrammableRoute));
        }
        _nvsSaveCount = _prefs.getUInt("save_cnt", 0) + 1;
        _prefs.putUInt("save_cnt", _nvsSaveCount);
        _prefs.end();
        _nvsHasSavedData = true;
    }
}

void RobotArm::initDefaultRoutes() {
    for (uint8_t i = 0; i < ServoCfg::COUNT; ++i) {
        _limits.minAngle[i] = ServoCfg::DEFAULT_MIN_ANGLE; // 15°
        _limits.maxAngle[i] = ServoCfg::DEFAULT_MAX_ANGLE; // 165°
    }

    _homePose = DefaultPoses::HOME;

    _waypoints.pickPoint           = { 90.0f, 125.0f,  50.0f, 65.0f };
    _waypoints.dropMetalPoint      = { 30.0f, 110.0f,  85.0f, 65.0f };
    _waypoints.dropNonMetalPoint   = { 150.0f, 110.0f, 85.0f, 65.0f };
    _waypoints.gripperOpenAngle    = 65.0f;
    _waypoints.gripperCloseAngle   = 95.0f;
    _waypoints.safeTravelElevation = 115.0f;
    _waypoints.safeTravelExtension = 75.0f;

    _calib.partDetectDistCm   = 12.0f;
    _calib.verifyDelayMs      = Timing::DEFAULT_VERIFY_DELAY_MS;
    _calib.maxMoveTimeoutMs   = Timing::DEFAULT_MOVE_TIMEOUT_MS;
    _calib.inductiveActiveLow = true; // Activo en LOW (con jumpers del PC817 retirados)

    _metalRoute.enabled    = true;
    _nonMetalRoute.enabled = true; // Activado siempre para clasificar piezas No Metálicas detectadas por HC-SR04

    // Construir pasos por defecto SOLO en RAM (sin sobrescribir la memoria NVS existente al encender)
    auto setStep = [this](RouteStep& step, float b, float ex, float el, float gr, uint16_t dwell, const char* lbl) {
        step.pose.base      = clampAngle(ServoCfg::BASE,      b);
        step.pose.extension = clampAngle(ServoCfg::EXTENSION, ex);
        step.pose.elevation = clampAngle(ServoCfg::ELEVATION, el);
        step.pose.gripper   = clampAngle(ServoCfg::GRIPPER,   gr);
        step.dwellMs        = dwell;
        strncpy(step.label, lbl, sizeof(step.label) - 1);
        step.label[sizeof(step.label) - 1] = '\0';
    };

    const float gOpen   = _waypoints.gripperOpenAngle;
    const float gClose  = _waypoints.gripperCloseAngle;
    const float safeEl  = _waypoints.safeTravelElevation;
    const float safeEx  = _waypoints.safeTravelExtension;
    const ArmPose& pick = _waypoints.pickPoint;
    const ArmPose& dMet = _waypoints.dropMetalPoint;
    const ArmPose& dNom = _waypoints.dropNonMetalPoint;

    _metalRoute.stepCount = 7;
    setStep(_metalRoute.steps[0], pick.base, safeEx,         safeEl,         gOpen,  250, "1. Alinear sobre pieza");
    setStep(_metalRoute.steps[1], pick.base, pick.extension, pick.elevation, gOpen,  350, "2. Bajar a pieza");
    setStep(_metalRoute.steps[2], pick.base, pick.extension, pick.elevation, gClose, 500, "3. Cerrar pinza");
    setStep(_metalRoute.steps[3], pick.base, safeEx,         safeEl,         gClose, 300, "4. Elevar pieza seguro");
    setStep(_metalRoute.steps[4], dMet.base, safeEx,         safeEl,         gClose, 300, "5. Girar a zona Metal");
    setStep(_metalRoute.steps[5], dMet.base, dMet.extension, dMet.elevation, gClose, 350, "6. Posicionar descarga");
    setStep(_metalRoute.steps[6], dMet.base, dMet.extension, dMet.elevation, gOpen,  450, "7. Soltar pieza");

    _nonMetalRoute.stepCount = 7;
    setStep(_nonMetalRoute.steps[0], pick.base, safeEx,         safeEl,         gOpen,  250, "1. Alinear sobre pieza");
    setStep(_nonMetalRoute.steps[1], pick.base, pick.extension, pick.elevation, gOpen,  350, "2. Bajar a pieza");
    setStep(_nonMetalRoute.steps[2], pick.base, pick.extension, pick.elevation, gClose, 500, "3. Cerrar pinza");
    setStep(_nonMetalRoute.steps[3], pick.base, safeEx,         safeEl,         gClose, 300, "4. Elevar pieza seguro");
    setStep(_nonMetalRoute.steps[4], dNom.base, safeEx,         safeEl,         gClose, 300, "5. Girar a No-Metal");
    setStep(_nonMetalRoute.steps[5], dNom.base, dNom.extension, dNom.elevation, gClose, 350, "6. Posicionar descarga");
    setStep(_nonMetalRoute.steps[6], dNom.base, dNom.extension, dNom.elevation, gOpen,  450, "7. Soltar pieza");
}

void RobotArm::loadAllFromNvs() {
    // 1. Cargar valores por defecto en RAM (sin tocar NVS)
    initDefaultRoutes();

    // 2. Abrir espacio de nombres "bot_cfg" en modo lectura/escritura para garantizar que exista en Flash
    if (_prefs.begin("bot_cfg", false)) {
        _nvsSaveCount = _prefs.getUInt("save_cnt", 0);
        _nvsHasSavedData = (_nvsSaveCount > 0);

        if (_prefs.getBytesLength("limits") == sizeof(JointLimitsConfig)) {
            _prefs.getBytes("limits", &_limits, sizeof(JointLimitsConfig));
            _nvsHasSavedData = true;
        }
        if (_prefs.getBytesLength("home") == sizeof(ArmPose)) {
            _prefs.getBytes("home", &_homePose, sizeof(ArmPose));
            _nvsHasSavedData = true;
        }
        if (_prefs.getBytesLength("waypts") == sizeof(ReferenceWaypoints)) {
            _prefs.getBytes("waypts", &_waypoints, sizeof(ReferenceWaypoints));
            _nvsHasSavedData = true;
        }
        if (_prefs.getBytesLength("calib") == sizeof(CalibrationConfig)) {
            _prefs.getBytes("calib", &_calib, sizeof(CalibrationConfig));
            _nvsHasSavedData = true;
        }
        if (_prefs.getBytesLength("rt_metal") == sizeof(ProgrammableRoute)) {
            _prefs.getBytes("rt_metal", &_metalRoute, sizeof(ProgrammableRoute));
            _nvsHasSavedData = true;
        }
        if (_prefs.getBytesLength("rt_nomet") == sizeof(ProgrammableRoute)) {
            _prefs.getBytes("rt_nomet", &_nonMetalRoute, sizeof(ProgrammableRoute));
            _nonMetalRoute.enabled = true;
            _nvsHasSavedData = true;
        }
        _prefs.end();
    }
}

void RobotArm::resetAllToFactoryDefaults() {
    if (_prefs.begin("bot_cfg", false)) {
        _prefs.clear();
        _prefs.end();
    }
    initDefaultRoutes();
}
