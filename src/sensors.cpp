#include "sensors.h"

void SensorManager::begin() {
    pinMode(Pins::IND_SENSOR_1,     INPUT_PULLUP);
    pinMode(Pins::IND_SENSOR_2,     INPUT_PULLUP);
    pinMode(Pins::LIMIT_BASE_LEFT,  INPUT_PULLUP);
    pinMode(Pins::LIMIT_BASE_RIGHT, INPUT_PULLUP);

    pinMode(Pins::ULTRA_TRIG, OUTPUT);
    digitalWrite(Pins::ULTRA_TRIG, LOW);
    pinMode(Pins::ULTRA_ECHO, INPUT);

    uint32_t now = millis();
    _ind1.stableState = _ind1.lastRawState = (digitalRead(Pins::IND_SENSOR_1) == LOW);
    _ind2.stableState = _ind2.lastRawState = (digitalRead(Pins::IND_SENSOR_2) == LOW);
    _limL.stableState = _limL.lastRawState = (digitalRead(Pins::LIMIT_BASE_LEFT) == LOW);
    _limR.stableState = _limR.lastRawState = (digitalRead(Pins::LIMIT_BASE_RIGHT) == LOW);

    _ind1.lastChangeMs = _ind2.lastChangeMs = _limL.lastChangeMs = _limR.lastChangeMs = now;
    _state.distanceCm  = 999.0f;
    _state.partPresent = false;
}

void SensorManager::updatePin(DebouncedPin& dp, uint32_t nowMs, bool activeLow) {
    dp.roseEdge = false;
    int rawLevel = digitalRead(dp.pin);
    bool rawActive = activeLow ? (rawLevel == LOW) : (rawLevel == HIGH);

    if (rawActive != dp.lastRawState) {
        dp.lastChangeMs = nowMs;
        dp.lastRawState = rawActive;
    }

    if ((nowMs - dp.lastChangeMs) >= dp.debounceMs) {
        if (rawActive != dp.stableState) {
            dp.stableState = rawActive;
            if (dp.stableState) {
                dp.roseEdge = true;
            }
        }
    }
}

float SensorManager::readUltrasonicCm() {
    digitalWrite(Pins::ULTRA_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(Pins::ULTRA_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(Pins::ULTRA_TRIG, LOW);

    // Timeout corto de 12 ms (~200 cm máx) para no bloquear el bucle de tiempo real
    unsigned long durationUs = pulseIn(Pins::ULTRA_ECHO, HIGH, 12000UL);
    if (durationUs == 0) {
        return 999.0f;
    }
    float cm = (durationUs * 0.0343f) * 0.5f;
    if (cm < 1.5f || cm > 200.0f) {
        return 999.0f;
    }
    return cm;
}

void SensorManager::update(float partThresholdCm, bool inductiveActiveLow) {
    // Ajustar dinámicamente Pull-Up o Pull-Down según el modo del optoacoplador PC817
    static bool lastModeLow = true;
    if (inductiveActiveLow != lastModeLow) {
        pinMode(Pins::IND_SENSOR_1, inductiveActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
        pinMode(Pins::IND_SENSOR_2, inductiveActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
        lastModeLow = inductiveActiveLow;
    }

    uint32_t now = millis();
    updatePin(_ind1, now, inductiveActiveLow);
    updatePin(_ind2, now, inductiveActiveLow);
    updatePin(_limL, now, true);
    updatePin(_limR, now, true);

    if (now - _lastUltraMs >= Timing::ULTRASONIC_PERIOD_MS) {
        _lastUltraMs = now;
        float rawCm = readUltrasonicCm();

        if (rawCm < 200.0f) {
            if (_filteredDistCm > 200.0f) {
                _filteredDistCm = rawCm;
            } else {
                // Filtro exponencial suave para evitar lecturas erráticas aisladas
                _filteredDistCm = (_filteredDistCm * 0.65f) + (rawCm * 0.35f);
            }
        } else {
            _filteredDistCm = 999.0f;
        }

        // Confirmar presencia con histéresis de lecturas consecutivas
        if (_filteredDistCm <= partThresholdCm) {
            if (_presentHoldCount < 3) _presentHoldCount++;
        } else if (_filteredDistCm > (partThresholdCm + 1.5f)) {
            if (_presentHoldCount > 0) _presentHoldCount--;
        }

        _state.distanceCm  = _filteredDistCm;
        _state.partPresent = (_presentHoldCount >= 2);
    }

    _state.inductive1          = _ind1.stableState;
    _state.inductive2          = _ind2.stableState;
    _state.anyMetalDetected    = _ind1.stableState || _ind2.stableState;
    _state.limitLeft           = _limL.stableState;
    _state.limitRight          = _limR.stableState;

    _state.inductive1Triggered = _ind1.roseEdge;
    _state.inductive2Triggered = _ind2.roseEdge;
    _state.limitLeftTriggered  = _limL.roseEdge;
    _state.limitRightTriggered = _limR.roseEdge;
}
