#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "globals.h"
#include "net_helpers.h"
#include "mqtt.h"
#include "mqtt_state.h"
#include "params.h"

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
        // connect() blocks Core 1 (web server) while waiting for CONNACK; the
        // 15 s PubSubClient default freezes the UI on every 5 s reconnect retry
        // when the broker is configured but unresponsive.
        mqttClient.setSocketTimeout(5);
        // /surprise blocks Core 1 — and therefore mqttClient.loop(), the only
        // PINGREQ source — for up to GEMINI_TIMEOUT_MS. PubSubClient's 15 s
        // default would let the broker drop the session at 1.5x = 22.5 s and
        // discard any firelamp/set command still in the socket buffer.
        mqttClient.setKeepAlive(60);
        mqttClient.setServer(mqIp.c_str(), mqPt);
        mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
            JsonDocument doc;
            // HA's default light schema publishes payload_on/payload_off verbatim
            // to cmd_t, so the bare ON/OFF form must be accepted alongside the
            // JSON payloads that hand-written automations send.
            if      (length == 2 && !memcmp(payload, "ON",  2)) doc["state"] = "ON";
            else if (length == 3 && !memcmp(payload, "OFF", 3)) doc["state"] = "OFF";
            else if (deserializeJson(doc, payload, length))     return;

            bool hadB     = doc.containsKey("b");
            bool hadState = doc.containsKey("state");
            static uint8_t last_b = BRIGHT_DEFAULT;

            // Brightness is the registry's autoJson=false parameter: it goes
            // through the ON/OFF state machine and setBright()'s gamma path here,
            // not applyJsonParams().
            MqttBrightDelta bd = mqttResolveBright(
                hadB, hadState,
                hadState ? (const char *)doc["state"] : nullptr,
                hadB ? (int)doc["b"] : 0,
                (uint8_t)uiBright.load(std::memory_order_relaxed), last_b);
            if (bd.bright >= 0) setBright(bd.bright);
            last_b = bd.last_b;

            bool changed = (hadB || hadState);
            changed |= applyJsonParams(doc);   // c/co/sp/bl/th + their side-effects

            if (changed) {
                updatePowerCalc();
                markDirty();
                mqttPublishState();
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
    // jsonEscape prevents a malicious topic prefix from injecting into the JSON
    // discovery payload published to the Home Assistant broker.
    String te = jsonEscape(mqT);
    const char *t = te.c_str();
    char payload[896];
    snprintf(payload, sizeof(payload),
        "{\"name\":\"Fire Lamp\",\"uniq_id\":\"%s\","
        "\"stat_t\":\"%s/state\",\"cmd_t\":\"%s/set\","
        "\"stat_val_tpl\":\"{{ value_json.state }}\","
        // pl_on/pl_off are used in BOTH directions by HA's default light schema:
        // published to cmd_t, and string-compared against the templated state
        // payload. They must therefore match what stat_val_tpl yields (ON/OFF),
        // or the entity never leaves "unknown".
        "\"pl_on\":\"ON\",\"pl_off\":\"OFF\","
        "\"bri_stat_t\":\"%s/state\",\"bri_val_tpl\":\"{{ value_json.b }}\","
        "\"bri_cmd_t\":\"%s/set\",\"bri_cmd_tpl\":\"{\\\"b\\\":{{ value }}}\",\"bri_scl\":100,"
        "\"dev\":{\"ids\":[\"%s\"],\"name\":\"Fire Lamp\",\"mdl\":\"FireLamp ESP32-S3\","
        "\"sw\":\"" FIRMWARE_VERSION "\"}}",
        uid, t, t, t, t, uid);
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

    // Reject characters that would break MQTT topic syntax or JSON embedding.
    // MQTT wildcards (#, +) produce illegal topics; " and \ break the discovery
    // payload (jsonEscape doubles \, which can truncate the fixed 896-byte buffer).
    for (int i = (int)t.length() - 1; i >= 0; i--) {
        char c = t[i];
        if (c == '#' || c == '+' || c == '"' || c == '\\' || (uint8_t)c < 0x21 || (uint8_t)c > 0x7E)
            t.remove(i, 1);
    }
    // Re-apply the default if sanitizing emptied the topic — e.g. a Cyrillic base
    // topic whose bytes are all > 0x7E gets fully stripped. An empty prefix yields
    // "/set"/"/state" and permanently silences mqttPublishState().
    if (t.length() == 0) t = "firelamp";

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
    // Read from in-memory module state — always consistent with the running config.
    // Password is write-only: return only whether it's set, not the value.
    // ip/u/t are <=64 raw; jsonEscape can double each, so 512 covers the worst case.
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"ip\":\"%s\",\"pt\":%d,\"u\":\"%s\",\"p_set\":%s,\"t\":\"%s\"}",
             jsonEscape(mqIp).c_str(), mqPt, jsonEscape(mqU).c_str(),
             mqP.length() > 0 ? "true" : "false", jsonEscape(mqT).c_str());
    server.send(200, "application/json", buf);
}

void registerMqttHandlers() {
    server.on("/setmqtt", HTTP_POST, handleSetMqtt);
    server.on("/getmqtt", HTTP_GET, handleGetMqtt);
}
