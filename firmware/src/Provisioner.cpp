#include "Provisioner.h"

#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include "BoardConfig.h"

namespace {
Provisioner* owner;
NimBLECharacteristic* statusCharacteristic;
String incomingConfig;

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    String packet = characteristic->getValue().c_str();
    if (packet == "BEGIN") {
      incomingConfig = "";
      statusCharacteristic->setValue("receiving");
      statusCharacteristic->notify();
      return;
    }
    if (packet != "END") {
      incomingConfig += packet;
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
  config_->mqttPort = doc["mqttPort"] | 8883;
  config_->mqttUsername = doc["mqttUsername"] | "";
  config_->mqttPassword = doc["mqttPassword"] | "";
  config_->pcMac = doc["pcMac"] | "";
  config_->broadcastIp = doc["broadcastIp"] | "255.255.255.255";
  config_->rootCa = doc["rootCa"] | "";
  if (!config_->ready() || !store_->save(*config_)) {
    status = "invalid-config";
    return false;
  }
  status = "saved-rebooting";
  rebootRequested_ = true;
  return true;
}

void Provisioner::begin(DeviceConfig* config, ConfigStore* store) {
  config_ = config;
  store_ = store;
  owner = this;
  NimBLEDevice::init(("PC Bot " + config->deviceId).c_str());
  NimBLEDevice::setSecurityAuth(true, true, true);
  auto* server = NimBLEDevice::createServer();
  auto* service = server->createService(BoardConfig::kBleServiceUuid);
  auto* configCharacteristic = service->createCharacteristic(
      BoardConfig::kBleConfigUuid, NIMBLE_PROPERTY::WRITE_ENC);
  statusCharacteristic = service->createCharacteristic(
      BoardConfig::kBleStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  configCharacteristic->setCallbacks(new ConfigCallbacks());
  statusCharacteristic->setValue("waiting");
  service->start();
  auto* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(BoardConfig::kBleServiceUuid);
  advertising->enableScanResponse(true);
  advertising->start();
  active_ = true;
}

void Provisioner::stop() {
  if (!active_) return;
  NimBLEDevice::deinit(true);
  active_ = false;
}
