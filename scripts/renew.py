#!/usr/bin/env python3
"""Self-renew the Claude usage OAuth token on Home Assistant.

Reads /config/claude_usage/credentials.json, refreshes it (requesting a fresh
1-year expiry, granted only for narrow scopes), atomically saves the rotated
credentials, and rewrites the claude_oauth_bearer line in /config/secrets.yaml.

Run weekly (automation), the refresh token only lives ~30 days, so periodic
renewal is what keeps the chain alive indefinitely. Exit 0 on success.
Stdlib only; runs inside the HA Core container via shell_command.
"""
import json, os, sys, tempfile, urllib.request

CRED = "/config/claude_usage/credentials.json"
SECRETS = "/config/secrets.yaml"
CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
UA = "claude-cli/2.1.209 (external, cli)"

try:
    creds = json.load(open(CRED))
    req = urllib.request.Request(
        "https://platform.claude.com/v1/oauth/token",
        data=json.dumps({
            "grant_type": "refresh_token",
            "refresh_token": creds["refresh_token"],
            "client_id": CLIENT_ID,
            "scope": "user:profile user:inference",
            "expires_in": 31536000,
        }).encode(),
        headers={"Content-Type": "application/json", "User-Agent": UA,
                 "anthropic-beta": "oauth-2025-04-20"})
    with urllib.request.urlopen(req, timeout=60) as r:
        new = json.load(r)

    if "access_token" not in new or "refresh_token" not in new:
        print(f"FAILED: incomplete response: {sorted(new.keys())}")
        sys.exit(1)

    merged = {**creds, **new}
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(CRED))
    with os.fdopen(fd, "w") as f:
        json.dump(merged, f, indent=2)
    os.chmod(tmp, 0o600)
    os.replace(tmp, CRED)

    lines = open(SECRETS).read().splitlines()
    out = [f'claude_oauth_bearer: "Bearer {new["access_token"]}"'
           if l.startswith("claude_oauth_bearer:") else l for l in lines]
    with open(SECRETS, "w") as f:
        f.write("\n".join(out) + "\n")

    print(f"RENEWED access_ttl_days={new.get('expires_in',0)/86400:.1f} "
          f"refresh_ttl_days={new.get('refresh_token_expires_in',0)/86400:.1f}")
except Exception as e:
    print(f"FAILED: {e}")
    sys.exit(1)
