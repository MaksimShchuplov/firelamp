#include <Arduino.h>
#include <WebSocketsServer.h>
#include "globals.h"

static WebSocketsServer wsServer(WS_PORT);
static bool             wsReady = false;

static void onWsEvent(uint8_t num, WStype_t type, uint8_t *, size_t) {
    // Push-only server: we don't process incoming messages.
    // Send current state to the newly connected client so it is synchronised
    // immediately even if it missed a broadcast while disconnected.
    // Use sendTXT(num) — not broadcastTXT — to target only the new client.
    if (type == WStype_CONNECTED) {
        char j[96];
        snprintf(j, sizeof(j),
                 "{\"b\":%d,\"c\":%d,\"co\":%d,\"sp\":%d,\"w\":%.1f,\"bl\":%d,\"th\":%d,\"upd\":%d}",
                 (int)uiBright, (int)uiContrast, (int)uiCooling, (int)uiSparking,
                 (float)currentPowerMw / 1000.0f, (int)uiBlend, (int)uiTheme, updatePending ? 1 : 0);
        wsServer.sendTXT(num, j);
    }
}

void wsSetup() {
    wsServer.begin();
    wsServer.onEvent(onWsEvent);
    wsReady = true;
}

void wsLoop() {
    if (!wsReady) return;
    wsServer.loop();
}

void wsPushState() {
    if (wsServer.connectedClients() == 0) return;
    char j[96];
    snprintf(j, sizeof(j),
             "{\"b\":%d,\"c\":%d,\"co\":%d,\"sp\":%d,\"w\":%.1f,\"bl\":%d,\"th\":%d,\"upd\":%d}",
             (int)uiBright, (int)uiContrast, (int)uiCooling, (int)uiSparking,
             (float)currentPowerMw / 1000.0f, (int)uiBlend, (int)uiTheme, updatePending ? 1 : 0);
    wsServer.broadcastTXT(j);
}
