# Claude Code usage on a 2.9" e-paper display

**Glance up, see your Claude limits, stay in flow.**

If you live in Claude Code you know the feeling: deep in a task and suddenly
you're out of session, or the weekly cap lands mid-sprint. This little desk
display keeps your Session, Weekly and per-model usage in view all day, with
reset times under each bar, so you can pace the heavy work without ever typing
`/usage`.

<p>
<img src="images/front.jpeg" width="49%" alt="Front view, the inverted 85% is the 80%+ warning" />
<img src="images/reboot.gif" width="49%" alt="Boot to first draw" />
</p>

### Reasons to love it

- **Always on**: e-paper is readable in any light, silent, and barely sips power.
- **Fresh**: updates within a minute of your numbers changing. The panel only
  redraws when something actually changed, so no constant e-paper flashing.
- **Works with your computer off**: runs entirely from Home Assistant. Claude
  usage from your phone and the web counts against the same limits, so the
  display stays honest while your laptop sleeps.
- **Warns you before it matters**: any bar at 80%+ flips to an inverted
  percentage you can read across the room (that's the 85% in the photo).
- **Fails loudly, never lies**: a full-screen TOKEN EXPIRED banner if auth
  dies, stale-data warnings if Home Assistant or the API goes away, and dashes
  instead of fake zeros when data is missing.
- **No upkeep**: set it up once and it renews its own API token every 4 hours.

## Hardware

- [FireBeetle 2 ESP32-C6](https://www.dfrobot.com/product-2771.html) (any
  ESP32 that ESPHome supports will do with pin tweaks)
- [Waveshare 2.9" e-Paper Module V2](https://www.waveshare.com/2.9inch-e-paper-module.htm),
  black and white, SPI. Wiring below matches the YAML.
- USB power. (A LiPo works too and battery sensors are included, but 5-minute
  refreshes will drain it; this config assumes mains.)

Wiring as configured: CLK to GPIO4, DIN/MOSI to GPIO6, CS to GPIO7, DC to
GPIO1, BUSY to GPIO3, RST to GPIO2.

<img src="images/rear.jpeg" width="60%" alt="Rear view, FireBeetle 2 ESP32-C6 in the 3D printed stand" />

The stand is a separate model:
[2.9" ePaper stand for FireBeetle2](https://makerworld.com/en/models/990598-2-9-epaper-stand-firebeetle2-backpack-battery#profileId-966064).

## How it works

```
Anthropic usage API  <-(poll 5 min, OAuth token)-  Home Assistant  -(push)->  ESP32 -> e-paper
```

Home Assistant polls `https://api.anthropic.com/api/oauth/usage`, the
undocumented endpoint behind Claude Code's own `/usage` screen, and exposes
the numbers as sensors. The ESP subscribes to them over the native API and
draws the bars.

## Setup

### 1. Mint a token (about 2 minutes)

```bash
python3 scripts/mint_claude_token.py
```

Open the printed URL, authorize, paste the code back. The credentials it
saves include a refresh token, which step 4 uses to keep everything renewing
automatically. The script's comments explain the traps that make this
non-obvious (scope requirements, and a User-Agent bot filter that fakes
"rate limited" responses).

### 2. Home Assistant

- Append the contents of `homeassistant/rest.yaml` to your
  `configuration.yaml` (top-level `rest:` block; merge if you already have one).
- Add the `claude_oauth_bearer` line the mint script printed to `secrets.yaml`.
- Restart HA (or reload "RESTful entities and notify services").
- Check Developer Tools > States: `sensor.claude_usage_session` and friends
  should be numbers, and `sensor.claude_usage_status` should be `ok`.

### 3. ESPHome

- Use the `esphome/` dir as your device dir (`firebeetle2-29.yaml` plus
  `includes/text_utils.h`); add `fonts/materialdesignicons-webfont.ttf`
  ([download](https://github.com/Templarian/MaterialDesign-Webfont/raw/master/fonts/materialdesignicons-webfont.ttf))
  as `esphome/fonts/`. Montserrat fetches automatically from Google Fonts at
  build time.
- In the YAML, set the two `REPLACE_ME` values (API encryption key, OTA
  password) and your `wifi_ssid`/`wifi_password` secrets.
- `cd esphome && esphome run firebeetle2-29.yaml`, then adopt the device in HA.

### 4. Auto-renewal

Copy `scripts/renew.py` and your minted credentials JSON to
`/config/claude_usage/` on HA, then wire the `shell_command` and two
automations from `homeassistant/automations.yaml`. From then on HA renews the
token every 4 hours, self-heals after downtime, and notifies you only if
renewal fails (the fix is always the same: run the mint script again). The
refresh token dies after about 30 days unused, so if you skip this step, plan
to re-mint by hand instead.

## Notes and gotchas

- **Unofficial API.** Anthropic could change or break this endpoint at any
  time; the JSON shape already changed once (mid-2026, when per-model usage
  moved into a `limits[]` array). The HA templates degrade to `unknown` (the
  panel shows `--`) rather than lying if the shape shifts again.
- **Rename the model row** if you're not tracking Fable: the template picks
  the first `weekly_scoped` limit on your account, and the label in the
  display lambda is just a string.
- **Don't leave a dead token polling.** Repeated failed-auth polls get the
  usage endpoint itself temporarily 429'd (it forgives as soon as valid auth
  returns). The status sensor and banner exist so a dead token is obvious.
- This Waveshare panel **cannot partial-refresh** under ESPHome
  (`full_update_every: 1` is required or frames superimpose). That's why the
  config redraws only on data changes instead.
- Battery bits (the ADC divider on GPIO0 and the pinned `adc_oneshot`
  external component) are specific to the FireBeetle 2 C6. Delete both if
  they don't apply to you.

## Similar projects

The endpoint is unofficial but has a real ecosystem:
[ccusage](https://github.com/ryoppippi/ccusage),
[Claude-Code-Usage-Monitor](https://github.com/Maciek-roboblog/Claude-Code-Usage-Monitor)
(whose [issue #202](https://github.com/Maciek-roboblog/Claude-Code-Usage-Monitor/issues/202)
is the best public spec of the endpoint),
[CodexBar](https://github.com/steipete/CodexBar), a
[TRMNL recipe](https://trmnl.com/recipes/263932) (which scrapes CLI output
instead of calling the API), and, if you'd rather not hand-roll the HA side,
the [hass-claude-usage](https://github.com/trickv/hass-claude-usage) HACS
integration.

Two things in this repo don't seem to be documented anywhere else:

1. **A 1-year token that can read usage.** Public docs treat "long-lived" and
   "usage-capable" as mutually exclusive (`claude setup-token` gives a year
   but inference-only; login tokens can read usage but live for hours).
   Requesting `expires_in: 31536000` at code exchange with only
   `user:profile user:inference` scope gets you both; broad scopes are what
   disqualify custom expiry. The mint script does this. Note you can't keep a
   1-year token and use auto-renewal: refreshing revokes the current access
   token, and refreshed tokens always live about 8 hours. This project uses
   auto-renewal.
2. **The token-endpoint User-Agent trap.** A fake-looking User-Agent gets an
   unconditional, permanent-looking `429 rate_limit_error` from the OAuth
   token endpoint, easily mistaken for real rate limiting (waiting and IP
   changes do nothing). Send the CLI-shaped UA and the same request is
   served. Community docs note a related UA effect on the usage endpoint;
   the token-exchange variant cost us a day of phantom-cooldown chasing.

## LLM Generated, Human Reviewed

This project was generated with Claude Code (Anthropic, Claude Fable 5).
Development was overseen by the human author with attention to reliability
and security. Architectural decisions, configuration choices, and development
sessions were closely planned, directed and verified by the human author
throughout. The code and results were reviewed and tested by the human author
beyond the LLM. Still, the code has had limited manual review; I encourage
you to make your own checks and use this code at your own risk.

Inspired by the [TRMNL Claude Usage recipe](https://trmnl.com/recipes/263932)
by Carl Edwards, which scrapes the Claude Code CLI; this build reads the API
directly and runs on cheap standalone hardware.

## License

PolyForm Noncommercial 1.0.0, see [LICENSE.md](./LICENSE.md).
