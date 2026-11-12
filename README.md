# Raksha — Smart SOS Home Security System

Raksha is a modular home-entry and intrusion-alert prototype built around an ESP32, an AI-Thinker ESP32-CAM, and an A9G GSM/GPS board.

> **Safety and deployment status:** This repository is a development starter, not a certified security or emergency-response product. Do not connect it to a real door lock or unattended police-calling workflow until it has been independently tested. Face matching can fail and must not be the only means of egress or emergency access. Police/emergency escalation must be confirmed by a responsible person; the prototype deliberately does not automatically call authorities.

## Planned capabilities

- Resident enrollment and access decisions.
- Camera capture at the entrance.
- Local face recognition through a trusted LAN computer/Raspberry Pi service (the classic ESP32-CAM is a camera endpoint; it is not treated as a robust modern face-recognition engine).
- Intrusion input and local audible alarm.
- A9G SMS and optional voice-call notification to configured household contacts.
- A9G GPS coordinates in alert messages when a valid fix is available.
- Battery-powered operation with regulated, protected power rails.

## Hardware baseline

- ESP32 DevKit (main controller)
- AI-Thinker ESP32-CAM (camera module)
- A9G GSM/GPRS + GPS development board (board variant and power input must be verified)
- Protected battery pack appropriate to the selected regulator and GSM current peaks
- DC/DC regulators, fuse, reverse-polarity protection, low-voltage cutoff
- PIR or door magnetic contact, siren driver (MOSFET/transistor), optional relay/lock driver

## Architecture

```text
ESP32-CAM --(Wi-Fi image/event)--> Local recognition service
                                      |
                                      | authorized / unknown
                                      v
                                  Main ESP32
                             /        |        \
                         Siren     Door I/O   A9G UART
                                                  |
                                         SMS / GPS / call
                                                  |
                                          Household contacts
```

The recognition service should run on a trusted local device on the same LAN. Configure it to return a signed or authenticated decision, not an unauthenticated arbitrary HTTP string. The example firmware in this repository uses a serial command interface for the first bench-integration stage; it does not unlock a door based on a raw camera result.

## Repository layout

- `firmware/main_esp32/` — main controller starter (alarm inputs, A9G UART, SMS and GPS parsing).
- `firmware/esp32_cam/` — camera capture starter.
- `server/face_service/` — local face-service integration contract and mock service.
- `hardware/` — wiring and power notes.
- `config/` — safe example configuration templates.
- `docs/` — setup, test plan and limitations.

## Wiring — prototype signal map

**Important:** A9G breakout boards vary substantially. Their VBAT input, onboard regulator, UART voltage tolerance and peak-current requirements are not standardized. Verify the exact board schematic before powering it. Never power an A9G from the ESP32 3V3 pin. Use a dedicated regulator sized for the board's documented transmit-current peaks, short low-resistance wiring and local bulk/ceramic capacitors as recommended by its manufacturer. Join grounds only after confirming the supply arrangement.

### Suggested ESP32 DevKit signal allocation

| Function | ESP32 pin | Notes |
|---|---:|---|
| A9G UART RX (ESP32 receives) | GPIO16 (RX2) | Connect to A9G TX; level-shift if required by the board |
| A9G UART TX (ESP32 sends) | GPIO17 (TX2) | Connect to A9G RX; level-shift if required |
| Door contact / PIR output | GPIO27 | Use a defined pull-up/down and suitable interface |
| Siren driver input | GPIO26 | Drive a MOSFET/transistor, never the siren directly |
| Tamper switch | GPIO25 | Optional, configure pull-up |
| Status LED | GPIO2 | Board-dependent |
| ESP32-CAM link | Wi-Fi | Avoid direct UART contention with A9G |

The ESP32-CAM is powered from its own stable 5 V regulator. Connect grounds as appropriate. A9G UART pins must not be assumed 5 V tolerant. Confirm logic-level compatibility with both module datasheets.

### Conceptual power wiring

```text
Protected battery pack
  +-- fuse + reverse polarity protection
  +-- 5 V buck/boost --> ESP32 DevKit (5V/VIN as board requires)
  +-- 5 V high-current regulator --> ESP32-CAM 5V
  +-- A9G-specific regulator --> A9G VBAT / power input per exact board manual
  +-- optional protected siren rail --> siren driver and siren

Battery negative / regulator grounds / module grounds: common reference
(only after verifying the modules' power topology)
```

Do not select the battery voltage, regulator or fuse by guesswork. Size them from the actual battery chemistry, regulator datasheets, camera load, GSM transmit peaks and siren load. Add a battery monitor and low-voltage cutoff.

## Firmware status

The current starter sketches provide:
- UART AT-command plumbing for the A9G.
- SMS notification with a configurable recipient.
- GPS NMEA RMC coordinate extraction and Google Maps link formatting.
- Input-triggered local alarm and notification with a cooldown.
- ESP32-CAM JPEG web endpoint for testing.

They are **not yet a finished access-control installation**. Before enabling access control, integrate the local face service with enrollment, liveness/anti-spoofing, confidence thresholds, audit logging and a fail-secure door interface that always permits safe manual egress. Do not store resident face images or biometric templates in a public repository.

## Configuration

1. Copy `config/secrets.example.h` to a local `secrets.h` file in the main firmware folder.
2. Fill in the household phone number and Wi-Fi credentials locally.
3. Never commit credentials, phone numbers, GPS home coordinates, face images or biometric templates.
4. Confirm the A9G cellular band and 2G availability with the SIM operator in the deployment location.
5. Test SMS, calls and GPS outdoors before installing the unit.

## Build environment

- Arduino IDE 2.x or PlatformIO.
- ESP32 Arduino core compatible with the selected board packages.
- ESP32-CAM: select the correct camera board definition (the sketch defaults to AI-Thinker).
- Main ESP32: select the correct ESP32 Dev Module.
- A9G UART wiring and baud rate must match the exact module firmware.

## Bench test checklist

- [ ] Verify every regulator output with a multimeter before connecting modules.
- [ ] Verify A9G power requirements and UART voltage levels from the exact breakout documentation.
- [ ] Test ESP32 and ESP32-CAM independently on USB/current-limited supplies.
- [ ] Test A9G registration, GPS fix, SMS and call manually.
- [ ] Test intrusion input and siren driver with the lock disconnected.
- [ ] Test false alarms, power loss, network loss and low battery.
- [ ] Validate recognition using consented household participants and presentation-attack tests.
- [ ] Keep a physical key/manual override and safe egress.
- [ ] Obtain human confirmation before contacting authorities.

## Emergency escalation

Raksha can alert designated household contacts and include the detected location. A camera classifier or facial mismatch alone is not sufficient evidence to automatically contact police. Any escalation beyond household contacts should be reviewed and confirmed by a person, using locally approved emergency channels.

## Privacy

Collect only the resident data needed for access. Obtain informed consent, protect enrollment/admin endpoints, encrypt data in transit and at rest, restrict retention, and provide a way to remove a resident. Keep all face data and phone numbers outside source control.

## License

Choose a license before public redistribution. Until then, all rights are reserved by the repository owner.
