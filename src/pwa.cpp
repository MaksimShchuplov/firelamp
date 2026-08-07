#include <Arduino.h>
#include "globals.h"
#include "net_helpers.h"
#include "page.h"

// =============================================================================
//  PWA — installable home-screen app (manifest + service worker)
//  Both assets are public, read-only, and require no CSRF guard.
// =============================================================================

static void handleManifest() {
    server.sendHeader("Cache-Control", "public, max-age=86400");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/manifest+json", "");
    server.sendContent_P(MANIFEST_JSON);
    server.sendContent("");
}

// Network-first, cache-fallback SW. Navigation requests (page load while lamp is
// off) are served from cache; API calls fall through so existing offline detection works.
static const char SW_JS[] PROGMEM =
    "const C='fl-v2';"
    // skipWaiting + clients.claim so a redeployed SW takes over immediately
    // instead of waiting for every PWA tab to close (single-tab home-screen app).
    "self.addEventListener('install',e=>{self.skipWaiting();e.waitUntil(caches.open(C).then(c=>c.add('/')))});"
    "self.addEventListener('activate',e=>e.waitUntil(caches.keys().then(ks=>Promise.all(ks.filter(k=>k!==C).map(k=>caches.delete(k)))).then(()=>self.clients.claim())));"
    "self.addEventListener('fetch',e=>{"
      "if(e.request.method!=='GET')return;"
      // Cache navigations only: the offline fallback serves caches.match('/'),
      // so caching API GETs (/state, /setb?v=N × every slider value) is pure
      // storage bloat. Clone synchronously — the page may lock the body the
      // moment respondWith() resolves, and a late clone() throws.
      "e.respondWith(fetch(e.request).then(r=>{"
        "if(r.ok&&e.request.mode==='navigate'){const rc=r.clone();caches.open(C).then(c=>c.put(e.request,rc));}"
        "return r;"
      "}).catch(()=>e.request.mode==='navigate'"
        "?caches.match('/').then(r=>r||new Response("
          "'<meta charset=utf-8>"
          "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
          "<title>Fire Lamp</title>"
          "<body style=\"margin:0;height:100vh;display:flex;flex-direction:column;"
          "align-items:center;justify-content:center;background:#0a0503;"
          "font:600 16px system-ui,sans-serif\">"
          "<div style=\"font-size:42px;color:#e8632a\">Fire Lamp</div>"
          "<div style=\"margin-top:10px;color:#888\">Offline</div>',"
          "{headers:{'Content-Type':'text/html;charset=utf-8'}}))"
        ":Promise.reject()));"
    "});";

static void handleServiceWorker() {
    // No-cache so the browser always revalidates the SW for version bumps.
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Service-Worker-Allowed", "/");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/javascript", "");
    server.sendContent_P(SW_JS);
    server.sendContent("");
}

void registerPwaHandlers() {
    server.on("/manifest.json", handleManifest);
    server.on("/sw.js",         handleServiceWorker);
}
