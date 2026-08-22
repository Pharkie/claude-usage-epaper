#!/usr/bin/env python3
"""Days left on the OAuth refresh CHAIN, as JSON for a command_line sensor.

`refresh_token_expires_in` is NOT a per-rotation TTL. It counts down to a fixed
expiry set at the original BROWSER mint, and rotating the refresh token does not
extend it. Verified 2026-08-22: the chain minted 2026-07-23 10:37 BST died
exactly 30.00 days later mid-cycle, after ~90 successful 4-hourly rotations, and
the auto-renew could not save it -- `invalid_grant / Refresh token expired`.

So when this reaches zero NOTHING automatic can recover: only re-running
mint_claude_token.py in a browser can. This sensor is the only advance warning.
"""
import json, time

CRED = "/config/claude_usage/credentials.json"

try:
    c = json.load(open(CRED))
    expires_at = c["obtained_at"] + c["refresh_token_expires_in"]
    print(json.dumps({
        "days_left": round(max(0.0, (expires_at - time.time()) / 86400), 2),
        "expires_at": time.strftime("%Y-%m-%d %H:%M %Z", time.localtime(expires_at)),
    }))
except Exception as e:
    # days_left null -> the sensor's availability template marks it unavailable,
    # rather than pushing a non-numeric state at a sensor that has a unit (which
    # raises ValueError and kills the entity outright).
    print(json.dumps({"days_left": None, "expires_at": "unknown", "error": str(e)}))
