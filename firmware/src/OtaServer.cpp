#include "OtaServer.h"

#include <ArduinoJson.h>
#include <Update.h>
#include <WiFi.h>

#include "BoardConfig.h"

namespace {
constexpr char kAuthHeader[] = "X-Pico-Token";
}

void OtaServer::begin(const String& token, const String& version,
                      const String& deviceId, ProgressHandler onProgress) {
  token_ = token;
  version_ = version;
  deviceId_ = deviceId;
  onProgress_ = std::move(onProgress);
  if (started_) return;

  // WebServer only retains headers it was told to keep.
  const char* wanted[] = {kAuthHeader};
  server_.collectHeaders(wanted, 1);
  server_.on("/info", HTTP_GET, [this]() { handleInfo(); });
  server_.on(
      "/update", HTTP_POST, [this]() { handleUpdatePost(); },
      [this]() { handleUpdateUpload(); });
  server_.onNotFound([this]() { server_.send(404, "text/plain", "not found"); });
  server_.begin();
  started_ = true;
  Serial.printf("[ota] update server on http://%s/update\n",
                WiFi.localIP().toString().c_str());
}

void OtaServer::tick() {
  if (started_) server_.handleClient();
}

bool OtaServer::authorised() {
  // Constant-time-ish comparison is overkill for a LAN token, but rejecting a
  // missing or short token explicitly is not.
  const String supplied = server_.header(kAuthHeader);
  return !token_.isEmpty() && supplied.length() == token_.length() &&
         supplied == token_;
}

void OtaServer::handleInfo() {
  JsonDocument doc;
  doc["deviceId"] = deviceId_;
  doc["version"] = version_;
  doc["ip"] = WiFi.localIP().toString();
  doc["sketchSize"] = ESP.getSketchSize();
  doc["freeSpace"] = ESP.getFreeSketchSpace();
  doc["authorised"] = authorised();
  String payload;
  serializeJson(doc, payload);
  server_.send(200, "application/json", payload);
}

void OtaServer::handleUpdateUpload() {
  HTTPUpload& upload = server_.upload();

  if (upload.status == UPLOAD_FILE_START) {
    if (!authorised()) {
      // The response is written by the POST handler; refuse to buffer anything.
      uploading_ = false;
      return;
    }
    expectedBytes_ = upload.totalSize > 0 ? upload.totalSize
                                          : ESP.getFreeSketchSpace();
    receivedBytes_ = 0;
    uploading_ = true;
    Serial.printf("[ota] upload started: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      uploading_ = false;
    }
    if (onProgress_) onProgress_(0);
    return;
  }

  if (!uploading_) return;

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
      uploading_ = false;
      return;
    }
    receivedBytes_ += upload.currentSize;
    if (onProgress_ && expectedBytes_ > 0) {
      const uint32_t percent = receivedBytes_ * 100 / expectedBytes_;
      onProgress_(static_cast<uint8_t>(percent > 100 ? 100 : percent));
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[ota] wrote %u bytes, rebooting\n",
                    static_cast<unsigned>(receivedBytes_));
      if (onProgress_) onProgress_(100);
    } else {
      Update.printError(Serial);
      uploading_ = false;
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    uploading_ = false;
    Serial.println("[ota] upload aborted");
  }
}

void OtaServer::handleUpdatePost() {
  if (!authorised()) {
    server_.send(401, "application/json", "{\"error\":\"bad token\"}");
    return;
  }
  const bool ok = !Update.hasError();
  JsonDocument doc;
  doc["ok"] = ok;
  doc["bytes"] = receivedBytes_;
  if (!ok) doc["error"] = "write failed";
  String payload;
  serializeJson(doc, payload);
  server_.send(ok ? 200 : 500, "application/json", payload);

  if (!ok) {
    uploading_ = false;
    return;
  }
  // Let the response reach the desktop app before the reboot drops the socket.
  delay(400);
  ESP.restart();
}
