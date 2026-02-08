#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <esp_now.h>
#include <esp_wifi.h>

const char* ssid = "Intermete do Ze";
const char* password = "zacarias";
uint8_t broadcastAddress[] = {0x08, 0xD1, 0xF9, 0xE7, 0x47, 0x38}; 

typedef struct struct_message {
    float ang_base, ang_braco1, ang_braco2, extensao, pressao, tilt_x;
    int rpm; bool motor_ligado; uint32_t msg_id;
} struct_message;

struct_message dados;
WebServer server(80);
unsigned long lastRecvTime = 0;

void handleEngine() {
    String c = server.arg("c");
    if(c=="start") dados.motor_ligado = true;
    else if(c=="stop") { dados.motor_ligado = false; dados.rpm = 0; }
    else if(c=="up") dados.rpm += 100;
    else if(c=="down") dados.rpm -= 100;
    esp_now_send(broadcastAddress, (uint8_t *) &dados, sizeof(dados));
    server.send(200);
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    memcpy(&dados, incomingData, sizeof(dados));
    lastRecvTime = millis();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><style>
  body { background:#000; color:#0f0; font-family:monospace; text-align:center; margin:0; padding:5px; }
  .screen { border:2px solid #444; background:#020202; max-width:800px; margin:auto; padding:10px; min-height:95vh; display:flex; flex-direction:column; }
  .tm-header { color:#f39c12; font-size:22px; font-weight:bold; margin-bottom:5px; }
  .led-bar { display:flex; gap:2px; height:25px; background:#111; margin-bottom:15px; border:1px solid #333; }
  .seg { flex:1; background:#222; }
  .g { background:#0f0; box-shadow:0 0 5px #0f0; } .y { background:#ff0; box-shadow:0 0 5px #ff0; } .r { background:#f00; box-shadow:0 0 10px #f00; }
  .diag { background:#050505; border:1px solid #222; height:280px; position:relative; }
  .footer-stats { display:grid; grid-template-columns:repeat(4, 1fr); background:#111; border:2px solid #333; padding:10px; margin-top:auto; gap:5px; }
  .stat-value { font-size:18px; color:#fff; font-weight:bold; }
  #sector-warn { background:#f00; color:#fff; font-weight:bold; display:none; padding:5px; animation:blink 0.5s infinite; }
  @keyframes blink { 0%{opacity:1} 50%{opacity:0.2} 100%{opacity:1} }
</style></head><body>
<div class="screen">
  <div class="tm-header"><span id="v_tm">0.0</span> / <span id="v_limit" style="font-size:0.6em">10.8</span> TM</div>
  <div class="led-bar" id="l-bar"></div>
  <div id="sector-warn">LIMITE DE SEGURANÇA REDUZIDO</div>

  <div style="display:grid; grid-template-columns: 1fr 1fr; gap:10px; flex-grow:1;">
    <div class="diag">
      <svg viewBox="0 0 200 200" width="100%" height="100%">
        <g transform="translate(40, 160)"><line x1="-20" x2="140" stroke="#333"/><g id="g_a1"><line x2="70" stroke="#f39c12" stroke-width="8"/><text id="t_a1" x="35" y="-10" fill="#f39c12" style="font-size:10px" text-anchor="middle">0°</text>
        <g id="g_a2" transform="translate(70,0)"><line x2="50" stroke="#888" stroke-width="6"/><text id="t_a2" x="25" y="-10" fill="#fff" style="font-size:10px" text-anchor="middle">0°</text><line id="g_ext" x1="50" x2="80" stroke="#fff" stroke-width="2" stroke-dasharray="2,1"/></g></g></g>
      </svg>
    </div>
    <div class="diag">
      <svg viewBox="0 0 200 200" width="100%" height="100%">
        <circle cx="100" cy="100" r="75" fill="none" stroke="#222" stroke-dasharray="3,3"/>
        <path d="M 100 100 L 47 47 A 75 75 0 0 1 153 47 Z" fill="rgba(255,0,0,0.15)" /> <!-- ZONA CABINE -->
        <rect x="90" y="65" width="20" height="25" fill="none" stroke="#444" /> <!-- CABINE -->
        
        <!-- CÍRCULO DE ALCANCE MÁXIMO PERMITIDO (DINÂMICO) -->
        <circle id="safe-radius-circle" cx="100" cy="100" r="0" fill="none" stroke="rgba(243, 156, 18, 0.4)" stroke-width="2" stroke-dasharray="5,2" />
        
        <g id="g_rot" transform="translate(100,100)"><line y2="-72" stroke="#f39c12" stroke-width="5" stroke-linecap="round"/><text id="t_ab" x="0" y="-78" fill="#f39c12" style="font-size:10px" text-anchor="middle">0°</text></g>
      </svg>
    </div>
  </div>

  <div class="footer-stats">
    <div><span style="font-size:10px">PESO KG</span><br><span id="v_peso" class="stat-value">0</span></div>
    <div><span style="font-size:10px">ALCANCE</span><br><span id="v_r" class="stat-value">0.0</span>M</div>
    <div><span style="font-size:10px">LIMITE</span><br><span id="v_rmax" style="color:#f39c12" class="stat-value">0.0</span>M</div>
    <div><span style="font-size:10px">RPM</span><br><span id="v_rpm" class="stat-value">0</span></div>
  </div>
</div>

<script>
  const bar = document.getElementById('l-bar');
  for(let i=0; i<30; i++) { let s = document.createElement('div'); s.className='seg'; bar.appendChild(s); }
  
  function update() {
    fetch('/data').then(r=>r.json()).then(d=>{
      let momento = (d.peso * d.r);
      let limiteTM = 10.8;
      
      if (d.ab < 45 || d.ab > 315) limiteTM = 6.48; // 60% Cabina
      else if ((d.ab >= 45 && d.ab <= 135) || (d.ab >= 225 && d.ab <= 315)) limiteTM = 8.64; // 80% Sapatas
      
      // Cálculo do Alcance Máximo para o peso atual (R = M / P)
      let rMax = d.peso > 0.1 ? (limiteTM / d.peso) : 10.0;
      if(rMax > 10) rMax = 10;

      document.getElementById('v_tm').innerText = momento.toFixed(1);
      document.getElementById('v_limit').innerText = limiteTM.toFixed(1);
      document.getElementById('v_peso').innerText = Math.round(d.peso * 1000);
      document.getElementById('v_r').innerText = d.r.toFixed(1);
      document.getElementById('v_rmax').innerText = rMax.toFixed(1);
      document.getElementById('v_rpm').innerText = d.rpm;

      document.getElementById('sector-warn').style.display = (limiteTM < 10.8) ? 'block' : 'none';
      document.getElementById('t_a1').textContent = Math.round(d.a1) + "°";
      document.getElementById('t_a2').textContent = Math.round(d.a2) + "°";
      document.getElementById('t_ab').textContent = Math.round(d.ab) + "°";

      document.getElementById('g_a1').setAttribute('transform', `rotate(${-d.a1})`);
      document.getElementById('g_a2').setAttribute('transform', `translate(70,0) rotate(${d.a2})`);
      document.getElementById('g_ext').setAttribute('x2', 50 + (d.e * 6));
      document.getElementById('g_rot').setAttribute('transform', `translate(100,100) rotate(${d.ab})`);

      // Atualiza Círculo de Segurança no SVG 360 (Escala: 7.5px por metro)
      document.getElementById('safe-radius-circle').setAttribute('r', rMax * 7.5);

      let p_perc = (momento / limiteTM) * 100;
      document.querySelectorAll('.seg').forEach((s,i) => {
        let step = (i / 30) * 120; s.className='seg';
        if(step <= p_perc) {
          if(step < 80) s.classList.add('g'); else if(step < 90) s.classList.add('y'); else s.classList.add('r');
        }
      });
    });
  }
  setInterval(update, 300);
</script></body></html>)rawliteral";

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA); WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    esp_now_init();
    esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv);
    esp_now_peer_info_t p={}; memcpy(p.peer_addr, broadcastAddress, 6); p.channel=WiFi.channel(); esp_now_add_peer(&p);
    server.on("/", [](){ server.send(200, "text/html", index_html); });
    server.on("/data", [](){
        float r = (3.8 * cos(dados.ang_braco1*0.0174)) + ((3.2 + dados.extensao) * cos((dados.ang_braco1-dados.ang_braco2)*0.0174));
        float h = 1.2 + (3.8 * sin(dados.ang_braco1*0.0174)) + ((3.2 + dados.extensao) * sin((dados.ang_braco1-dados.ang_braco2)*0.0174));
        String j = "{\"p\":"+String(dados.pressao)+",\"peso\":"+String((dados.pressao-15)*0.021)+",\"r\":"+String(r)+",\"h\":"+String(h)+",\"ab\":"+String(dados.ang_base)+",\"a1\":"+String(dados.ang_braco1)+",\"a2\":"+String(dados.ang_braco2)+",\"e\":"+String(dados.extensao)+",\"tx\":"+String(dados.tilt_x)+",\"rpm\":"+String(dados.rpm)+",\"mot\":"+String(dados.motor_ligado)+"}";
        server.send(200, "application/json", j);
    });
    ElegantOTA.begin(&server); server.begin();
}
void loop() { server.handleClient(); ElegantOTA.loop(); }
