#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <functional>

// LAN firmware updates. The desktop app POSTs a firmware.bin here; the image is
// written to the inactive OTA slot and only becomes active after it verifies,
// so a failed transfer leaves the running firmware untouched.
//
// The endpoint is authenticated with a per-device token: without one, anyone on
// the network could replace the firmware, which is a complete takeover.
class OtaServer {
 public:
  // Called with 0..100 while an upload is running, so the UI can show progress.
  using ProgressHandler = std::function<void(uint8_t)>;

  void begin(const String& token, const String& version, const String& deviceId,
             ProgressHandler onProgress);
  void tick();
  void stop();
  bool active() const { return uploading_; }

 private:
  void handleInfo();
  void handleUpdatePost();
  void handleUpdateUpload();
  bool authorised();

  WebServer server_{80};
  String token_;
  String version_;
  String deviceId_;
  ProgressHandler onProgress_;
  size_t expectedBytes_ = 0;
  size_t receivedBytes_ = 0;
  bool uploading_ = false;
  bool started_ = false;
};
