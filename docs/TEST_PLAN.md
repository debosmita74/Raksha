# Raksha test plan

## Bench tests (no door lock connected)

1. Visual inspection and continuity check.
2. Verify each regulated output voltage without modules attached.
3. Power ESP32 and confirm serial boot.
4. Power ESP32-CAM separately and test repeated JPEG captures.
5. Power A9G with its specified supply and verify AT, registration, SMS and voice-call behavior.
6. Verify GPS fix outdoors and test coordinate conversion.
7. Trigger GPIO27 and verify siren output at GPIO26 through a driver.
8. Confirm SMS rate limiting and recovery after modem reset.

## Fault injection

- Disconnect Wi-Fi.
- Remove SIM / cellular service.
- Block GPS reception.
- Restart A9G while ESP32 remains powered.
- Simulate stuck intrusion input.
- Simulate low battery and sudden supply sag.
- Reboot during an alert.
- Confirm no duplicate message storm occurs.

## Biometric validation

- Use only participants who have explicitly consented.
- Test different lighting, pose, glasses and normal appearance variation.
- Test printed photographs and replayed video; reject spoof attempts.
- Measure false acceptance and false rejection rates.
- Use a conservative threshold and a manual fallback.
- Keep an independent key/manual egress path.

## Field readiness gate

Do not install on a real access-controlled entry until power integrity, environmental protection, tamper response, cybersecurity, privacy, mechanical override and safe egress have been independently reviewed. Do not have the prototype automatically contact emergency services based only on computer vision.
