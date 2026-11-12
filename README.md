# Raksha — Smart SOS Home Security System

Raksha is a modular home-entry and intrusion-alert prototype built around an ESP32, an AI-Thinker ESP32-CAM, an A9G GSM/GPS board, and a Raspberry Pi 4/5 local face-matching service.

> **Safety status:** This is a development prototype, not a certified security product. Phase 2 uses OpenCV Haar detection + LBPH matching; it does NOT implement liveness or presentation-attack detection. Match-only unlocking is disabled by default. Do not connect a real lock until a validated second factor/liveness mechanism, power integrity, cybersecurity, mechanical override and safe egress have been reviewed. Emergency-service escalation requires human confirmation.

## Hardware

- ESP32 DevKit main controller
- AI-Thinker ESP32-CAM camera endpoint
- Raspberry Pi 4 or 5 for local face matching and resident enrollment
- A9G GSM/GPRS + GPS board for SMS/GPS alerts (board-specific power and AT commands must be verified)
- Protected battery pack, appropriately rated regulators, fuse and low-voltage cutoff
- PIR/door contact, siren driver and optional isolated lock driver

## Architecture

```text
AI-Thinker ESP32-CAM -- private Wi-Fi JPEG --> Raspberry Pi 4/5
                                                | OpenCV face matching
                                                | resident enrollment
                                                | consecutive-frame check
                                                v authenticated LAN request
                                           Main ESP32 DevKit
                                            | GPIO33 lock driver
                                            | GPIO27 intrusion input
                                            | GPIO26 siren driver
                                            | A9G UART (Phase 1 alert firmware)
```

The Pi stores resident face crops locally under its private data directory. No biometric data belongs in Git. The Phase 2 match-only access setting defaults to disabled.

## Repository layout

- `firmware/main_esp32/` — Phase 1 A9G SMS/GPS and intrusion starter.
- `firmware/esp32_cam/` — AI-Thinker ESP32-CAM JPEG endpoint.
- `firmware/phase2_access_controller/` — authenticated Phase 2 ESP32 access endpoint.
- `server/face_service/` — Raspberry Pi OpenCV LBPH service, resident enrollment API and camera polling agent.
- `hardware/WIRING.md` — Phase 1 signal/power guidance.
- `hardware/PHASE2_WIRING.md` — Phase 2 network and pin wiring.
- `docs/TEST_PLAN.md` — bench and safety test plan.
- `docs/PHASE2_SETUP.md` — Raspberry Pi setup, enrollment and commissioning.
- `config/secrets.example.h` — placeholder configuration only.

## Phase 2 quick start

Read [Phase 2 setup](docs/PHASE2_SETUP.md) and [Phase 2 wiring](hardware/PHASE2_WIRING.md). The short version:

1. Flash the AI-Thinker camera sketch and note its private LAN IP.
2. Install `server/face_service/requirements.txt` on Raspberry Pi OS 64-bit.
3. Set separate random `RAKSHA_ADMIN_TOKEN` and `RAKSHA_API_TOKEN` values on the Pi; set matching Wi-Fi/API settings locally in the ESP32 sketch.
4. Configure the Pi's camera and ESP32 URLs and reserve their IP addresses.
5. Enroll only consenting residents using at least five images each.
6. Verify recognition and the ESP32 health endpoint with the lock disconnected.
7. Keep `RAKSHA_ENABLE_MATCH_ONLY_ACCESS=false` until a separately validated liveness or second-factor method is integrated.

The Pi service requires OpenCV-contrib with `cv2.face.LBPHFaceRecognizer_create()`. The ESP32 Phase 2 sketch requires the ArduinoJson library.

## Phase 2 security limitations

- LBPH is a basic face matcher, not a modern high-assurance biometric system.
- Consecutive matching frames are only a temporal consistency check; they do not prove a live person is present and can be defeated by a photograph/video replay.
- Match-only unlocking is disabled by default.
- The starter ESP32 endpoint uses bearer-token HTTP on a private LAN. This is not encrypted transport; isolate the devices on a trusted network. Production deployment should use authenticated TLS or a hardened local gateway.
- The ESP32 resident allow-list is intentionally empty. Configure it explicitly before any bench access test.
- In-memory duplicate-event suppression is not durable replay protection.
- Do not expose camera, Flask or ESP32 endpoints to the public internet.

## Phase 1 electrical safety

A9G breakout boards vary. Verify the exact carrier-board input voltage, UART logic level and GSM transmit-current requirements from its schematic/manual. Never power A9G from the ESP32 3V3 pin. Use a dedicated correctly rated regulator, protection and short low-resistance wiring. All lock/siren loads require properly rated driver stages; never drive them directly from GPIO.

## Privacy and emergency response

Obtain informed consent for biometric enrollment. Keep face images, templates, phone numbers, Wi-Fi credentials, API tokens and home coordinates outside source control. Restrict access, encrypt backups, set a retention policy and support resident deletion. Raksha may notify household contacts; any escalation to authorities must be reviewed and confirmed by a person using locally approved emergency channels.

## Status

Phase 1 hardware/firmware starter and Phase 2 Raspberry Pi face-matching integration are committed. This repository has not been hardware-tested by the maintainer; compile, bench-test and validate on the exact board revisions before installation.

## License

No license has been selected. All rights reserved by the repository owner unless a license is added.
