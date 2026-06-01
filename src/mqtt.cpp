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
    snprintf(j, sizeof(j), "{\"b\":%d,\"c\":%d,\"co\":%d,\"sp\":%d,\"bl\":%d,\"th\":%d}",
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
                if (doc.containsKey("b"))  { int v = doc["b"]; setBright(constrain(v, 0, 100)); changed = true; }
                if (doc.containsKey("c"))  { uiContrast = constrain((int)doc["c"], 0, 100); changed = true; }
                if (doc.containsKey("co")) { uiCooling = constrain((int)doc["co"], 20, 150); recalcCooling(); changed = true; }
                if (doc.containsKey("sp")) { uiSparking = constrain((int)doc["sp"], 0, 255); changed = true; }
                if (doc.containsKey("bl")) { uiBlend = constrain((int)doc["bl"], 0, 255); changed = true; }
                if (doc.containsKey("th")) { uiTheme = constrain((int)doc["th"], 0, 3); changed = true; }
                
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
    Preferences p;
    p.begin("mqtt", false);
    p.putString("ip", server.arg("ip"));
    p.putInt("pt", server.arg("pt").toInt());
    p.putString("u", server.arg("u"));
    p.putString("p", server.arg("p"));
    p.putString("t", server.arg("t"));
    p.end();
    
    server.send(200, "application/json", "{\"ok\":true}");
    mqttClient.disconnect();
    initMqtt();
}

static void handleGetMqtt() {
    Preferences p;
    p.begin("mqtt", true);
    String ip = p.getString("ip", "");
    int pt = p.getInt("pt", 1883);
    String u = p.getString("u", "");
    String pw = p.getString("p", "");
    String t = p.getString("t", "firelamp");
    p.end();
    
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"ip\":\"%s\",\"pt\":%d,\"u\":\"%s\",\"p\":\"%s\",\"t\":\"%s\"}",
             jsonEscape(ip).c_str(), pt, jsonEscape(u).c_str(), jsonEscape(pw).c_str(), jsonEscape(t).c_str());
    server.send(200, "application/json", buf);
}

void registerMqttHandlers() {
    server.on("/setmqtt", HTTP_POST, handleSetMqtt);
    server.on("/getmqtt", HTTP_GET, handleGetMqtt);
}
