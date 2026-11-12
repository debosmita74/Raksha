#!/usr/bin/env python3
"""Raksha local face service for Raspberry Pi 4/5.

Uses OpenCV Haar detection + LBPH face matching. This is a prototype identity
matcher, NOT presentation-attack detection or production biometric security.
It deliberately returns review/unknown for ambiguous results. Keep this service
on a trusted isolated LAN and never expose Flask directly to the internet.
"""
from __future__ import annotations

import os
import re
import json
import time
import threading
from pathlib import Path
from typing import Optional

import cv2
import numpy as np
from flask import Flask, jsonify, request, abort

BASE = Path(__file__).resolve().parent
DATA_DIR = Path(os.getenv("RAKSHA_DATA_DIR", str(BASE / "data"))).resolve()
FACES_DIR = DATA_DIR / "faces"
MODEL_PATH = DATA_DIR / "lbph.yml"
REGISTRY_PATH = DATA_DIR / "residents.json"
ADMIN_TOKEN = os.getenv("RAKSHA_ADMIN_TOKEN", "")
API_TOKEN = os.getenv("RAKSHA_API_TOKEN", "")
CAMERA_URL = os.getenv("RAKSHA_CAMERA_URL", "http://192.168.1.50/capture")
ESP32_URL = os.getenv("RAKSHA_ESP32_URL", "http://192.168.1.51/api/access")
MATCH_THRESHOLD = float(os.getenv("RAKSHA_LBPH_THRESHOLD", "55"))
MIN_STABLE_MATCHES = int(os.getenv("RAKSHA_MIN_STABLE_MATCHES", "3"))
ACCESS_ENABLED = os.getenv("RAKSHA_ENABLE_MATCH_ONLY_ACCESS", "false").lower() == "true"
POLL_SECONDS = float(os.getenv("RAKSHA_POLL_SECONDS", "1.0"))

app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = 5 * 1024 * 1024
lock = threading.RLock()
face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
)
if face_cascade.empty():
    raise RuntimeError("OpenCV Haar cascade not found")

recognizer = cv2.face.LBPHFaceRecognizer_create()
model_ready = False
label_to_id: dict[int, str] = {}
last_agent_event: dict[str, object] = {"status": "not_started"}


def _ensure_dirs() -> None:
    FACES_DIR.mkdir(parents=True, exist_ok=True)
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    if not REGISTRY_PATH.exists():
        REGISTRY_PATH.write_text("{}", encoding="utf-8")


def _read_registry() -> dict:
    _ensure_dirs()
    try:
        return json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return {}


def _write_registry(registry: dict) -> None:
    REGISTRY_PATH.write_text(json.dumps(registry, indent=2), encoding="utf-8")


def _authorized(token: str, expected: str) -> bool:
    return bool(expected) and token.startswith("Bearer ") and token[7:] == expected


def require_admin() -> None:
    if not _authorized(request.headers.get("Authorization", ""), ADMIN_TOKEN):
        abort(401, "Admin token missing or invalid")


def require_api() -> None:
    if not _authorized(request.headers.get("Authorization", ""), API_TOKEN):
        abort(401, "API token missing or invalid")


def _decode_image(raw: bytes) -> Optional[np.ndarray]:
    if not raw:
        return None
    arr = np.frombuffer(raw, dtype=np.uint8)
    return cv2.imdecode(arr, cv2.IMREAD_COLOR)


def _largest_face(gray: np.ndarray):
    faces = face_cascade.detectMultiScale(
        gray, scaleFactor=1.1, minNeighbors=5, minSize=(80, 80)
    )
    if len(faces) == 0:
        return None
    return max(faces, key=lambda box: int(box[2]) * int(box[3]))


def _face_chip(image: np.ndarray) -> Optional[np.ndarray]:
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    box = _largest_face(gray)
    if box is None:
        return None
    x, y, w, h = box
    chip = gray[y:y+h, x:x+w]
    chip = cv2.resize(chip, (160, 160), interpolation=cv2.INTER_AREA)
    chip = cv2.equalizeHist(chip)
    return chip


def retrain() -> int:
    global recognizer, model_ready, label_to_id
    _ensure_dirs()
    chips, labels = [], []
    new_map: dict[int, str] = {}
    registry = _read_registry()
    for label, resident_id in enumerate(sorted(registry.keys()), start=1):
        resident_dir = FACES_DIR / resident_id
        if not resident_dir.exists():
            continue
        count = 0
        for path in resident_dir.glob("*.jpg"):
            image = cv2.imread(str(path))
            if image is None:
                continue
            chip = _face_chip(image)
            if chip is not None:
                chips.append(chip)
                labels.append(label)
                count += 1
        if count:
            new_map[label] = resident_id
    with lock:
        label_to_id = new_map
        if not chips:
            model_ready = False
            return 0
        recognizer = cv2.face.LBPHFaceRecognizer_create()
        recognizer.train(chips, np.asarray(labels, dtype=np.int32))
        recognizer.save(str(MODEL_PATH))
        model_ready = True
    return len(chips)


def _predict(image: np.ndarray) -> dict:
    global model_ready
    chip = _face_chip(image)
    if chip is None:
        return {"decision": "no_face", "resident_id": None, "distance": None}
    with lock:
        if not model_ready:
            return {"decision": "review", "resident_id": None, "distance": None,
                    "reason": "No enrolled face model"}
        label, distance = recognizer.predict(chip)
        resident_id = label_to_id.get(int(label))
    if resident_id is not None and float(distance) <= MATCH_THRESHOLD:
        return {"decision": "match", "resident_id": resident_id,
                "distance": round(float(distance), 2)}
    return {"decision": "unknown", "resident_id": None,
            "distance": round(float(distance), 2)}


@app.get("/health")
def health():
    return jsonify({
        "service": "Raksha Face Service",
        "model_ready": model_ready,
        "access_enabled": ACCESS_ENABLED,
        "warning": "LBPH match is not liveness detection; do not use as sole production lock credential."
    })


@app.get("/admin/residents")
def list_residents():
    require_admin()
    return jsonify(_read_registry())


@app.post("/admin/residents/<resident_id>/enroll")
def enroll(resident_id: str):
    require_admin()
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", resident_id):
        abort(400, "resident_id must contain 1-32 letters, digits, underscore or hyphen")
    display_name = request.form.get("display_name", resident_id).strip()[:60]
    uploads = request.files.getlist("images")
    if len(uploads) < 5:
        abort(400, "Upload at least five consented face images with varied lighting/pose")
    resident_dir = FACES_DIR / resident_id
    resident_dir.mkdir(parents=True, exist_ok=True)
    saved = 0
    for upload in uploads[:30]:
        raw = upload.read()
        image = _decode_image(raw)
        if image is None:
            continue
        chip = _face_chip(image)
        if chip is None:
            continue
        target = resident_dir / f"face_{int(time.time() * 1000)}_{saved}.jpg"
        if cv2.imwrite(str(target), chip):
            saved += 1
    if saved < 5:
        abort(400, f"Only {saved} usable face images detected; at least five are required")
    registry = _read_registry()
    registry[resident_id] = {"display_name": display_name, "image_count": saved}
    _write_registry(registry)
    total = retrain()
    return jsonify({"ok": True, "resident_id": resident_id,
                    "images_saved": saved, "training_images_total": total})


@app.delete("/admin/residents/<resident_id>")
def remove_resident(resident_id: str):
    require_admin()
    registry = _read_registry()
    if resident_id not in registry:
        abort(404, "Resident not found")
    import shutil
    shutil.rmtree(FACES_DIR / resident_id, ignore_errors=True)
    del registry[resident_id]
    _write_registry(registry)
    total = retrain()
    return jsonify({"ok": True, "removed": resident_id, "training_images_total": total})


@app.post("/api/recognize")
def recognize():
    require_api()
    upload = request.files.get("image")
    if upload is None:
        abort(400, "multipart field 'image' is required")
    image = _decode_image(upload.read())
    if image is None:
        abort(400, "Invalid image")
    result = _predict(image)
    result["access_enabled"] = ACCESS_ENABLED
    result["liveness"] = "not_implemented"
    result["warning"] = "LBPH recognition does not prevent photo/video spoofing"
    return jsonify(result)


def agent_loop():
    """Poll ESP32-CAM; require consecutive same-person matches before a grant.

    This temporal consistency check is NOT liveness/anti-spoofing. Match-only
    unlock is disabled by default and must be explicitly enabled for a
    controlled bench prototype after the owner accepts the limitation.
    """
    global last_agent_event
    import requests
    previous_id = None
    stable_count = 0
    last_sent_id = None
    while True:
        try:
            cam = requests.get(CAMERA_URL, timeout=5)
            cam.raise_for_status()
            files = {"image": ("frame.jpg", cam.content, "image/jpeg")}
            response = requests.post(
                "http://127.0.0.1:8000/api/recognize",
                files=files,
                headers={"Authorization": f"Bearer {API_TOKEN}"},
                timeout=8,
            )
            response.raise_for_status()
            result = response.json()
            rid = result.get("resident_id") if result.get("decision") == "match" else None
            if rid and rid == previous_id:
                stable_count += 1
            elif rid:
                previous_id, stable_count = rid, 1
            else:
                previous_id, stable_count = None, 0

            last_agent_event = {"last_result": result, "stable_count": stable_count,
                                "access_enabled": ACCESS_ENABLED, "updated": time.time()}

            if ACCESS_ENABLED and rid and stable_count >= MIN_STABLE_MATCHES and rid != last_sent_id:
                event_id = f"{rid}-{int(time.time())}"
                payload = {"resident_id": rid, "event_id": event_id}
                sent = requests.post(
                    ESP32_URL,
                    json=payload,
                    headers={"Authorization": f"Bearer {API_TOKEN}"},
                    timeout=4,
                )
                sent.raise_for_status()
                last_sent_id = rid
                last_agent_event["door_controller"] = sent.json()
            if not rid:
                last_sent_id = None
        except Exception as exc:
            last_agent_event = {"status": "error", "message": str(exc), "updated": time.time()}
        time.sleep(POLL_SECONDS)


@app.get("/admin")
def admin_dashboard():
    require_admin()
    return app.send_static_file("dashboard.html")


@app.get("/admin/agent-status")
def agent_status():
    require_admin()
    return jsonify(last_agent_event)


if __name__ == "__main__":
    _ensure_dirs()
    retrain()
    if not ADMIN_TOKEN or not API_TOKEN:
        raise SystemExit("Set RAKSHA_ADMIN_TOKEN and RAKSHA_API_TOKEN before starting.")
    threading.Thread(target=agent_loop, daemon=True).start()
    # Bind only to localhost; put an authenticated reverse proxy in front if
    # remote admin access is required. Do not expose the dev server publicly.
    app.run(host="127.0.0.1", port=8000, threaded=True)
