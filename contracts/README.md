# MQTT contract

Root topic: `pcbot/v1/{deviceId}`. Commands use QoS 1 and must not be
retained. State and telemetry use QoS 1; state messages are retained.

Command envelope:

```json
{
  "commandId": "0bb08c25-4dc2-4da2-a36b-11c20855bf3f",
  "source": "mobile",
  "action": "on",
  "issuedAt": "2026-06-15T12:00:00Z",
  "expiresAt": "2026-06-15T12:00:30Z"
}
```

Acknowledgement:

```json
{
  "commandId": "0bb08c25-4dc2-4da2-a36b-11c20855bf3f",
  "source": "esp32",
  "status": "accepted",
  "message": "Wake-on-LAN packet sent",
  "timestamp": "2026-06-15T12:00:01Z"
}
```

Consumers reject expired commands and remember recent `commandId` values.
An `off` command is only handled by the active PC agent (Windows or Linux).
ESP32 never cuts power. Linux acknowledgements use source `linux-agent`;
the topics and command envelope are shared with Windows.
