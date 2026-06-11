#include <Arduino.h>
#include <Update.h>
#include "globals.h"
#include "net_helpers.h"

// =============================================================================
//  MANUAL FIRMWARE FLASH  (/flash)
//  Recovery endpoint: user downloads firmware.bin from GitHub Releases in their
//  browser, then uploads it directly to the lamp over local HTTP — no TLS to
//  GitHub required.  Useful when the OTA-to-GitHub path is broken.
// =============================================================================

static bool s_flashAuth = false;

static void handleFlashPage() {
    server.send(200, "text/html", F(
        "<!doctype html><meta name='viewport' content='width=device-width'>"
        "<style>body{font:15px sans-serif;max-width:420px;margin:40px auto;"
        "padding:0 16px;background:#111;color:#eee}h2{color:#f80}a{color:#f80}"
        "button{padding:10px 20px;background:#c44;color:#fff;border:none;"
        "border-radius:4px;cursor:pointer;font-size:15px}button:disabled{opacity:.4}"
        "pre{background:#222;padding:10px;border-radius:4px;white-space:pre-wrap}"
        "input{display:block;margin:12px 0}</style>"
        "<h2>Manual Firmware Flash</h2>"
        "<p>1. Download <code>firmware.bin</code> from "
        "<a href='https://github.com/maksimshchuplov/firelamp/releases/latest'"
        " target='_blank'>GitHub Releases</a>.</p>"
        "<p>2. Select the file below and click Upload.</p>"
        "<input type='file' id='f' accept='.bin'>"
        "<button id='b' onclick='up()'>Upload</button>"
        "<pre id='s'></pre>"
        "<script>"
        "function up(){"
        "var f=document.getElementById('f').files[0];"
        "if(!f){alert('Select firmware.bin first');return;}"
        "if(f.size<50000){alert('File too small — wrong file?');return;}"
        "var s=document.getElementById('s');"
        "document.getElementById('b').disabled=true;"
        "s.textContent='Uploading '+Math.round(f.size/1024)+' KB…';"
        "var fd=new FormData();fd.append('firmware',f);"
        "fetch('/flash',{method:'POST',headers:{'X-Requested-With':'firelamp'},body:fd})"
        ".then(r=>r.text().then(t=>({ok:r.ok,t})))"
        ".then(({ok,t})=>{"
        "if(ok){s.textContent=t+'\\nReloading in 8 s…';"
        "setTimeout(()=>location.href='/',8000);}"
        "else{s.textContent='Error: '+t;"
        "document.getElementById('b').disabled=false;}})"
        ".catch(()=>{"
        "s.textContent='Upload sent — lamp rebooting.\\nReloading in 10 s…';"
        "setTimeout(()=>location.href='/',10000);});}"
        "</script>"
    ));
}

static void handleFlashUpload() {
    HTTPUpload &up = server.upload();
    if (up.status == UPLOAD_FILE_START) {
        s_flashAuth = (server.header("X-Requested-With") == "firelamp");
        if (s_flashAuth && !Update.begin(UPDATE_SIZE_UNKNOWN)) {
            LOG_ERROR("Flash: Update.begin: %s", Update.errorString());
            s_flashAuth = false;
        }
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (s_flashAuth && Update.write(up.buf, up.currentSize) != up.currentSize) {
            LOG_ERROR("Flash: write failed at offset %u", (unsigned)up.totalSize);
            Update.abort();
            s_flashAuth = false;
        }
    } else if (up.status == UPLOAD_FILE_END) {
        if (s_flashAuth && !Update.end(true))
            LOG_ERROR("Flash: Update.end: %s", Update.errorString());
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        if (s_flashAuth) Update.abort();
        s_flashAuth = false;
    }
}

static void handleFlashDone() {
    if (!s_flashAuth) {
        server.send(403, "text/plain", "Forbidden");
        return;
    }
    if (Update.hasError()) {
        String e = Update.errorString();
        s_flashAuth = false;
        server.send(500, "text/plain", String("Flash failed: ") + e);
        return;
    }
    s_flashAuth = false;
    if (prefsDirty.load(std::memory_order_relaxed)) {
        if (flushPrefs())
            prefsDirty.store(false, std::memory_order_relaxed);
        else
            LOG_WARN("Flash: NVS pre-flush failed — settings may not persist");
    }
    server.send(200, "text/plain", "Flash complete — rebooting...");
    server.client().flush();
    server.client().stop();
    delay(100);
    ESP.restart();
}

void registerFlashHandlers() {
    server.on("/flash", HTTP_GET,  handleFlashPage);
    server.on("/flash", HTTP_POST, handleFlashDone, handleFlashUpload);
}
