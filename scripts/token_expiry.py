#!/usr/bin/env python3
"""Days left on the Claude usage ACCESS token, as JSON for a command_line sensor.

This house runs the STATIC 1-YEAR token (design B), so the number that matters
is the access token's own expiry -- `obtained_at + expires_in`. A fresh browser
mint asks for expires_in=31536000 and Anthropic grants it with scope
user:profile, which is everything the usage endpoint needs.

The refresh CHAIN is reported too, as an attribute, but it is deliberately NOT
used: refreshing revokes the current access token and hands back an 8h one, so
exercising the chain is what turns a 1-year token into an 8h token. The chain
expires 30 days after the mint and is simply allowed to lapse.

`days_left` therefore counts down to the re-mint deadline. A value near 0.3 means
something rotated the token -- check that claude_usage_token_renew is still
commented out in automations.yaml.
"""
import json, time

CRED = "/config/claude_usage/credentials.json"
FMT = "%Y-%m-%d %H:%M %Z"

try:
    c = json.load(open(CRED))
    obtained = c["obtained_at"]
    access_at = obtained + c["expires_in"]
    chain_at = obtained + c.get("refresh_token_expires_in", 0)
    now = time.time()
    print(json.dumps({
        "days_left": round(max(0.0, (access_at - now) / 86400), 2),
        "expires_at": time.strftime(FMT, time.localtime(access_at)),
        "minted_at": time.strftime(FMT, time.localtime(obtained)),
        "expires_in": c["expires_in"],
        "looks_rotated": c["expires_in"] < 86400,
        "chain_days_left": round(max(0.0, (chain_at - now) / 86400), 2),
        "chain_expires_at": time.strftime(FMT, time.localtime(chain_at)),
    }))
except Exception as e:
    # days_left null -> the sensor's availability template marks it unavailable,
    # rather than pushing a non-numeric state at a sensor that has a unit (which
    # raises ValueError and kills the entity outright).
    print(json.dumps({"days_left": None, "expires_at": "unknown", "error": str(e)}))
