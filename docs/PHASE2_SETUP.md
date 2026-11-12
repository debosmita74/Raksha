# Raksha Phase 2 setup — Raspberry Pi 4/5 face matching

## 1. Prepare the Raspberry Pi

Use Raspberry Pi OS 64-bit. Connect the Pi, ESP32-CAM and main ESP32 to the same trusted private LAN. Reserve stable IP addresses for the camera and ESP32.

Install dependencies (a virtual environment is recommended):

```bash
sudo apt update
sudo apt install -y python3-venv python3-pip libatlas-base-dev
cd ~/Raksha/server/face_service
python3 -m venv --system-site-packages .venv
source .venv/bin/activate
pip install -r requirements.txt
```

If the OpenCV contrib wheel fails to install on your Pi OS/Python version, use a compatible distro OpenCV-contrib package or build one for that OS; the code requires `cv2.face.LBPHFaceRecognizer_create()`.

## 2. Configure secrets

Copy `.env.example` to a private `.env`, generate two different long random secrets, and set the camera and ESP32 URLs. Do not commit the real .env. Example:

```bash
python3 -c "import secrets; print(secrets.token_urlsafe(32))"
```

Set environment variables from that file in your service shell. Do not expose port 8000 to the LAN; the Flask development process binds to localhost. The polling agent runs in the same process and reaches the local API.

## 3. Configure ESP32-CAM

Flash `firmware/esp32_cam/esp32_cam.ino` with AI-Thinker ESP32-CAM board selected. Set the Wi-Fi SSID/password locally. Open serial monitor and note its IP address. Visit `http://CAMERA_IP/capture` from a browser on the LAN; it should return a JPEG.

## 4. Configure the main ESP32

Flash `firmware/phase2_access_controller/phase2_access_controller.ino`. Set the same Wi-Fi credentials and API token as `RAKSHA_API_TOKEN` on the Pi. Set an explicit comma-separated resident allow-list in the sketch. The allow-list is intentionally empty in the committed starter, so no one can unlock until you configure it. The lock driver output is GPIO33 and is pulse-operated.

Keep the relay/lock disconnected for first tests. Verify `http://ESP32_IP/health` returns JSON. Do not put the API token in screenshots or public logs.

## 5. Enroll residents

Start the face service with its environment variables loaded:

```bash
python raksha_face_service.py
```

Enrollment is an authenticated multipart API. Example with five or more consented JPEG/PNG photos:

```bash
curl -X POST "http://127.0.0.1:8000/admin/residents/alice/enroll" \
  -H "Authorization: Bearer $RAKSHA_ADMIN_TOKEN" \
  -F "display_name=Alice" \
  -F "images=@alice_01.jpg" \
  -F "images=@alice_02.jpg" \
  -F "images=@alice_03.jpg" \
  -F "images=@alice_04.jpg" \
  -F "images=@alice_05.jpg"
```

Use a range of lighting and natural poses. Do not enroll people without informed consent. The service stores face crops under `RAKSHA_DATA_DIR`; back up that private directory securely.

List or remove residents with the authenticated admin endpoints:

```bash
curl -H "Authorization: Bearer $RAKSHA_ADMIN_TOKEN" http://127.0.0.1:8000/admin/residents
curl -X DELETE -H "Authorization: Bearer $RAKSHA_ADMIN_TOKEN" http://127.0.0.1:8000/admin/residents/alice
```

## 6. Test in safe mode

The default `RAKSHA_ENABLE_MATCH_ONLY_ACCESS=false` means the agent will recognize and report a match but will not send an unlock request. Check service health and agent status. Tune the LBPH threshold using your own validation set; lower distance is a closer match.

Only after adding a separate validated liveness or second-factor control, completing security review and testing the mechanical override should you consider enabling any automatic access action. The current code's repeated-frame check is not liveness detection and can be defeated by a photo/video replay.

## 7. Known Phase 2 limitations

- OpenCV LBPH is a basic baseline, not state-of-the-art face recognition.
- No anti-spoof/liveness detector is implemented.
- The ESP32 HTTP bearer token is prototype LAN authentication, not encrypted transport. Use an isolated trusted network; production should use authenticated TLS or a stronger local gateway.
- The ESP32's in-memory duplicate-event check is not durable replay protection.
- The current agent uses a development Flask server; deploy behind a hardened local service manager and authenticated proxy for sustained use.
- Door locks, life-safety egress and emergency response require professional review and local compliance.
