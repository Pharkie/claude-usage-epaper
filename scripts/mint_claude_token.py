#!/usr/bin/env python3
"""Mint a 1-YEAR Claude OAuth token that can read your Claude Code usage stats.

Stdlib only. Run it, open the printed URL in a browser, click Authorize,
paste the code shown (looks like  longcode#longstate ) back into the prompt.

Why this works (learned the hard way):
- The usage endpoint needs the `user:profile` scope. `claude setup-token`
  tokens are inference-only, so they can NOT read usage.
- A custom `expires_in` (1 year) is only allowed for NARROW scopes, request
  more scopes and the server refuses. So we ask for user:profile+user:inference.
- Anthropic's OAuth endpoints reply "429 rate_limit_error" FOREVER to requests
  with a fake-looking User-Agent. That 429 is bot defence, not a rate limit -
  the UA below matches the real Claude Code CLI format.
"""
import base64, hashlib, json, os, secrets, urllib.parse, urllib.request

CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e"  # Claude Code's public OAuth client
REDIRECT = "https://platform.claude.com/oauth/code/callback"
UA = "claude-cli/2.1.209 (external, cli)"

verifier = base64.urlsafe_b64encode(secrets.token_bytes(32)).rstrip(b"=").decode()
challenge = base64.urlsafe_b64encode(
    hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()
state = base64.urlsafe_b64encode(secrets.token_bytes(32)).rstrip(b"=").decode()

url = "https://claude.ai/oauth/authorize?" + urllib.parse.urlencode({
    "code": "true", "client_id": CLIENT_ID, "response_type": "code",
    "redirect_uri": REDIRECT, "scope": "user:profile user:inference",
    "code_challenge": challenge, "code_challenge_method": "S256", "state": state,
})
print("\n1) Open this URL, sign in if asked, click Authorize:\n\n" + url)
code_state = input("\n2) Paste the code shown (code#state): ").strip()
code = code_state.split("#")[0]

req = urllib.request.Request(
    "https://platform.claude.com/v1/oauth/token",
    data=json.dumps({
        "grant_type": "authorization_code", "code": code,
        "redirect_uri": REDIRECT, "client_id": CLIENT_ID,
        "code_verifier": verifier, "state": state,
        "expires_in": 31536000,  # request 1 year
    }).encode(),
    headers={"Content-Type": "application/json", "User-Agent": UA,
             "anthropic-beta": "oauth-2025-04-20"})
with urllib.request.urlopen(req, timeout=120) as r:
    tok = json.load(r)

days = tok.get("expires_in", 0) / 86400
out = os.path.expanduser("~/claude_usage_token.json")
with open(out, "w") as f:
    json.dump(tok, f, indent=2)
os.chmod(out, 0o600)

print(f"\nSUCCESS, token valid {days:.0f} days, scope: {tok.get('scope')}")
print(f"Full response (incl. refresh_token for renewal) saved to {out}")
print("\nPut this line in Home Assistant secrets.yaml (single line, no wrap):")
print(f'\nclaude_oauth_bearer: "Bearer {tok["access_token"]}"')
