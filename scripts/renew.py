#!/usr/bin/env python3
"""Refresh the Claude usage OAuth token; print the new bearer for HA to store.

Runs as an HA shell_command. Refreshes the chain (rotating ~8h access token +
~30-day refresh token), saves credentials.json, and prints the new bearer as
the last stdout line (BEARER=...). The calling automation writes that into
input_text.claude_oauth_bearer, which the REST sensor reads as a templated
header on every poll, so the integration is NEVER reloaded (repeated
rest.reload was what wedged it).
"""
import json, os, sys, time, tempfile, urllib.request

CRED = "/config/claude_usage/credentials.json"
CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
UA = "claude-cli/2.1.209 (external, cli)"

try:
    creds = json.load(open(CRED))
    req = urllib.request.Request(
        "https://platform.claude.com/v1/oauth/token",
        data=json.dumps({"grant_type": "refresh_token",
                         "refresh_token": creds["refresh_token"],
                         "client_id": CLIENT_ID}).encode(),
        headers={"Content-Type": "application/json", "User-Agent": UA,
                 "anthropic-beta": "oauth-2025-04-20"})
    # under HA's 60s shell_command kill, so we fail cleanly first
    with urllib.request.urlopen(req, timeout=30) as r:
        new = json.load(r)
    if "access_token" not in new or "refresh_token" not in new:
        print(f"FAILED: incomplete refresh response {sorted(new.keys())}")
        sys.exit(1)
    merged = {**creds, **new, "obtained_at": int(time.time())}
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(CRED))
    with os.fdopen(fd, "w") as f:
        json.dump(merged, f, indent=2)
    os.chmod(tmp, 0o600)
    os.replace(tmp, CRED)
    print(f"BEARER=Bearer {new['access_token']}")
except Exception as e:
    print(f"FAILED: {e}")
    sys.exit(1)
