#include "web.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static WebServer        g_server(80);
static SemaphoreHandle_t g_mutex = nullptr;
static ImuReading        g_snapshot = {};
static bool              g_init = false;

// Tiny self-contained HTML/JS. Polls /imu every 100 ms; renders accel,
// gyro, temp, and the age of the latest sample.
static const char INDEX_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SwarmLoc field node — IMU</title>
<style>
  :root { color-scheme: dark; }
  body { font-family: -apple-system, system-ui, sans-serif;
         background:#111; color:#eee; margin:1.5em; }
  h1   { font-size:1.4em; margin:0 0 0.2em 0; }
  .sub { color:#888; font-size:0.9em; margin-bottom:1em; }
  .grid{ display:grid; grid-template-columns:auto auto;
         gap:0.3em 1.2em; max-width:24em; }
  .k   { color:#88f; }
  .v   { font-family: ui-monospace, monospace; font-size:1.15em; }
  .ok  { color:#7c7; }
  .stale { color:#c77; }
</style></head>
<body>
  <h1>SwarmLoc node — IMU</h1>
  <div class="sub" id="meta">connecting...</div>
  <div class="grid">
    <span class="k">accel x (g)</span><span class="v" id="ax">—</span>
    <span class="k">accel y (g)</span><span class="v" id="ay">—</span>
    <span class="k">accel z (g)</span><span class="v" id="az">—</span>
    <span class="k">gyro x (°/s)</span><span class="v" id="gx">—</span>
    <span class="k">gyro y (°/s)</span><span class="v" id="gy">—</span>
    <span class="k">gyro z (°/s)</span><span class="v" id="gz">—</span>
    <span class="k">temp (°C)</span><span class="v" id="t">—</span>
    <span class="k">sample age</span><span class="v" id="age">—</span>
  </div>
<script>
async function poll(){
  try {
    const r = await fetch('/imu', {cache:'no-store'});
    const j = await r.json();
    const fmt = (n,p) => (typeof n === 'number' ? n.toFixed(p) : n);
    document.getElementById('ax').textContent = fmt(j.ax,3);
    document.getElementById('ay').textContent = fmt(j.ay,3);
    document.getElementById('az').textContent = fmt(j.az,3);
    document.getElementById('gx').textContent = fmt(j.gx,2);
    document.getElementById('gy').textContent = fmt(j.gy,2);
    document.getElementById('gz').textContent = fmt(j.gz,2);
    document.getElementById('t').textContent  = fmt(j.t,2);
    const meta = document.getElementById('meta');
    if (j.ms) {
      const age = Date.now()/1000 - j.ms/1000;
      const ageStr = age < 1 ? `${(age*1000).toFixed(0)} ms` : `${age.toFixed(1)} s`;
      document.getElementById('age').textContent = ageStr;
      meta.textContent = `polling /imu @ 10 Hz · uptime ${(j.ms/1000).toFixed(0)} s`;
      meta.className = j.ready ? 'sub ok' : 'sub stale';
    } else {
      meta.textContent = 'IMU offline';
      meta.className = 'sub stale';
    }
  } catch (e) {
    document.getElementById('meta').textContent = 'fetch failed: ' + e;
    document.getElementById('meta').className = 'sub stale';
  }
  setTimeout(poll, 100);
}
poll();
</script>
</body></html>
)HTML";

bool web_init() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[web] cannot start — WiFi not connected");
    return false;
  }

  if (!g_mutex) g_mutex = xSemaphoreCreateMutex();

  g_server.on("/", []() {
    g_server.send(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  g_server.on("/imu", []() {
    ImuReading r{};
    bool have_lock = false;
    if (g_mutex && xSemaphoreTake(g_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      r = g_snapshot;
      xSemaphoreGive(g_mutex);
      have_lock = true;
    }
    char buf[320];
    snprintf(buf, sizeof(buf),
             "{\"ready\":%s,\"ax\":%.4f,\"ay\":%.4f,\"az\":%.4f,"
             "\"gx\":%.3f,\"gy\":%.3f,\"gz\":%.3f,"
             "\"t\":%.2f,\"ms\":%lu}",
             (have_lock && r.millis_when) ? "true" : "false",
             r.accel_x, r.accel_y, r.accel_z,
             r.gyro_x,  r.gyro_y,  r.gyro_z,
             r.temp_c,  (unsigned long)r.millis_when);
    g_server.send(200, "application/json", buf);
  });

  g_server.onNotFound([]() {
    g_server.send(404, "text/plain", "not found\n");
  });

  g_server.begin();
  g_init = true;
  Serial.printf("[web] listening on http://%s/\n",
                WiFi.localIP().toString().c_str());
  return true;
}

void web_handle() {
  if (g_init) g_server.handleClient();
}

void web_set_imu(const ImuReading& r) {
  if (!g_mutex) return;
  if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    g_snapshot = r;
    xSemaphoreGive(g_mutex);
  }
}
