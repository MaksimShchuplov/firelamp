#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "globals.h"
#include "net_helpers.h"
#include "mqtt.h"

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

static String mqIp;
static int mqPt = 1883;
static String mqU;
static String mqP;
static String mqT = "firelamp";
static unsigned long lastReconnectAttempt = 0;

void mqttPublishState() {
    if (!mqttClient.connected() || mqT.length() == 0) return;
    char j[128];
    snprintf(j, sizeof(j), "{\"state\":\"%s\",\"b\":%d,\"c\":%d,\"co\":%d,\"sp\":%d,\"bl\":%d,\"th\":%d}",
             uiBright > 0 ? "ON" : "OFF",
             (int)uiBright, (int)uiContrast, (int)uiCooling, (int)uiSparking, (int)uiBlend, (int)uiTheme);
    String topic = mqT + "/state";
    mqttClient.publish(topic.c_str(), j);
}

void initMqtt() {
    Preferences p;
    p.begin("mqtt", true);
    mqIp = p.getString("ip", "");
    mqPt = p.getInt("pt", 1883);
    mqU = p.getString("u", "");
    mqP = p.getString("p", "");
    mqT = p.getString("t", "firelamp");
    p.end();

    if (mqIp.length() > 0) {
        // HA discovery config (~700 B) exceeds PubSubClient's 256 B default buffer.
        mqttClient.setBufferSize(1024);
        mqttClient.setServer(mqIp.c_str(), mqPt);
        mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, payload, length);
            if (!err) {
                bool changed = false;
                static uint8_t last_b = BRIGHT_DEFAULT;

                if (doc.containsKey("state")) {
                    String st = doc["state"];
                    if (st == "OFF" && uiBright > 0) {
                        last_b = uiBright;
                        setBright(0);
                        changed = true;
                    } else if (st == "ON" && uiBright == 0) {
                        setBright(last_b > 0 ? last_b : BRIGHT_DEFAULT);
                        changed = true;
                    }
                }

                if (doc.containsKey("b"))  {
                    int v = doc["b"];
                    setBright(constrain(v, BRIGHT_MIN, BRIGHT_MAX));
                    if (v > 0) last_b = v; // remember last non-zero brightness
                    changed = true;
                }
                if (doc.containsKey("c"))  { uiContrast = constrain((int)doc["c"], CONTRAST_MIN, CONTRAST_MAX); changed = true; }
                if (doc.containsKey("co")) { uiCooling = constrain((int)doc["co"], COOLING_MIN, COOLING_MAX); recalcCooling(); changed = true; }
                if (doc.containsKey("sp")) { uiSparking = constrain((int)doc["sp"], SPARKING_MIN, SPARKING_MAX); changed = true; }
                if (doc.containsKey("bl")) { uiBlend = constrain((int)doc["bl"], BLEND_MIN, BLEND_MAX); changed = true; }
                if (doc.containsKey("th")) { uiTheme = constrain((int)doc["th"], 0, THEME_COUNT - 1); changed = true; }
                
                if (changed) {
                    buildHeatPalette();
                    updatePowerCalc();
                    markDirty();
                    mqttPublishState();
                }
            }
        });
    }
}

// Home Assistant MQTT Discovery: a retained config message makes the lamp
// appear in HA automatically (Settings → Devices) — no configuration.yaml.
// Retained delivery also covers HA restarts without a birth-message listener.
static void publishDiscovery() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char uid[20];
    snprintf(uid, sizeof(uid), "firelamp_%02x%02x%02x", mac[3], mac[4], mac[5]);
    char topic[64];
    snprintf(topic, sizeof(topic), "homeassistant/light/%s/config", uid);
    char payload[896];
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Fire Lamp\",\"uniq_id\":\"%s\","
        "\"stat_t\":\"%s/state\",\"cmd_t\":\"%s/set\","
        "\"stat_val_tpl\":\"{{ value_json.state }}\","
        "\"pl_on\":\"{\\\"state\\\":\\\"ON\\\"}\",\"pl_off\":\"{\\\"state\\\":\\\"OFF\\\"}\","
        "\"bri_stat_t\":\"%s/state\",\"bri_val_tpl\":\"{{ value_json.b }}\","
        "\"bri_cmd_t\":\"%s/set\",\"bri_cmd_tpl\":\"{\\\"b\\\":{{ value }}}\",\"bri_scl\":100,"
        "\"dev\":{\"ids\":[\"%s\"],\"name\":\"Fire Lamp\",\"mdl\":\"FireLamp ESP32-S3\","
        "\"sw\":\"" FIRMWARE_VERSION "\"}}",
        uid, mqT.c_str(), mqT.c_str(), mqT.c_str(), mqT.c_str(), uid);
    mqttClient.publish(topic, payload, true);
}

void serviceMqtt() {
    if (mqIp.length() == 0 || WiFi.status() != WL_CONNECTED) return;
    
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            char clientId[24];
            snprintf(clientId, sizeof(clientId), "firelamp-%04x", (unsigned)random(0xffff));
            bool connected = false;

            if (mqU.length() > 0) {
                connected = mqttClient.connect(clientId, mqU.c_str(), mqP.c_str());
            } else {
                connected = mqttClient.connect(clientId);
            }
            
            if (connected) {
                LOG_INFO("MQTT connected to %s:%d", mqIp.c_str(), mqPt);
                String sub = mqT + "/set";
                mqttClient.subscribe(sub.c_str());
                publishDiscovery();
                mqttPublishState();
            } else {
                LOG_WARN("MQTT connect failed, rc=%d", mqttClient.state());
            }
        }
    } else {
        mqttClient.loop();
    }
}

static void handleSetMqtt() {
    if (!isWebRequest()) return;

    String ip = server.arg("ip");
    String u  = server.arg("u");
    String pw = server.arg("p");
    String t  = server.arg("t");
    if (t.length() == 0) t = "firelamp";

    int pt = server.arg("pt").toInt();
    if (pt <= 0 || pt > 65535) pt = 1883;

    // Clamp strings to sane lengths to avoid NVS exhaustion
    if (ip.length() > 64) ip = ip.substring(0, 64);
    if (u.length()  > 64) u  = u.substring(0, 64);
    if (pw.length() > 64) pw = pw.substring(0, 64);
    if (t.length()  > 64) t  = t.substring(0, 64);

    Preferences p;
    p.begin("mqtt", false);
    p.putString("ip", ip);
    p.putInt("pt", pt);
    p.putString("u", u);
    if (pw == "-")        p.remove("p");          // "-" is the explicit "clear password" sentinel
    else if (pw.length() > 0) p.putString("p", pw);
    p.putString("t", t);
    p.end();

    server.send(200, "application/json", "{\"ok\":true}");
    mqttClient.disconnect();
    initMqtt();
}

static void handleGetMqtt() {
    Preferences p;
    p.begin("mqtt", true);
    String ip    = p.getString("ip", "");
    int    pt    = p.getInt("pt", 1883);
    String u     = p.getString("u", "");
    bool   hasPw = p.getString("p", "").length() > 0;
    String t     = p.getString("t", "firelamp");
    p.end();

    // Password is write-only: return only whether it's set, not the value.
    // ip/u/t are <=64 raw; jsonEscape can double each, so 512 covers the worst case.
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"ip\":\"%s\",\"pt\":%d,\"u\":\"%s\",\"p_set\":%s,\"t\":\"%s\"}",
             jsonEscape(ip).c_str(), pt, jsonEscape(u).c_str(), hasPw ? "true" : "false", jsonEscape(t).c_str());
    server.send(200, "application/json", buf);
}

void registerMqttHandlers() {
    server.on("/setmqtt", HTTP_POST, handleSetMqtt);
    server.on("/getmqtt", HTTP_GET, handleGetMqtt);
}
