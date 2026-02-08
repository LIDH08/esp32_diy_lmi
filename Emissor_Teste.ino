#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <esp_now.h>
#include <esp_wifi.h>

const char* ssid = "Intermete do Ze";
const char* password = "zacarias";
uint8_t broadcastAddress[] = {0xC0, 0x49, 0xEF, 0xD1, 0x90, 0x64}; 

typedef struct struct_message {
    float ang_base, ang_braco1, ang_braco2, extensao, pressao, tilt_x;
    int rpm;
    bool motor_ligado;
    uint32_t msg_id;
} struct_message;

struct_message dados;
WebServer server(80);
unsigned long lastSendTime = 0;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
    struct_message temp;
    memcpy(&temp, incomingData, sizeof(temp));
    dados.motor_ligado = temp.motor_ligado;
    dados.rpm = temp.rpm;
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><title>EMISSOR PM</title>
<style>
  body { font-family: sans-serif; background: #121212; color: #eee; text-align: center; }
  .card { background: #1e1e1e; border-radius: 15px; padding: 20px; max-width: 400px; margin: auto; border: 1px solid #333; }
  input[type=range] { width: 100%; accent-color: #f39c12; margin: 10px 0; }
  .group { display: flex; justify-content: space-between; font-weight: bold; margin-top: 15px; }
  .val { color: #f39c12; font-family: monospace; font-size: 1.2em; }
  .status { padding: 10px; border-radius: 5px; margin-bottom: 10px; font-weight: bold; }
  .on { background: #27ae60; } .off { background: #c0392b; }
</style></head><body>
  <div class="card">
    <h2>📡 EMISSOR PM</h2>
    <div id="m_st" class="status off">MOTOR OFF</div>
    <div class="group"><span>RPM:</span><span id="rv" class="val">0</span></div>
    <hr>
    <div class="group"><span>Pressão:</span><span id="pv" class="val">0</span> bar</div>
    <input type="range" min="0" max="300" oninput="u('p',this.value); d('pv',this.value)">
    <div class="group"><span>Braço 1:</span><span id="a1v" class="val">0</span>°</div>
    <input type="range" min="-20" max="85" oninput="u('a1',this.value); d('a1v',this.value)">
    <div class="group"><span>Braço 2:</span><span id="a2v" class="val">0</span>°</div>
    <input type="range" min="0" max="160" oninput="u('a2',this.value); d('a2v',this.value)">
    <div class="group"><span>Extensão:</span><span id="ev" class="val">0</span> m</div>
    <input type="range" min="0" max="8" step="0.1" oninput="u('e',this.value); d('ev',this.value)">
    <div class="group"><span>Base:</span><span id="abv" class="val">0</span>°</div>
    <input type="range" min="0" max="360" oninput="u('ab',this.value); d('abv',this.value)">
    <br><a href="/update" style="color:#444; font-size:0.8em">OTA UPDATE</a>
  </div>
<script>
  function u(p,v) { fetch(`/set?${p}=${v}`); }
  function d(i,v) { document.getElementById(i).innerHTML = v; }
  function sync() {
    fetch('/data').then(r=>r.json()).then(d=>{
      document.getElementById('m_st').innerText = d.mot ? "MOTOR ON" : "MOTOR OFF";
      document.getElementById('m_st').className = d.mot ? "status on" : "status off";
      document.getElementById('rv').innerText = d.rpm;
    });
  }
  setInterval(sync, 500);
</script></body></html>)rawliteral";

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA); WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    esp_now_init();
    esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
    esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv);
    esp_now_peer_info_t p = {}; memcpy(p.peer_addr, broadcastAddress, 6); p.channel = WiFi.channel(); esp_now_add_peer(&p);
    server.on("/", [](){ server.send(200, "text/html", index_html); });
    server.on("/data", [](){ String j = "{\"mot\":"+String(dados.motor_ligado)+",\"rpm\":"+String(dados.rpm)+"}"; server.send(200, "application/json", j); });
    server.on("/set", [](){
        if (server.hasArg("p"))  dados.pressao = server.arg("p").toFloat();
        if (server.hasArg("a1")) dados.ang_braco1 = server.arg("a1").toFloat();
        if (server.hasArg("a2")) dados.ang_braco2 = server.arg("a2").toFloat();
        if (server.hasArg("e"))  dados.extensao = server.arg("e").toFloat();
        if (server.hasArg("ab")) dados.ang_base = server.arg("ab").toFloat();
        esp_now_send(broadcastAddress, (uint8_t *) &dados, sizeof(dados));
        server.send(200);
    });
    ElegantOTA.begin(&server); server.begin();
}
void loop() { server.handleClient(); ElegantOTA.loop(); 
    if(millis() - lastSendTime > 500) { esp_now_send(broadcastAddress, (uint8_t *) &dados, sizeof(dados)); lastSendTime = millis(); }
}
