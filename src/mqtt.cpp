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
        mqttClient.setServer(mqIp.c_str(), mqPt);
        mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
            String s = "";
            for(unsigned int i=0; i<length; i++) s += (char)payload[i];
            
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, s);
            if (!err) {
                bool changed = false;
                static uint8_t last_b = 100;

                if (doc.containsKey("state")) {
                    String st = doc["state"];
                    if (st == "OFF" && uiBright > 0) {
                        last_b = uiBright;
                        setBright(0);
                        changed = true;
                    } else if (st == "ON" && uiBright == 0) {
                        setBright(last_b > 0 ? last_b : 100);
                        changed = true;
                    }
                }

                if (doc.containsKey("b"))  { 
                    int v = doc["b"]; 
                    setBright(constrain(v, 0, 100)); 
                    if (v > 0) last_b = v; // remember last non-zero brightness
                    changed = true; 
                }
                if (doc.containsKey("c"))  { uiContrast = constrain((int)doc["c"], 0, 100); changed = true; }
                if (doc.containsKey("co")) { uiCooling = constrain((int)doc["co"], 20, 150); recalcCooling(); changed = true; }
                if (doc.containsKey("sp")) { uiSparking = constrain((int)doc["sp"], 0, 255); changed = true; }
                if (doc.containsKey("bl")) { uiBlend = constrain((int)doc["bl"], 0, 255); changed = true; }
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

void serviceMqtt() {
    if (mqIp.length() == 0 || WiFi.status() != WL_CONNECTED) return;
    
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            String clientId = "firelamp-" + String(random(0xffff), HEX);
            bool connected = false;
            
            if (mqU.length() > 0) {
                connected = mqttClient.connect(clientId.c_str(), mqU.c_str(), mqP.c_str());
            } else {
                connected = mqttClient.connect(clientId.c_str());
            }
            
            if (connected) {
                LOG_INFO("MQTT connected to %s:%d", mqIp.c_str(), mqPt);
                String sub = mqT + "/set";
                mqttClient.subscribe(sub.c_str());
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
    if (pw.length() > 0) p.putString("p", pw);  // empty means "keep existing"
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
    bool   hasPw = p.isKey("p") && p.getString("p", "").length() > 0;
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
