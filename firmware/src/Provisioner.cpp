#include "Provisioner.h"

#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <WiFi.h>

#include "BoardConfig.h"

namespace {
Provisioner* owner;
NimBLECharacteristic* statusCharacteristic;
String incomingConfig;
bool configOverflow = false;
constexpr size_t kMaxConfigBytes = 16384;

const char kPortalHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Pico Assistant Setup</title>
  <style>
    :root{color-scheme:dark;--bg:#050914;--card:#0b1424;--edge:#233754;--cyan:#25e6ff;--green:#4dff9a;--pink:#ff3cac;--muted:#8ea1b9}
    *{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,#10233b 0,var(--bg) 48%);font:15px system-ui,sans-serif;color:#eef7ff;min-height:100vh}
    main{width:min(720px,100%);margin:auto;padding:24px 16px 48px}.brand{display:flex;align-items:center;gap:12px;margin-bottom:20px}
    .mark{width:6px;height:38px;background:var(--cyan);box-shadow:0 0 18px var(--cyan)}h1{margin:0;font-size:25px;letter-spacing:.08em}.tag{color:var(--cyan)}
    .status{padding:12px 14px;border:1px solid var(--edge);border-radius:12px;background:#08101dcc;color:var(--muted);margin-bottom:16px}
    section{background:linear-gradient(145deg,#0d192b,#08111f);border:1px solid var(--edge);border-radius:16px;padding:18px;margin:14px 0;box-shadow:0 14px 40px #0007}
    h2{font-size:14px;letter-spacing:.16em;color:var(--cyan);margin:0 0 15px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:13px}
    label{display:block;color:var(--muted);font-size:12px;letter-spacing:.04em}input,textarea{width:100%;margin-top:6px;border:1px solid #29415f;border-radius:9px;background:#050b14;color:#fff;padding:12px;outline:none}
    input:focus,textarea:focus{border-color:var(--cyan);box-shadow:0 0 0 2px #25e6ff22}textarea{min-height:112px;resize:vertical}
    button{border:0;border-radius:10px;padding:12px 16px;font-weight:750;letter-spacing:.05em;cursor:pointer}.scan{margin-top:10px;background:#182b43;color:var(--cyan)}
    .save{width:100%;background:linear-gradient(90deg,var(--cyan),var(--green));color:#031018;box-shadow:0 0 24px #25e6ff44}.save:disabled{opacity:.55}
    .hint{color:var(--muted);font-size:12px;line-height:1.5}.result{display:none;margin-top:14px;padding:12px;border-radius:9px}.ok{display:block;background:#0c3528;color:var(--green)}.bad{display:block;background:#3b1720;color:#ff8298}
    @media(max-width:560px){.grid{grid-template-columns:1fr}main{padding-top:16px}}
  </style>
</head>
<body><main>
  <div class="brand"><div class="mark"></div><div><h1>PICO <span class="tag">ASSISTANT</span></h1><div class="hint">SECURE DEVICE ONBOARDING</div></div></div>
  <div class="status">Connected directly to the assistant. Configure the destination Wi-Fi and private MQTT account, then the device will restart.</div>
  <form id="setup">
    <section><h2>01 // WI-FI LINK</h2><div class="grid">
      <label>Network name (SSID)<input id="wifiSsid" name="wifiSsid" list="networks" autocomplete="off" required></label>
      <label>Wi-Fi password<input name="wifiPassword" type="password" autocomplete="new-password"></label>
    </div><datalist id="networks"></datalist><button class="scan" id="scan" type="button">SCAN NETWORKS</button></section>
    <section><h2>02 // DEVICE</h2><div class="grid">
      <label>Device ID<input name="deviceId" value="desktop-01" required pattern="[A-Za-z0-9_-]{2,32}"></label>
      <label>PC MAC address<input name="pcMac" placeholder="AA:BB:CC:DD:EE:FF" required></label>
      <label>LAN broadcast IP<input name="broadcastIp" value="255.255.255.255" required></label>
      <label>Time zone (clock screen)<input name="timezone" list="zones" value="UTC0" required></label>
    </div><datalist id="zones">
      <option value="UTC0">UTC</option>
      <option value="GMT0BST,M3.5.0/1,M10.5.0">United Kingdom</option>
      <option value="CET-1CEST,M3.5.0,M10.5.0/3">Central Europe</option>
      <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Eastern Europe</option>
      <option value="EST5EDT,M3.2.0,M11.1.0">US Eastern</option>
      <option value="CST6CDT,M3.2.0,M11.1.0">US Central</option>
      <option value="PST8PDT,M3.2.0,M11.1.0">US Pacific</option>
      <option value="IST-5:30">India</option>
      <option value="&lt;+06&gt;-6">Bangladesh</option>
      <option value="&lt;+04&gt;-4">Gulf</option>
      <option value="CST-8">China</option>
      <option value="JST-9">Japan</option>
      <option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Sydney</option>
    </datalist></section>
    <section><h2>03 // MQTT CLOUD</h2><div class="grid">
      <label>Broker host<input name="mqttHost" placeholder="cluster.hivemq.cloud" required></label>
      <label>TLS port<input name="mqttPort" type="number" value="8883" min="1" max="65535" required></label>
      <label>MQTT username<input name="mqttUsername" autocomplete="username" required></label>
      <label>MQTT password<input name="mqttPassword" type="password" autocomplete="new-password"></label>
    </div><label style="margin-top:13px">Broker root CA certificate (PEM)<textarea name="rootCa" placeholder="-----BEGIN CERTIFICATE-----"></textarea></label>
    <p class="hint">Use a dedicated bot credential. During Wi-Fi recovery, leave the MQTT password and CA blank to preserve their stored values.</p></section>
    <button class="save" id="save" type="submit">SAVE CONFIGURATION &amp; RESTART</button><div id="result" class="result"></div>
  </form>
  <script>
    const form=document.querySelector('#setup'),result=document.querySelector('#result'),save=document.querySelector('#save');
    fetch('/api/config').then(r=>r.json()).then(c=>Object.entries(c).forEach(([k,v])=>{const e=form.elements[k];if(e&&v!==null&&v!=='')e.value=v})).catch(()=>{});
    document.querySelector('#scan').onclick=async e=>{e.target.disabled=true;e.target.textContent='SCANNING...';try{let a=null;for(let i=0;i<24;i++){const r=await fetch('/api/scan');if(r.status===202){await new Promise(v=>setTimeout(v,500));continue}if(!r.ok)throw Error('scan failed');a=await r.json();break}if(!a)throw Error('scan timeout');const d=document.querySelector('#networks');d.innerHTML='';a.networks.forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.label=n.rssi+' dBm '+(n.secure?'secured':'open');d.appendChild(o)});result.className='result ok';result.textContent=a.networks.length+' networks found. Select or type your SSID.'}catch(x){result.className='result bad';result.textContent='Scan failed. Enter the SSID manually.'}finally{e.target.disabled=false;e.target.textContent='SCAN NETWORKS'}};
    form.onsubmit=async e=>{e.preventDefault();save.disabled=true;save.textContent='VALIDATING...';result.className='result';try{const r=await fetch('/api/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(new FormData(form))});const j=await r.json();if(!r.ok)throw Error(j.message||j.status);result.className='result ok';result.textContent='Saved. The bot is restarting and this setup network will close.';save.textContent='CONFIGURATION SAVED'}catch(x){result.className='result bad';result.textContent=x.message;save.disabled=false;save.textContent='SAVE CONFIGURATION & RESTART'}};
  </script>
</main></body></html>
)HTML";

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    String packet = characteristic->getValue().c_str();
    if (packet == "BEGIN") {
      incomingConfig = "";
      configOverflow = false;
      statusCharacteristic->setValue("receiving");
      statusCharacteristic->notify();
      return;
    }
    if (packet != "END") {
      if (incomingConfig.length() + packet.length() > kMaxConfigBytes) {
        configOverflow = true;
      } else if (!configOverflow) {
        incomingConfig += packet;
      }
      return;
    }
    if (configOverflow) {
      incomingConfig = "";
      statusCharacteristic->setValue("config-too-large");
      statusCharacteristic->notify();
      return;
    }
    String status;
    owner->applyConfigJson(incomingConfig, status);
    incomingConfig = "";
    statusCharacteristic->setValue(status.c_str());
    statusCharacteristic->notify();
  }
};
}

String Provisioner::setupUrl() const {
  return "http://" + WiFi.softAPIP().toString();
}

bool Provisioner::applyConfigJson(const String& json, String& status) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    status = "invalid-json";
    return false;
  }
  config_->deviceId = doc["deviceId"] | config_->deviceId;
  config_->wifiSsid = doc["wifiSsid"] | "";
  config_->wifiPassword = doc["wifiPassword"] | "";
  config_->mqttHost = doc["mqttHost"] | "";
  const int mqttPort = doc["mqttPort"] | static_cast<int>(config_->mqttPort);
  if (mqttPort < 1 || mqttPort > 65535) {
    status = "invalid-config";
    return false;
  }
  config_->mqttPort = static_cast<uint16_t>(mqttPort);
  config_->mqttUsername = doc["mqttUsername"] | "";
  config_->mqttPassword = doc["mqttPassword"] | config_->mqttPassword;
  config_->pcMac = doc["pcMac"] | "";
  config_->broadcastIp = doc["broadcastIp"] | "255.255.255.255";
  config_->rootCa = doc["rootCa"] | config_->rootCa;
  config_->timezone = doc["timezone"] | config_->timezone;
  if (config_->timezone.isEmpty()) config_->timezone = "UTC0";
  const String err = config_->validationError();
  if (!err.isEmpty()) {
    status = err;
    return false;
  }
  if (!store_->save(*config_)) {
    status = "Save failed — check NVS storage";
    return false;
  }
  status = "saved-rebooting";
  rebootRequested_ = true;
  return true;
}

void Provisioner::startAccessPoint() {
  const uint64_t chipId = ESP.getEfuseMac();
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06llX",
           static_cast<unsigned long long>(chipId & 0xFFFFFF));
  char secret[9];
  snprintf(secret, sizeof(secret), "%08lX",
           static_cast<unsigned long>(esp_random()));
  apSsid_ = String(BoardConfig::kSetupApPrefix) + String(suffix);
  apPassword_ = "PICO-" + String(secret);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid_.c_str(), apPassword_.c_str(), 1, false, 2);
  delay(100);
  dnsServer_.start(53, "*", WiFi.softAPIP());
}

void Provisioner::handleConfig() {
  JsonDocument doc;
  doc["deviceId"] = config_->deviceId;
  doc["wifiSsid"] = config_->wifiSsid;
  doc["mqttHost"] = config_->mqttHost;
  doc["mqttPort"] = config_->mqttPort;
  doc["mqttUsername"] = config_->mqttUsername;
  doc["pcMac"] = config_->pcMac;
  doc["broadcastIp"] = config_->broadcastIp;
  doc["timezone"] = config_->timezone;
  String payload;
  serializeJson(doc, payload);
  webServer_.send(200, "application/json", payload);
}

void Provisioner::handleScan() {
  int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true, false, false, 120);
    webServer_.send(202, "application/json", "{\"status\":\"scanning\"}");
    return;
  }
  if (count == WIFI_SCAN_RUNNING) {
    webServer_.send(202, "application/json", "{\"status\":\"scanning\"}");
    return;
  }

  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();
  for (int index = 0; index < count && index < 20; ++index) {
    JsonObject network = networks.add<JsonObject>();
    network["ssid"] = WiFi.SSID(index);
    network["rssi"] = WiFi.RSSI(index);
    network["secure"] = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  String payload;
  serializeJson(doc, payload);
  webServer_.send(200, "application/json", payload);
}

void Provisioner::handleSave() {
  String mqttPassword = webServer_.arg("mqttPassword");
  String rootCa = webServer_.arg("rootCa");
  if (mqttPassword.isEmpty()) mqttPassword = config_->mqttPassword;
  if (rootCa.isEmpty()) rootCa = config_->rootCa;

  JsonDocument doc;
  doc["deviceId"] = webServer_.arg("deviceId");
  doc["wifiSsid"] = webServer_.arg("wifiSsid");
  doc["wifiPassword"] = webServer_.arg("wifiPassword");
  doc["mqttHost"] = webServer_.arg("mqttHost");
  doc["mqttPort"] = webServer_.arg("mqttPort").toInt();
  doc["mqttUsername"] = webServer_.arg("mqttUsername");
  doc["mqttPassword"] = mqttPassword;
  doc["pcMac"] = webServer_.arg("pcMac");
  doc["broadcastIp"] = webServer_.arg("broadcastIp");
  doc["rootCa"] = rootCa;
  doc["timezone"] = webServer_.arg("timezone");

  String json;
  serializeJson(doc, json);
  String status;
  if (!applyConfigJson(json, status)) {
    JsonDocument errDoc;
    errDoc["status"] = "invalid-config";
    errDoc["message"] = status;
    String response;
    serializeJson(errDoc, response);
    webServer_.send(400, "application/json", response);
    return;
  }
  webServer_.send(200, "application/json",
                  "{\"status\":\"saved-rebooting\"}");
}

void Provisioner::redirectToPortal() {
  webServer_.sendHeader("Location", setupUrl(), true);
  webServer_.send(302, "text/plain", "");
}

void Provisioner::startWebPortal() {
  webServer_.on("/", HTTP_GET, [this]() {
    webServer_.send_P(200, "text/html", kPortalHtml);
  });
  webServer_.on("/api/config", HTTP_GET, [this]() { handleConfig(); });
  webServer_.on("/api/scan", HTTP_GET, [this]() { handleScan(); });
  webServer_.on("/api/save", HTTP_POST, [this]() { handleSave(); });
  webServer_.on("/generate_204", HTTP_GET, [this]() { redirectToPortal(); });
  webServer_.on("/hotspot-detect.html", HTTP_GET,
                [this]() { redirectToPortal(); });
  webServer_.on("/ncsi.txt", HTTP_GET, [this]() { redirectToPortal(); });
  webServer_.onNotFound([this]() { redirectToPortal(); });
  webServer_.begin();
}

void Provisioner::begin(DeviceConfig* config, ConfigStore* store) {
  config_ = config;
  store_ = store;
  owner = this;
  startAccessPoint();
  startWebPortal();
  Serial.printf("[setup] AP %s active at %s\n", apSsid_.c_str(),
                setupUrl().c_str());

  NimBLEDevice::init((String(BoardConfig::kProductFullName) + " " +
                      config->deviceId)
                         .c_str());
  NimBLEDevice::setSecurityAuth(true, true, true);
  auto* server = NimBLEDevice::createServer();
  auto* service = server->createService(BoardConfig::kBleServiceUuid);
  auto* configCharacteristic = service->createCharacteristic(
      BoardConfig::kBleConfigUuid, NIMBLE_PROPERTY::WRITE_ENC);
  statusCharacteristic = service->createCharacteristic(
      BoardConfig::kBleStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  configCharacteristic->setCallbacks(new ConfigCallbacks());
  statusCharacteristic->setValue("waiting");
  auto* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(BoardConfig::kBleServiceUuid);
  advertising->enableScanResponse(true);
  advertising->start();
  active_ = true;
}

void Provisioner::tick() {
  if (!active_) return;
  dnsServer_.processNextRequest();
  webServer_.handleClient();
}

void Provisioner::stop() {
  if (!active_) return;
  webServer_.stop();
  dnsServer_.stop();
  WiFi.softAPdisconnect(true);
  NimBLEDevice::deinit(true);
  active_ = false;
}
