#include "captive_portal.h"
#include "web_ui.h"

CaptivePortalServer::CaptivePortalServer(
    RobotArm& arm,
    SensorManager& sensors,
    RobotStateMachine& fsm,
    SemaphoreHandle_t mutex
)
    : _arm(arm),
      _sensors(sensors),
      _fsm(fsm),
      _mutex(mutex),
      _server(80),
      _ws("/ws") {}

void CaptivePortalServer::begin() {
    WiFi.mode(WIFI_AP);
    IPAddress localIp(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(localIp, gateway, subnet);

    if (strlen(NetCfg::AP_PASS) >= 8) {
        WiFi.softAP(NetCfg::AP_SSID, NetCfg::AP_PASS, NetCfg::AP_CHANNEL, 0, NetCfg::MAX_CLIENTS);
    } else {
        WiFi.softAP(NetCfg::AP_SSID, nullptr, NetCfg::AP_CHANNEL, 0, NetCfg::MAX_CLIENTS);
    }

    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer.start(53, "*", localIp);

    _ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            client->text(buildConfigJson());
            client->text(buildTelemetryJson());
        } else if (type == WS_EVT_DATA) {
            handleWsMessage(arg, data, len);
        }
    });

    _server.addHandler(&_ws);
    setupCaptiveRoutes();
    _server.begin();
}

void CaptivePortalServer::processDns() {
    _dnsServer.processNextRequest();
    _ws.cleanupClients();
}

void CaptivePortalServer::setupCaptiveRoutes() {
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", WEB_UI_HTML);
    });

    const char* captivePaths[] = {
        "/generate_204",
        "/gen_204",
        "/hotspot-detect.html",
        "/library/test/success.html",
        "/connecttest.txt",
        "/ncsi.txt",
        "/redirect",
        "/canonical.html"
    };

    for (const char* path : captivePaths) {
        _server.on(path, HTTP_GET, [](AsyncWebServerRequest* request) {
            request->redirect("http://192.168.4.1/");
        });
    }

    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "application/json", buildTelemetryJson());
    });

    _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "application/json", buildConfigJson());
    });

    _server.onNotFound([](AsyncWebServerRequest* request) {
        request->redirect("http://192.168.4.1/");
    });
}

void CaptivePortalServer::handleWsMessage(void* arg, uint8_t* data, size_t len) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) {
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return;

    String cmd = doc["cmd"] | "";
    bool configChanged = false;

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(35)) == pdTRUE) {
        const SensorState& s = _sensors.getState();

        if (cmd == "set_mode") {
            String mode = doc["mode"] | "auto";
            if (mode == "auto") {
                _fsm.requestModeChange(SystemMode::AUTO);
            } else if (mode == "manual") {
                _fsm.requestModeChange(SystemMode::MANUAL);
            }
        } else if (cmd == "move_joint") {
            // Si el usuario mueve un slider y no hay falla de seguridad, pasar a Modo Manual automáticamente
            if (_fsm.getTelemetry().mode != SystemMode::MANUAL && !_fsm.getTelemetry().safetyConditionActive) {
                _fsm.requestModeChange(SystemMode::MANUAL);
            }
            if (_fsm.isManualMovementAllowed()) {
                uint8_t joint = doc["joint"] | 0;
                float angle   = doc["angle"] | 90.0f;
                if (joint < ServoCfg::COUNT) {
                    _arm.setJointTarget(static_cast<ServoCfg::Joint>(joint), angle, s.limitLeft, s.limitRight);
                }
            }
        } else if (cmd == "set_speed") {
            float spd = doc["speed"] | ServoCfg::DEFAULT_SPEED_DPS;
            _arm.setSpeedDps(spd);
        } else if (cmd == "goto_home") {
            if (_fsm.isManualMovementAllowed()) {
                _arm.setTargetPose(_arm.getHomePose());
            }
        } else if (cmd == "save_home") {
            if (_fsm.isManualMovementAllowed()) {
                _arm.saveCurrentAsHome();
                configChanged = true;
            }
        } else if (cmd == "capture_waypoint") {
            String which = doc["which"] | "";
            _arm.captureWaypoint(which);
            configChanged = true;
        } else if (cmd == "goto_waypoint") {
            if (_fsm.getTelemetry().mode != SystemMode::MANUAL) {
                _fsm.requestModeChange(SystemMode::MANUAL);
            }
            if (_fsm.isManualMovementAllowed()) {
                String which = doc["which"] | "";
                const ReferenceWaypoints& wp = _arm.getWaypoints();
                if (which == "pick")          _arm.setTargetPose(wp.pickPoint);
                if (which == "drop_metal")    _arm.setTargetPose(wp.dropMetalPoint);
                if (which == "drop_nonmetal") _arm.setTargetPose(wp.dropNonMetalPoint);
            }
        } else if (cmd == "save_limits") {
            JsonArray mins = doc["min"].as<JsonArray>();
            JsonArray maxs = doc["max"].as<JsonArray>();
            if (mins.size() == 4 && maxs.size() == 4) {
                for (uint8_t i = 0; i < 4; ++i) {
                    _arm.setJointLimits(i, mins[i].as<float>(), maxs[i].as<float>());
                }
                _arm.saveLimitsToNvs();
                configChanged = true;
            }
        } else if (cmd == "save_calib") {
            float distCm       = doc["distCm"] | 12.0f;
            uint16_t verifyMs  = doc["verifyMs"] | 700;
            uint16_t timeoutMs = doc["timeoutMs"] | 4500;
            bool indActiveLow  = doc["indActiveLow"] | true;
            _arm.setCalibration(distCm, verifyMs, timeoutMs, indActiveLow);
            configChanged = true;
        } else if (cmd == "add_route_step") {
            bool isMetal     = doc["isMetal"] | true;
            String label     = doc["label"] | "Paso";
            uint16_t dwellMs = doc["dwellMs"] | 400;
            _arm.addCurrentPoseToRoute(isMetal, label.c_str(), dwellMs);
            configChanged = true;
        } else if (cmd == "update_step") {
            bool isMetal     = doc["isMetal"] | true;
            uint8_t idx      = doc["index"] | 0;
            uint16_t dwellMs = doc["dwellMs"] | 400;
            _arm.updateRouteStepWithCurrentPose(isMetal, idx, dwellMs);
            configChanged = true;
        } else if (cmd == "edit_step_full") {
            bool isMetal     = doc["isMetal"] | true;
            uint8_t idx      = doc["index"] | 0;
            uint16_t dwellMs = doc["dwellMs"] | 400;
            String label     = doc["label"] | "Paso";
            ArmPose p {
                doc["b"]  | 90.0f,
                doc["ex"] | 70.0f,
                doc["el"] | 115.0f,
                doc["gr"] | 75.0f
            };
            _arm.editRouteStepFull(isMetal, idx, p, dwellMs, label.c_str());
            configChanged = true;
        } else if (cmd == "delete_step") {
            bool isMetal = doc["isMetal"] | true;
            uint8_t idx  = doc["index"] | 0;
            _arm.deleteRouteStep(isMetal, idx);
            configChanged = true;
        } else if (cmd == "move_step_order") {
            bool isMetal = doc["isMetal"] | true;
            uint8_t idx  = doc["index"] | 0;
            int8_t dir   = doc["dir"] | 0;
            _arm.moveRouteStepOrder(isMetal, idx, dir);
            configChanged = true;
        } else if (cmd == "goto_step") {
            if (_fsm.getTelemetry().mode != SystemMode::MANUAL) {
                _fsm.requestModeChange(SystemMode::MANUAL);
            }
            if (_fsm.isManualMovementAllowed()) {
                bool isMetal = doc["isMetal"] | true;
                uint8_t idx  = doc["index"] | 0;
                const ProgrammableRoute& rt = isMetal ? _arm.getMetalRoute() : _arm.getNonMetalRoute();
                if (idx < rt.stepCount) {
                    _arm.setTargetPose(rt.steps[idx].pose);
                }
            }
        } else if (cmd == "clear_route") {
            bool isMetal = doc["isMetal"] | true;
            _arm.clearRoute(isMetal);
            configChanged = true;
        } else if (cmd == "set_nonmetal_enabled") {
            bool enabled = doc["enabled"] | false;
            _arm.setNonMetalRouteEnabled(enabled);
            configChanged = true;
        } else if (cmd == "test_route") {
            bool isMetal = doc["isMetal"] | true;
            _fsm.triggerTestRoute(isMetal);
        } else if (cmd == "homing") {
            _fsm.startHoming();
        } else if (cmd == "estop") {
            _fsm.triggerEmergencyStop("Paro de emergencia activado desde Portal bot-detector");
        } else if (cmd == "ack_fault") {
            _fsm.acknowledgeAndClearFault();
        } else if (cmd == "reset_counters") {
            _fsm.resetCounters();
        } else if (cmd == "factory_reset") {
            _arm.resetAllToFactoryDefaults();
            configChanged = true;
        }

        xSemaphoreGive(_mutex);
    }

    if (configChanged) {
        broadcastConfig();
    }
}

String CaptivePortalServer::buildTelemetryJson() {
    JsonDocument doc;
    doc["type"] = "telemetry";

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        const SensorState& s       = _sensors.getState();
        const SystemTelemetry& tel = _fsm.getTelemetry();
        ArmPose pose               = _arm.getCurrentPose();

        const char* modeStr = "AUTO";
        if (tel.mode == SystemMode::MANUAL) modeStr = "MANUAL";
        else if (tel.mode == SystemMode::HOMING) modeStr = "HOMING";
        else if (tel.mode == SystemMode::FAULT_LOCKED) modeStr = "FALLA";

        doc["mode"]          = modeStr;
        doc["stateName"]     = tel.stateName;
        doc["status"]        = tel.statusText;
        doc["lastResult"]    = tel.lastResult;
        doc["safetyActive"]  = tel.safetyConditionActive;
        doc["manualAllowed"] = _fsm.isManualMovementAllowed();
        doc["pcaOnline"]     = _arm.isPcaConnected();

        doc["distCm"]      = s.distanceCm;
        doc["partPresent"] = s.partPresent;
        doc["metal"]       = s.anyMetalDetected;
        doc["ind1"]        = s.inductive1;
        doc["ind2"]        = s.inductive2;
        doc["limL"]        = s.limitLeft;
        doc["limR"]        = s.limitRight;

        doc["metalCnt"]    = tel.metalCount;
        doc["nonMetalCnt"] = tel.nonMetalCount;
        doc["faultCnt"]    = tel.faultCount;

        JsonArray angles = doc["angles"].to<JsonArray>();
        angles.add(pose.base);
        angles.add(pose.extension);
        angles.add(pose.elevation);
        angles.add(pose.gripper);

        xSemaphoreGive(_mutex);
    }

    doc["uptime"] = millis() / 1000;
    doc["heap"]   = ESP.getFreeHeap();

    String output;
    serializeJson(doc, output);
    return output;
}

String CaptivePortalServer::buildConfigJson() {
    JsonDocument doc;
    doc["type"] = "config";

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(25)) == pdTRUE) {
        doc["nvsSaved"] = _arm.hasSavedNvsData();
        doc["nvsCount"] = _arm.getNvsSaveCount();

        const JointLimitsConfig& lim = _arm.getLimits();
        JsonObject limObj = doc["limits"].to<JsonObject>();
        JsonArray minArr  = limObj["min"].to<JsonArray>();
        JsonArray maxArr  = limObj["max"].to<JsonArray>();
        for (uint8_t i = 0; i < 4; ++i) {
            minArr.add(lim.minAngle[i]);
            maxArr.add(lim.maxAngle[i]);
        }

        const CalibrationConfig& cal = _arm.getCalibration();
        JsonObject calObj = doc["calib"].to<JsonObject>();
        calObj["distCm"]       = cal.partDetectDistCm;
        calObj["verifyMs"]     = cal.verifyDelayMs;
        calObj["timeoutMs"]    = cal.maxMoveTimeoutMs;
        calObj["indActiveLow"] = cal.inductiveActiveLow;

        const ReferenceWaypoints& wp = _arm.getWaypoints();
        JsonObject wpObj = doc["waypoints"].to<JsonObject>();
        wpObj["gripOpen"]  = wp.gripperOpenAngle;
        wpObj["gripClose"] = wp.gripperCloseAngle;
        wpObj["safeEl"]    = wp.safeTravelElevation;
        wpObj["safeEx"]    = wp.safeTravelExtension;
        auto addPoseArr = [](JsonObject parent, const char* key, const ArmPose& p) {
            JsonArray a = parent[key].to<JsonArray>();
            a.add(p.base); a.add(p.extension); a.add(p.elevation); a.add(p.gripper);
        };
        addPoseArr(wpObj, "home",        _arm.getHomePose());
        addPoseArr(wpObj, "pick",        wp.pickPoint);
        addPoseArr(wpObj, "dropMetal",   wp.dropMetalPoint);
        addPoseArr(wpObj, "dropNonMetal",wp.dropNonMetalPoint);

        auto serializeRoute = [](JsonObject target, const ProgrammableRoute& rt) {
            target["enabled"] = rt.enabled;
            JsonArray arr = target["steps"].to<JsonArray>();
            for (uint8_t i = 0; i < rt.stepCount; ++i) {
                JsonObject st = arr.add<JsonObject>();
                st["label"] = rt.steps[i].label;
                st["dwell"] = rt.steps[i].dwellMs;
                JsonArray p = st["pose"].to<JsonArray>();
                p.add(rt.steps[i].pose.base);
                p.add(rt.steps[i].pose.extension);
                p.add(rt.steps[i].pose.elevation);
                p.add(rt.steps[i].pose.gripper);
            }
        };

        serializeRoute(doc["metalRoute"].to<JsonObject>(),    _arm.getMetalRoute());
        serializeRoute(doc["nonMetalRoute"].to<JsonObject>(), _arm.getNonMetalRoute());

        xSemaphoreGive(_mutex);
    }

    String output;
    serializeJson(doc, output);
    return output;
}

void CaptivePortalServer::broadcastTelemetry() {
    if (_ws.count() == 0) return;
    _ws.textAll(buildTelemetryJson());
}

void CaptivePortalServer::broadcastConfig() {
    if (_ws.count() == 0) return;
    _ws.textAll(buildConfigJson());
}
