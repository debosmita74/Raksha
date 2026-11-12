# Raksha Phase 2 — Raspberry Pi + ESP32-CAM + ESP32 access controller

## Network topology

All three devices should be on a private Wi-Fi network with DHCP reservations/static addresses.

```text
AI-Thinker ESP32-CAM
  5 V dedicated regulated supply
  Wi-Fi: JPEG at http://CAMERA_IP/capture
             |
             | private LAN
             v
Raspberry Pi 4 / 5
  Flask + OpenCV-contrib LBPH service
  Local encrypted/private resident image directory
  Polls camera; requires repeated consistent match
             |
             | authenticated HTTP POST /api/access
             v
Main ESP32 DevKit
  GPIO33 -> isolated relay/MOSFET driver -> lock hardware
  GPIO27 <- door/PIR input
  GPIO26 -> siren driver
```

## ESP32 Phase 2 pin map

| ESP32 pin | Connection | Notes |
|---|---|---|
| GPIO33 | Relay/MOSFET driver input | Lock pulse; never connect lock coil directly |
| GPIO27 | PIR/door contact | Active low in starter sketch |
| GPIO26 | Siren driver input | Driver stage required |
| 5V/VIN | Regulated ESP32 supply | Follow exact DevKit input specification |
| GND | Signal ground | Coordinate with isolated driver design |

The relay/lock must have its own supply and protection. Use a properly rated opto-isolated driver where appropriate. Verify whether the lock is fail-safe or fail-secure with a qualified installer; preserve mechanical egress and key override. The firmware output is configured to default to locked/off on reset, but the mechanical system must remain safe during power loss.

## ESP32-CAM

Keep the AI-Thinker ESP32-CAM powered by a stable dedicated 5 V supply. Its camera endpoint must be reachable only from the private LAN. Reserve its IP address and set `RAKSHA_CAMERA_URL` on the Pi.

## Raspberry Pi

- Install Raspberry Pi OS 64-bit.
- Use a dedicated account and private data directory.
- Keep face images and model files on the Pi, not in Git.
- Use a firewall to allow only the Pi and ESP32 to communicate with the relevant endpoints.
- Do not port-forward the camera, Flask service or ESP32 controller to the public internet.

## Important limitation

The current OpenCV Haar + LBPH pipeline performs face matching only. Consecutive matching frames reduce transient false matches but do NOT establish liveness and do NOT stop a printed photograph or replayed video. Match-only unlocking is disabled by default. Add a validated presentation-attack detection or independent second factor before enabling it for a real home.
