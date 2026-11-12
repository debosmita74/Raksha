# Local face service integration

The classic AI-Thinker ESP32-CAM sketch supplies JPEG frames at `http://CAMERA_IP/capture`. Run a local face service on a trusted LAN machine (PC or Raspberry Pi with sufficient compute) to compare a frame against consented, enrolled household members.

## Required API contract (to implement)

`POST /v1/recognize`
- Request: JPEG image bytes.
- Response: JSON `{ "decision": "known" | "unknown" | "review", "resident_id": "opaque-id", "confidence": 0.0, "liveness": "pass" | "fail" | "not_checked", "request_id": "..." }`.
- Require authenticated client access (e.g. mutual TLS or a securely provisioned token).
- Enforce rate limits, request size limits and audit logs.
- Return `review` when confidence is marginal or liveness is not passed.
- Never unlock solely from a single face match. Require anti-spoofing/liveness and an independent policy check.
- Store biometric templates locally encrypted; do not place face images/templates in Git.

## Mock response

A development mock may return only `review`. Do not use a mock to unlock a physical door.

This directory intentionally contains an API contract, not a falsely presented production biometric system. Select and validate a suitable recognition/liveness engine for the target compute platform before implementation.
