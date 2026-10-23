#pragma once
#include <Arduino.h>
#include "config.h"

struct SensorState {
    bool  inductive1;         // GPIO 7 (LOW = Metal)
    bool  inductive2;         // GPIO 4 (LOW = Metal)
    bool  anyMetalDetected;   // OR lógico de ambos sensores inductivos
    bool  limitLeft;          // GPIO 5 (LOW = Tope Izquierdo)
    bool  limitRight;         // GPIO 6 (LOW = Tope Derecho)

    float distanceCm;         // Distancia filtrada del HC-SR04 en cm (999.0 si fuera de rango)
    bool  partPresent;        // true cuando distanceCm <= umbral calibrado de presencia

    // Flancos de subida
    bool  inductive1Triggered;
    bool  inductive2Triggered;
    bool  limitLeftTriggered;
    bool  limitRightTriggered;
};

class SensorManager {
public:
    void begin();
    void update(float partThresholdCm, bool inductiveActiveLow = true);
    const SensorState& getState() const { return _state; }

private:
    struct DebouncedPin {
        uint8_t  pin;
        uint32_t debounceMs;
        bool     stableState;
        bool     lastRawState;
        uint32_t lastChangeMs;
        bool     roseEdge;
    };

    DebouncedPin _ind1 { Pins::IND_SENSOR_1,     Timing::DEBOUNCE_INDUCT_MS, false, false, 0, false };
    DebouncedPin _ind2 { Pins::IND_SENSOR_2,     Timing::DEBOUNCE_INDUCT_MS, false, false, 0, false };
    DebouncedPin _limL { Pins::LIMIT_BASE_LEFT,  Timing::DEBOUNCE_LIMIT_MS,  false, false, 0, false };
    DebouncedPin _limR { Pins::LIMIT_BASE_RIGHT, Timing::DEBOUNCE_LIMIT_MS,  false, false, 0, false };

    SensorState _state {};
    uint32_t    _lastUltraMs = 0;
    float       _filteredDistCm = 999.0f;
    uint8_t     _presentHoldCount = 0;

    void updatePin(DebouncedPin& dp, uint32_t nowMs, bool activeLow = true);
    float readUltrasonicCm();
};
