#include "GatewayWebServer.h"

#include <Update.h>

#include "../Gateway/BuildInfo.h"
#include "../Gateway/DebugOutput.h"
#include "../ModbusServer/GoodWeInverter.h"
#include "../ModbusClient/HuaweiDTSU666HW.h"

namespace Gateway
{
namespace
{
constexpr uint16_t WEB_SERVER_PORT = 80;

String goodWePhysicalValue(
    const uint16_t *registers,
    uint16_t index)
{
    const uint16_t address = GoodWeInverter::REG_START + index;
    switch (address)
    {
    case 306:
    case 307:
    case 308:
        return String(registers[index] / GoodWeInverter::VOLTAGE_SCALE, 1) + " V";
    case 309:
    case 311:
    case 313:
    {
        const uint16_t raw = registers[index + 1];
        const int32_t current =
            raw < 0x8000u ? raw : static_cast<int32_t>(raw) - 0x10000;
        return String(current / DTSU666HW_CURRENT_SCALE, 2) + " A";
    }
    case 315:
    case 317:
    case 319:
    case 321:
    {
        const uint16_t raw = registers[index + 1];
        int32_t power =
            raw < 0x8000u ? raw : static_cast<int32_t>(raw) - 0x10000;
        if (registers[index] != 0)
        {
            power = power < 0 ? power : -power;
        }
        return String(power) + " W";
    }
    default:
        return "-";
    }
}

const char WEB_STYLE[] PROGMEM = R"rawliteral(
*{box-sizing:border-box}
body{font-family:Arial,sans-serif;max-width:1100px;margin:2rem auto;padding:0 1rem;background:#111827;color:#e5e7eb}
a{color:#7dd3fc}h1{margin-bottom:.3rem}h2{margin-top:2rem;border-bottom:1px solid #374151;padding-bottom:.4rem}
nav{display:flex;flex-wrap:wrap;gap:.6rem;margin:1.2rem 0}
nav a,.button,button{display:inline-block;font:inherit;text-decoration:none;padding:.6rem 1rem;border:1px solid #4b5563;border-radius:6px;background:#1f2937;color:#e5e7eb;cursor:pointer}
nav a:hover,.button:hover,button:hover{background:#374151}
button.primary{background:#0369a1;border-color:#0284c7}
button.danger{background:#991b1b;border-color:#b91c1c}
button.selected,button.selected:hover{background:#15803d;border-color:#4ade80;color:#fff;box-shadow:0 0 0 2px #4ade8066}
.status,.card,.controls,form{padding:1rem;background:#1f2937;border:1px solid #374151;border-radius:8px;margin:1rem 0}
.status{border-left:5px solid #22c55e}.status.error{border-left-color:#ef4444}
table{border-collapse:collapse;width:100%;background:#1f2937;margin-bottom:1rem}
th,td{text-align:left;padding:.55rem .7rem;border-bottom:1px solid #374151}th{background:#273449;color:#cbd5e1}
input,button{font:inherit;padding:.6rem;margin:.25rem 0}input[type=file]{display:block;max-width:100%;color:#cbd5e1}
.traffic pre{white-space:pre-wrap;word-break:break-word;background:#030712;color:#d1fae5;padding:1rem;max-height:60vh;overflow:auto;border-radius:6px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:1rem}
small,.muted{color:#9ca3af}
.debug-console{padding:1rem;background:#1f2937;border:1px solid #374151;border-radius:8px;margin:1rem 0}
textarea{display:block;width:100%;height:240px;resize:vertical;overflow:auto;background:#273449;color:#e5e7eb;border:1px solid #374151;border-radius:6px;padding:.8rem;font: .9rem/1.4 Consolas,monospace}
progress{display:block;width:100%;height:1.4rem;margin:1rem 0;accent-color:#38bdf8}
@media(max-width:600px){body{margin:1rem auto}}
)rawliteral";

const char HOME_PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Meter Emulator</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<h1>Smart Meter Emulator</h1>
<small>Software build: <span id="buildVersion">Loading...</span></small>
<nav><a href="/">Overview</a><a href="/traffic">RS485 Traffic</a><a href="/ota">Firmware Update</a></nav>
<div id="status" class="status">Loading data...</div>
<div class="controls">
<strong>Debug:</strong>
<button id="debugOn" onclick="setMode('debug',true)">Debug on</button>
<button id="debugOff" onclick="setMode('debug',false)">Debug off</button>
<span id="modes"><small></small></span>
</div>
<section class="debug-console">
<h2>Debug Output</h2>
<textarea id="debugLog" readonly aria-label="Debug Output" wrap="off">Loading debug output...</textarea>
</section>
<div class="grid">
<section><h2>Huawei DTSU666-HW</h2><table><thead><tr><th>Measurement</th><th>Raw value</th><th>Physical value</th></tr></thead><tbody id="huawei"></tbody></table></section>
<section><h2>GoodWe Registers</h2><table><thead><tr><th>Register</th><th>Raw value</th><th>Physical value</th></tr></thead><tbody id="goodwe"></tbody></table></section>
</div>
<script>
function row(name,raw,value){return `<tr><td>${name}</td><td>${raw}</td><td>${value}</td></tr>`;}
async function refresh(){
 try{const d=await (await fetch('/api/data')).json();
  document.getElementById('buildVersion').textContent=d.build_version;
  document.getElementById('status').textContent=`Cache: ${d.cache_valid?'valid':'invalid'} | Age: ${d.cache_age_ms} ms | Ethernet: ${d.ethernet}`;
  document.getElementById('status').className='status'+(d.cache_valid?'':' error');
    document.getElementById('modes').innerHTML=`<small>Debug: ${d.debug?'on':'off'} | RS485 traffic: ${d.traffic?'on':'off'}</small>`;
  document.getElementById('debugOn').classList.toggle('selected',d.debug);
  document.getElementById('debugOff').classList.toggle('selected',!d.debug);
  document.getElementById('huawei').innerHTML=
   row('U L1',d.huawei.ua_raw,d.huawei.ua_v.toFixed(2)+' V')+
   row('U L2',d.huawei.ub_raw,d.huawei.ub_v.toFixed(2)+' V')+
   row('U L3',d.huawei.uc_raw,d.huawei.uc_v.toFixed(2)+' V')+
   row('I L1',d.huawei.ia_raw,d.huawei.ia_a.toFixed(2)+' A')+
   row('I L2',d.huawei.ib_raw,d.huawei.ib_a.toFixed(2)+' A')+
   row('I L3',d.huawei.ic_raw,d.huawei.ic_a.toFixed(2)+' A')+
   row('P L1',d.huawei.pa_raw,d.huawei.pa_w+' W')+
   row('P L2',d.huawei.pb_raw,d.huawei.pb_w+' W')+
   row('P L3',d.huawei.pc_raw,d.huawei.pc_w+' W')+
   row('P total',d.huawei.ptotal_raw,d.huawei.ptotal_w+' W');
  const registerNames={
   306:'U L1',307:'U L2',308:'U L3',
   309:'I L1 High',310:'I L1 Low',
   311:'I L2 High',312:'I L2 Low',
   313:'I L3 High',314:'I L3 Low',
   315:'P L1 Sign',316:'P L1 Value',
   317:'P L2 Sign',318:'P L2 Value',
   319:'P L3 Sign',320:'P L3 Value',
   321:'P total sign',322:'P total value'
  };
  document.getElementById('goodwe').innerHTML=d.goodwe.map((v,i)=>{
   const groupStart=[3,5,7,9,11,13,15].includes(i);
   const groupContinuation=[4,6,8,10,12,14,16].includes(i);
   const physicalCell=groupContinuation?'':`<td${groupStart?' rowspan="2"':''}>${d.goodwe_physical[i]}</td>`;
   const register=306+i;
   const registerName=registerNames[register]||'unmapped';
   return `<tr><td>${register} (${registerName})</td><td>${v}</td>${physicalCell}</tr>`;
  }).join('');
 }catch(e){document.getElementById('status').textContent='Unable to connect to the gateway';document.getElementById('status').className='status error';}
}
async function refreshDebugLog(){
 try{
  const response=await fetch('/api/debug-log');
  if(!response.ok)throw new Error('HTTP error');
  const data=await response.json();
  const box=document.getElementById('debugLog');
  const wasAtBottom=box.scrollTop+box.clientHeight>=box.scrollHeight-8;
  const text=data.entries.join('\n');
  if(box.value!==text){
   box.value=text;
   if(wasAtBottom)box.scrollTop=box.scrollHeight;
  }
 }catch(e){}
}
async function setMode(mode,enabled){
 try{
  const response=await fetch(`/api/debug?mode=${mode}&enabled=${enabled?'1':'0'}`,{method:'POST'});
  if(!response.ok)throw new Error('Failed to apply setting');
 }catch(e){alert('Failed to apply setting');}
 refresh();
 refreshDebugLog();
}
refresh();refreshDebugLog();setInterval(refresh,1000);setInterval(refreshDebugLog,1000);
</script>
</body></html>
)rawliteral";

const char TRAFFIC_PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RS485 Traffic - Smart Meter Emulator</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<h1>RS485 Traffic Capture</h1>
<nav><a href="/">Overview</a><a href="/traffic">RS485 Traffic</a><a href="/ota">Firmware Update</a></nav>
<section class="card">
<p>Enable or disable recording and view recently captured messages.</p>
<button id="trafficOn" onclick="setTraffic(true)">Traffic on</button>
<button id="trafficOff" onclick="setTraffic(false)">Traffic off</button>
<button onclick="clearTraffic()">Clear capture</button>
<p id="trafficStatus" class="muted">Loading status...</p>
<p class="muted">The buffer holds up to 64 messages; older entries are overwritten.</p>
<div class="traffic"><pre id="trafficLog">Loading traffic capture...</pre></div>
</section>
<script>
let trafficInitialized=false;
async function refresh(refreshTrafficLog=false){
 try{
   const dataResponse=await fetch('/api/data');
   if(!dataResponse.ok)throw new Error('HTTP error');
   const data=await dataResponse.json();
   document.getElementById('trafficStatus').textContent=`Recording: ${data.traffic?'on':'off'}`;
   document.getElementById('trafficOn').classList.toggle('selected',data.traffic);
   document.getElementById('trafficOff').classList.toggle('selected',!data.traffic);
   if(data.traffic||!trafficInitialized||refreshTrafficLog){
     const trafficResponse=await fetch('/api/traffic');
     if(!trafficResponse.ok)throw new Error('HTTP error');
     const entries=await trafficResponse.json();
     const trafficLog=document.getElementById('trafficLog');
     const wasAtBottom=!trafficInitialized||
       trafficLog.scrollTop+trafficLog.clientHeight>=trafficLog.scrollHeight-8;
     trafficLog.textContent=entries.length?entries.map(e=>`T+${(e.timestamp_ms/1000).toFixed(3)} s | ${e.interface} ${e.direction} | ${e.hex}`).join('\n'):'No messages recorded';
     if(wasAtBottom)trafficLog.scrollTop=trafficLog.scrollHeight;
     trafficInitialized=true;
   }
 }catch(e){document.getElementById('trafficStatus').textContent='Unable to connect to the gateway';}
}
async function setTraffic(enabled){
 try{
   const response=await fetch(`/api/debug?mode=traffic&enabled=${enabled?'1':'0'}`,{method:'POST'});
   if(!response.ok)throw new Error('Failed to apply setting');
   await refresh(true);
 }catch(e){document.getElementById('trafficStatus').textContent='Failed to apply traffic setting';}
}
async function clearTraffic(){
 try{
   const response=await fetch('/api/traffic/clear',{method:'POST'});
   if(!response.ok)throw new Error('Failed to clear capture');
   await refresh(true);
 }catch(e){document.getElementById('trafficStatus').textContent='Failed to clear capture';}
}
refresh();setInterval(refresh,1000);
</script>
</body></html>
)rawliteral";

const char OTA_PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Firmware Update - Smart Meter Emulator</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<h1>Firmware Update</h1>
<nav><a href="/">Overview</a><a href="/traffic">RS485 Traffic</a><a href="/ota">Firmware Update</a></nav>
<section class="card">
<p>Installed version: <strong id="buildVersion">Loading...</strong></p>
<form id="updateForm">
<label for="firmware">Firmware file (.bin)</label>
<input id="firmware" type="file" accept=".bin" required>
<button id="updateButton" class="primary" type="submit">Update firmware</button>
</form>
<progress id="uploadProgress" max="100" value="0"></progress>
<p id="uploadStatus" role="status" aria-live="polite">Select a firmware file to start the update.</p>
<small>The ESP32 will restart after a successful update.</small>
</section>
<script>
fetch('/api/data').then(response=>{
 if(!response.ok)throw new Error('Failed to load build version');
 return response.json();
}).then(data=>{document.getElementById('buildVersion').textContent=data.build_version;})
 .catch(()=>{document.getElementById('buildVersion').textContent='Unavailable';});
document.getElementById('updateForm').addEventListener('submit',event=>{
 event.preventDefault();
 const file=document.getElementById('firmware').files[0];
 if(!file)return;
 const progress=document.getElementById('uploadProgress');
 const status=document.getElementById('uploadStatus');
 const button=document.getElementById('updateButton');
 const request=new XMLHttpRequest();
 const form=new FormData();
 form.append('firmware',file,file.name);
 button.disabled=true;
 progress.value=0;
 status.textContent='Uploading firmware...';
 request.upload.addEventListener('progress',event=>{
   if(event.lengthComputable){
    const percent=Math.round(event.loaded/event.total*100);
    progress.value=percent;
    status.textContent=`Upload: ${percent}% (${event.loaded} / ${event.total} Bytes)`;
   }
 });
 request.addEventListener('load',()=>{
   status.textContent=request.status>=200&&request.status<300?request.responseText:`Update failed: ${request.responseText||request.statusText}`;
   if(request.status>=200&&request.status<300)progress.value=100;
   button.disabled=false;
 });
 request.addEventListener('error',()=>{
   status.textContent='Connection error during firmware upload.';
   button.disabled=false;
 });
 request.addEventListener('abort',()=>{
   status.textContent='Firmware upload aborted.';
   button.disabled=false;
 });
 request.open('POST','/update');
 request.send(form);
});
</script>
</body></html>
)rawliteral";
}

GatewayWebServer::GatewayWebServer(
    ModbusGateway &modbusGateway,
    GatewayLogger &logger)
    : server_(WEB_SERVER_PORT),
      ethernet_(),
      modbusGateway_(modbusGateway),
      logger_(logger),
      serverReady_(false),
      otaRestartPending_(false)
{
}

void GatewayWebServer::begin()
{
    if (!ethernet_.begin())
    {
        GatewayDebug::println("Failed to start Ethernet.");
    }

    configureRoutes();
    server_.begin();
    serverReady_ = true;
    GatewayDebug::println("HTTP server started (Ethernet, port 80).");
}

void GatewayWebServer::handle()
{
    if (serverReady_)
        server_.handleClient();

    ethernet_.handle();

    if (otaRestartPending_)
    {
        delay(100);
        ESP.restart();
    }
}

String GatewayWebServer::dataJson() const
{
    const MeterData meter = modbusGateway_.meterData();
    const int32_t pTotal = meter.pa + meter.pb + meter.pc;
    uint16_t goodWeRegisters[GoodWeInverter::REG_COUNT] = {};
    if (meter.valid)
    {
        GoodWeInverter::buildRegisters(meter, goodWeRegisters);
    }

    String json = "{\"build_version\":\"";
    json += Gateway::SOFTWARE_BUILD_TIMESTAMP;
    json += "\",\"cache_valid\":";
    json += meter.valid ? "true" : "false";
    json += ",\"cache_age_ms\":";
    json += meter.valid ? millis() - meter.timestamp : 0;
    json += ",\"ethernet\":\"";
    json += ethernet_.localIP().toString();
    json += "\",\"debug\":";
    json += logger_.debugEnabled() ? "true" : "false";
    json += ",\"traffic\":";
    json += logger_.trafficEnabled() ? "true" : "false";
    json += ",\"huawei\":{";
    json += "\"ua_raw\":" + String(meter.ua);
    json += ",\"ub_raw\":" + String(meter.ub);
    json += ",\"uc_raw\":" + String(meter.uc);
    json += ",\"ia_raw\":" + String(meter.ia);
    json += ",\"ib_raw\":" + String(meter.ib);
    json += ",\"ic_raw\":" + String(meter.ic);
    json += ",\"pa_raw\":" + String(meter.pa);
    json += ",\"pb_raw\":" + String(meter.pb);
    json += ",\"pc_raw\":" + String(meter.pc);
    json += ",\"ua_v\":" + String(meter.ua / DTSU666HW_VOLTAGE_SCALE, 2);
    json += ",\"ub_v\":" + String(meter.ub / DTSU666HW_VOLTAGE_SCALE, 2);
    json += ",\"uc_v\":" + String(meter.uc / DTSU666HW_VOLTAGE_SCALE, 2);
    json += ",\"ia_a\":" + String(meter.ia / DTSU666HW_CURRENT_SCALE, 2);
    json += ",\"ib_a\":" + String(meter.ib / DTSU666HW_CURRENT_SCALE, 2);
    json += ",\"ic_a\":" + String(meter.ic / DTSU666HW_CURRENT_SCALE, 2);
    json += ",\"pa_w\":" + String(meter.pa);
    json += ",\"pb_w\":" + String(meter.pb);
    json += ",\"pc_w\":" + String(meter.pc);
    json += ",\"ptotal_raw\":" + String(pTotal);
    json += ",\"ptotal_w\":" + String(pTotal);
    json += "},\"goodwe\":[";
    for (uint16_t index = 0;
         index < GoodWeInverter::REG_COUNT;
         ++index)
    {
        if (index > 0)
        {
            json += ',';
        }
        json += String(goodWeRegisters[index]);
    }
    json += "],\"goodwe_physical\":[";
    for (uint16_t index = 0;
         index < GoodWeInverter::REG_COUNT;
         ++index)
    {
        if (index > 0)
        {
            json += ',';
        }
        json += '"';
        json += goodWePhysicalValue(goodWeRegisters, index);
        json += '"';
    }
    json += "]}";
    return json;
}

void GatewayWebServer::configureRoutes()
{
    server_.on("/", HTTP_GET, [this]()
    {
        server_.send_P(200, "text/html; charset=utf-8", HOME_PAGE);
    });
    server_.on("/traffic", HTTP_GET, [this]()
    {
        server_.send_P(200, "text/html; charset=utf-8", TRAFFIC_PAGE);
    });
    server_.on("/ota", HTTP_GET, [this]()
    {
        server_.send_P(200, "text/html; charset=utf-8", OTA_PAGE);
    });
    server_.on("/style.css", HTTP_GET, [this]()
    {
        server_.send_P(200, "text/css; charset=utf-8", WEB_STYLE);
    });
    server_.on("/api/debug-log", HTTP_GET, [this]()
    {
        server_.send(200, "application/json", GatewayDebug::logJson());
    });
    server_.on("/api/data", HTTP_GET, [this]()
    {
        server_.send(200, "application/json", dataJson());
    });
    server_.on("/api/traffic", HTTP_GET, [this]()
    {
        server_.send(200, "application/json", logger_.trafficJson());
    });
    server_.on("/api/traffic/clear", HTTP_POST, [this]()
    {
        logger_.clearTraffic();
        server_.send(200, "text/plain", "Traffic capture cleared");
    });
    server_.on("/api/debug", HTTP_POST, [this]()
    {
        if (!server_.hasArg("mode") || !server_.hasArg("enabled"))
        {
            server_.send(400, "text/plain", "mode and enabled are required");
            return;
        }

        const bool enabled = server_.arg("enabled") == "1";
        const String mode = server_.arg("mode");
        if (mode == "debug")
        {
            logger_.setDebugEnabled(enabled);
        }
        else if (mode == "traffic")
        {
            logger_.setTrafficEnabled(enabled);
        }
        else
        {
            server_.send(400, "text/plain", "Unknown mode");
            return;
        }
        server_.send(200, "application/json", dataJson());
    });
    server_.on("/update", HTTP_POST, [this]()
    {
        const bool updateFailed = Update.hasError();
        server_.send(
            updateFailed ? 500 : 200,
            "text/plain",
            updateFailed
                ? "Update failed"
                : "Update successful; restarting");
        otaRestartPending_ = !updateFailed;
    }, [this]()
    {
        HTTPUpload &upload = server_.upload();
        if (upload.status == UPLOAD_FILE_START)
        {
            otaRestartPending_ = false;
            if (!Update.begin(UPDATE_SIZE_UNKNOWN))
            {
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
            if (!Update.end(true))
            {
                Update.printError(Serial);
            }
        }
    });
}

} // namespace Gateway
