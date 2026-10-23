#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "robot_arm.h"
#include "sensors.h"
#include "state_machine.h"

class CaptivePortalServer {
public:
    CaptivePortalServer(RobotArm& arm, SensorManager& sensors, RobotStateMachine& fsm, SemaphoreHandle_t mutex);

    void begin();
    void processDns();
    void broadcastTelemetry();
    void broadcastConfig();

private:
    RobotArm&          _arm;
    SensorManager&     _sensors;
    RobotStateMachine& _fsm;
    SemaphoreHandle_t  _mutex;

    DNSServer      _dnsServer;
    AsyncWebServer _server;
    AsyncWebSocket _ws;

    void setupCaptiveRoutes();
    void handleWsMessage(void* arg, uint8_t* data, size_t len);
    String buildTelemetryJson();
    String buildConfigJson();
};
