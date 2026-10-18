# Home Assistant deployment

This deployment runs Home Assistant behind Caddy HTTPS and connects outward to
the managed MQTT broker. It does not expose an MQTT port from the Oracle VM.

1. Point a DNS name at the Oracle VM and allow inbound TCP 80 and 443.
2. Copy `.env.example` to `.env` and set `PUBLIC_HOST`.
3. Run `docker compose up -d`.
4. Complete Home Assistant onboarding through the HTTPS hostname.
5. Add the MQTT integration using the dedicated Home Assistant HiveMQ
   credential and TLS port 8883.

The included package targets the default device ID `desktop-01`. Change every
topic and unique ID before startup if the bot uses another device ID. It creates
online-state, CPU/GPU/load/RAM/network/uptime entities, speed-test results,
latest-alert state, safe non-retained QoS 1 command scripts, reset buttons, a
template power switch, and a YAML command-center dashboard. Power-off is still
executed by the Windows or Linux agent on the PC.

Copy `.env.example` to `.env`; never edit the example with real values. The
optional `google_assistant.example.yaml` is intentionally not loaded by default
because it requires a user-owned Google Cloud project and service-account file.

Do not commit `.env`, Home Assistant `.storage`, database files, broker
credentials, or private keys.
