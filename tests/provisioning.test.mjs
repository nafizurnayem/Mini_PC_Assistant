import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const provisioner = await readFile(
  new URL('../firmware/src/Provisioner.cpp', import.meta.url), 'utf8');
const config = await readFile(
  new URL('../firmware/include/DeviceConfig.h', import.meta.url), 'utf8');
const main = await readFile(
  new URL('../firmware/src/main.cpp', import.meta.url), 'utf8');

test('browser provisioning uses a protected temporary access point', () => {
  assert.match(provisioner, /esp_random\(\)/);
  assert.match(provisioner, /WiFi\.softAP\(.*false,\s*2\)/s);
  assert.match(provisioner, /dnsServer_\.start\(53,\s*"\*"/);
  assert.match(provisioner, /webServer_\.on\("\/api\/save",\s*HTTP_POST/);
  assert.match(provisioner, /WiFi\.scanNetworks\(true/);
  assert.match(provisioner, /WIFI_SCAN_RUNNING/);
  assert.match(provisioner, /WiFi\.softAPdisconnect\(true\)/);
});

test('configuration read API never returns stored secrets', () => {
  const readHandler = provisioner.slice(
    provisioner.indexOf('void Provisioner::handleConfig()'),
    provisioner.indexOf('void Provisioner::handleScan()'));
  assert.doesNotMatch(readHandler, /wifiPassword/);
  assert.doesNotMatch(readHandler, /mqttPassword/);
  assert.doesNotMatch(readHandler, /rootCa/);
});

test('saved configuration validates identity, MAC and TLS CA', () => {
  assert.match(config, /deviceId\.length\(\) > 32/);
  assert.match(config, /BEGIN CERTIFICATE/);
  assert.match(config, /END CERTIFICATE/);
  assert.match(config, /validMac/);
  assert.match(config, /mqttPort == 0/);
});

test('BLE provisioning is bounded and preserves stored MQTT secrets', () => {
  assert.match(provisioner, /kMaxConfigBytes/);
  assert.match(provisioner, /config-too-large/);
  assert.match(provisioner, /doc\["mqttPassword"\]\s*\|\s*config_->mqttPassword/);
  assert.match(provisioner, /doc\["rootCa"\]\s*\|\s*config_->rootCa/);
});

test('unknown reset actions cannot erase the device', () => {
  assert.match(main, /command\.action != "wifi" && command\.action != "factory"/);
  assert.match(main, /Unknown reset action/);
});

test('command timestamps accept UTC fractional seconds safely', () => {
  assert.match(main, /value\[consumed\] == '\.'/);
  assert.match(main, /value\[consumed\] != 'Z'/);
});

test('boot hold provides offline Wi-Fi recovery', () => {
  assert.match(main, /Hold 5s for Wi-Fi setup/);
  assert.match(main, /configStore\.clearWifi\(\)/);
  assert.match(main, /provisioner\.start|provisioner\.begin/);
});
