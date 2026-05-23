#include <Arduino.h>
#include <WebSocketsServer.h>
#include "globals.h"
#include "net_helpers.h"

static WebSocketsServer      wsServer(WS_PORT);
static std::atomic<bool>     wsReady{false};

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
}

void wsPushState() {
    if (!wsReady.load(std::memory_order_acquire)) return;
    if (wsServer.connectedClients() == 0) return;
    char j[96];
    formatState(j, sizeof(j));
    wsServer.broadcastTXT(j);
}
