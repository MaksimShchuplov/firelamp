#include <Arduino.h>
#include <WebSocketsServer.h>
#include "globals.h"
#include "net_helpers.h"

static WebSocketsServer      wsServer(WS_PORT);
static std::atomic<bool>     wsReady{false};

// Deferred surprise push: surpriseTask writes here, wsLoop() drains on Core 1.
// Direct broadcastTXT from a FreeRTOS task would race with wsServer.loop().
static char                  s_surpriseName[128];
static std::atomic<bool>     s_surprisePending{false};

static void onWsEvent(uint8_t num, WStype_t type, uint8_t *, size_t) {
    // Push-only server: we don't process incoming messages.
    // Send current state to the newly connected client so it is synchronised
    // immediately even if it missed a broadcast while disconnected.
    // Use sendTXT(num) — not broadcastTXT — to target only the new client.
    if (type == WStype_CONNECTED) {
        char j[96];
        formatState(j, sizeof(j));
        wsServer.sendTXT(num, j);
    }
}

void wsSetup() {
    wsServer.begin();
    wsServer.onEvent(onWsEvent);
    // Ping every 10 s; wait 3 s for pong; drop after 2 missed pongs.
    // Prevents NAT/router idle-TCP teardown that silently kills WS connections.
    wsServer.enableHeartbeat(10000, 3000, 2);
    // release ensures all wsServer writes above are visible before wsReady is seen true on Core 1
    wsReady.store(true, std::memory_order_release);
}

void wsLoop() {
    if (!wsReady.load(std::memory_order_acquire)) return;
    wsServer.loop();
    if (s_surprisePending.load(std::memory_order_acquire)) {
        s_surprisePending.store(false, std::memory_order_relaxed);
        char j[256];
        char state[96];
        formatState(state, sizeof(state));
        int slen = strlen(state);
        if (slen > 0 && state[slen - 1] == '}') state[slen - 1] = '\0';
        snprintf(j, sizeof(j), "%s,\"name\":\"%s\"}", state, s_surpriseName);
        if (wsServer.connectedClients() > 0) wsServer.broadcastTXT(j);
    }
}

void wsPushState() {
    if (!wsReady.load(std::memory_order_acquire)) return;
    if (wsServer.connectedClients() == 0) return;
    char j[96];
    formatState(j, sizeof(j));
    wsServer.broadcastTXT(j);
}

void wsPushSurprise(const char *escapedName) {
    // Called from surpriseTask (a separate FreeRTOS task on Core 1).
    // Must NOT call wsServer directly — wsServer.loop() runs in the Arduino
    // task on the same core; preemptive scheduling would cause a data race.
    // Instead, copy the name into a static buffer and set a flag that wsLoop()
    // drains on the next iteration (always from the Arduino task).
    strncpy(s_surpriseName, escapedName, sizeof(s_surpriseName) - 1);
    s_surpriseName[sizeof(s_surpriseName) - 1] = '\0';
    s_surprisePending.store(true, std::memory_order_release);
}
