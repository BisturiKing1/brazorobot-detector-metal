#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Preferences.h>
#include "config.h"

class RobotArm {
public:
    RobotArm();

    bool begin();
    void update(float dtSeconds, bool limitLeftActive, bool limitRightActive);

    // Control de poses y articulaciones
    bool setTargetPose(const ArmPose& pose);
    bool setJointTarget(ServoCfg::Joint joint, float angleDeg, bool limitLeftActive = false, bool limitRightActive = false);
    void setSpeedDps(float speedDps);
    float getSpeedDps() const { return _speedDps; }

    // Estado de movimiento y monitoreo de atasco
    bool isMoving() const;
    uint32_t getCurrentMoveElapsedMs(uint32_t nowMs) const;
    ArmPose getCurrentPose() const;
    ArmPose getTargetPose() const;
    float getCurrentAngle(ServoCfg::Joint joint) const { return _currentAngle[joint]; }
    bool isPcaConnected() const { return _pcaOnline; }
    bool isSafetyLocked() const { return _safetyLock; }
    bool wasBlockedByLimitSwitch() const { return _blockedByLimit; }

    // Seguridad y Paros de Emergencia (Nunca reanuda automáticamente sin orden explícita)
    void lockSafetyStop();
    void unlockSafetyStop();

    // Configuración de Límites de Articulaciones (Inicialmente 15° a 165°)
    const JointLimitsConfig& getLimits() const { return _limits; }
    void setJointLimits(uint8_t joint, float minDeg, float maxDeg);
    void saveLimitsToNvs();

    // Posición inicial segura (HOME) y Rutas Programables (Metal y No Metal)
    const ArmPose& getHomePose() const { return _homePose; }
    void saveCurrentAsHome();

    const ProgrammableRoute& getMetalRoute() const { return _metalRoute; }
    const ProgrammableRoute& getNonMetalRoute() const { return _nonMetalRoute; }

    bool addCurrentPoseToRoute(bool isMetal, const char* label, uint16_t dwellMs);
    bool updateRouteStepWithCurrentPose(bool isMetal, uint8_t stepIndex, uint16_t dwellMs);
    bool editRouteStepFull(bool isMetal, uint8_t stepIndex, const ArmPose& pose, uint16_t dwellMs, const char* label);
    bool deleteRouteStep(bool isMetal, uint8_t stepIndex);
    bool moveRouteStepOrder(bool isMetal, uint8_t stepIndex, int8_t direction);
    void clearRoute(bool isMetal);
    void setNonMetalRouteEnabled(bool enabled);

    // Parámetros de calibración experimental (HC-SR04, lógica inductiva y tiempos)
    const CalibrationConfig& getCalibration() const { return _calib; }
    void setCalibration(float distCm, uint16_t verifyMs, uint16_t timeoutMs, bool inductiveActiveLow);

    // Asistente por Puntos de Referencia (Generación automática de trayectoria fluida)
    const ReferenceWaypoints& getWaypoints() const { return _waypoints; }
    void captureWaypoint(const String& which);
    void rebuildRoutesFromWaypoints();

    bool hasSavedNvsData() const { return _nvsHasSavedData; }
    uint32_t getNvsSaveCount() const { return _nvsSaveCount; }

    void resetAllToFactoryDefaults();

private:
    Adafruit_PWMServoDriver _pca;
    Preferences _prefs;
    bool     _pcaOnline        = false;
    bool     _safetyLock       = false;
    bool     _blockedByLimit   = false;
    bool     _nvsHasSavedData  = false;
    uint32_t _nvsSaveCount     = 0;

    float    _currentAngle[ServoCfg::COUNT];
    float    _targetAngle[ServoCfg::COUNT];
    float    _speedDps          = ServoCfg::DEFAULT_SPEED_DPS;
    uint32_t _moveStartMs       = 0;
    bool     _wasMoving         = false;
    uint32_t _jointIdleMs[ServoCfg::COUNT]     = { 0, 0, 0, 0 };
    bool     _jointDetached[ServoCfg::COUNT]   = { false, false, false, false };

    JointLimitsConfig  _limits;
    ArmPose            _homePose;
    ReferenceWaypoints _waypoints;
    ProgrammableRoute  _metalRoute;
    ProgrammableRoute  _nonMetalRoute;
    CalibrationConfig  _calib;

    void loadAllFromNvs();
    void initDefaultRoutes();
    void saveRouteToNvs(bool isMetal);
    void writeServoAngle(uint8_t channel, float angleDeg);
    float clampAngle(uint8_t channel, float angleDeg) const;
};
