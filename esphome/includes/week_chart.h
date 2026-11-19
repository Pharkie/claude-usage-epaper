// The week chart: left column with the week's %, time to reset and the reset
// moment; right, Mon..Sun cumulative usage against an even-pace diagonal, with
// a dashed projection at the last 24 h's rate and, when that projection hits
// 100% before the reset, an X and "runs out <when>"; a strip underneath.
// Fable is the solid black part of the week's hatched area: its share of the
// week, so it grows only when Fable use does. "■ Fable 4%" under the reset
// time gives Fable against its OWN limit, the number Claude's app shows.
// A handed-out reset zeroes the week (or Fable) without moving the Monday
// reset; HA spots it, and the pace line then starts from it (pace_from).
//
// Everything is worked out by Home Assistant (homeassistant/week_chart.yaml);
// this file only draws. Coordinates are the mockup's, in the 296 x 128
// landscape frame, and text uses BASELINE alignment so the y values are the
// mockup's baselines.

#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace week_chart {

static const int CX0 = 104;     // chart left (week start)
static const int CX1 = 292;     // chart right (reset)
static const int CY_TOP = 10;   // 100%
static const int CY_BASE = 94;  // 0%

inline int XH(float h) { return CX0 + (int) lroundf(h * (CX1 - CX0) / 168.0f); }

inline int YP(float p) {
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return CY_BASE - (int) lroundf(p * (CY_BASE - CY_TOP) / 100.0f);
}

// "0 2 3 - 5" -> values, one per hour. "-" is an hour HA has no reading for;
// it takes the previous hour's value. Leading unknowns stay -1 (not drawn).
inline std::vector<int> ParsePoints(const std::string &s) {
  std::vector<int> out;
  size_t i = 0, n = s.size();
  while (i < n) {
    while (i < n && s[i] == ' ') i++;
    if (i >= n) break;
    size_t j = i;
    while (j < n && s[j] != ' ') j++;
    std::string tok = s.substr(i, j - i);
    char *end = nullptr;
    long v = strtol(tok.c_str(), &end, 10);
    out.push_back((tok != "-" && end != nullptr && *end == '\0') ? (int) v : -1);
    i = j;
  }
  int last = -1;
  for (auto &v : out) {
    if (v < 0) v = last; else last = v;
  }
  return out;
}

// Bresenham with an on/off dash pattern (off == 0 draws solid) and an optional
// second pixel for a 2 px line.
inline void PatternLine(esphome::display::Display *it, int x0, int y0, int x1, int y1,
                        esphome::Color c, int on, int off, int thick) {
  const bool steep = abs(y1 - y0) > abs(x1 - x0);
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, k = 0;
  while (true) {
    if (off == 0 || (k % (on + off)) < on) {
      it->draw_pixel_at(x0, y0, c);
      if (thick > 1) {
        if (steep) it->draw_pixel_at(x0 + 1, y0, c);
        else it->draw_pixel_at(x0, y0 + 1, c);
      }
    }
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
    k++;
  }
}

inline int TextWidth(esphome::display::Display *it, esphome::font::Font *f, const char *s) {
  int x1 = 0, y1 = 0, w = 0, h = 0;
  it->get_text_bounds(0, 0, s, f, esphome::display::TextAlign::TOP_LEFT, &x1, &y1, &w, &h);
  return w;
}

struct Fonts {
  esphome::font::Font *big;     // the week's %
  esphome::font::Font *row;     // time to reset
  esphome::font::Font *tiny;    // labels
  esphome::font::Font *tiny_b;  // today's day letter, the run-out time
  esphome::font::Font *strip;   // the strip
};

struct Data {
  int used = -1;              // week %, from the ESP's own week sensor
  std::string points;         // hourly history, see ParsePoints
  int hour = -1;              // hours since the week began, 0-167; -1 unknown
  float run_out = -1;         // hour of the week usage reaches 100%, or -1
  std::string run_out_label;  // "Fri 23:00"
  int proj_end = -1;          // % at the reset at the current rate, or -1
  std::string time_left;      // "3d 11h"
  std::string reset_label;    // "Mon 01:00"
  std::string days;           // "MTWTFSS"
  std::string strip;          // text for the strip
  bool invert_strip = false;  // black strip = act now
  int fable = -1;             // Fable % of its own limit, -1 unknown
  std::string fable_share;    // Fable's part of the week, in the week's %,
                              // hourly, as points (HA carries it across resets)
  int pace_from = 0;          // hour the even pace starts from: 0, or the hour
                              // of a reset in the middle of the week
};

inline void Draw(esphome::display::Display *it, const Fonts &f,
                 esphome::Color ink, esphome::Color paper, const Data &d) {
  using esphome::display::TextAlign;
  const int W = it->get_width();
  // An early reset leaves resets_at where it was, so the even pace restarts
  // from 0% at the reset rather than at the week's start. The old week's climb
  // stays drawn to the left of it, as history.
  const int pf = std::min(std::max(d.pace_from, 0), 167);

  // ---- Left column ----
  it->print(4, 13, f.tiny, ink, TextAlign::BASELINE_LEFT, "Week");
  if (d.used >= 0) {
    it->printf(2, 47, f.big, ink, TextAlign::BASELINE_LEFT, "%d%%", d.used);
  } else {
    it->print(2, 47, f.big, ink, TextAlign::BASELINE_LEFT, "--");
  }
  if (!d.time_left.empty()) {
    it->print(4, 71, f.row, ink, TextAlign::BASELINE_LEFT, d.time_left.c_str());
    it->print(4, 83, f.tiny, ink, TextAlign::BASELINE_LEFT, "to reset");
  }
  if (!d.reset_label.empty()) {
    it->print(4, 95, f.tiny, ink, TextAlign::BASELINE_LEFT, d.reset_label.c_str());
  }
  // In the corner beside the day letters, which was empty. The square is the
  // legend for the solid slice.
  if (d.fable >= 0) {
    it->filled_rectangle(4, 98, 7, 7, ink);
    it->print(14, 105, f.tiny, ink, TextAlign::BASELINE_LEFT, "Fable");
    it->printf(14 + TextWidth(it, f.tiny, "Fable "), 105, f.tiny_b, ink,
               TextAlign::BASELINE_LEFT, "%d%%", d.fable);
  }

  // ---- Chart frame ----
  for (int x = CX0; x <= CX1; x += 3) it->draw_pixel_at(x, CY_TOP, ink);
  it->print(101, 13, f.tiny, ink, TextAlign::BASELINE_RIGHT, "100");
  it->horizontal_line(CX0, CY_BASE, CX1 - CX0 + 1, ink);
  const std::string days = d.days.size() >= 7 ? d.days : std::string("MTWTFSS");
  const int today = d.hour >= 0 ? d.hour / 24 : -1;
  for (int day = 0; day <= 7; day++) {
    it->vertical_line(XH(day * 24), CY_BASE, 4, ink);
    if (day < 7) {
      char letter[2] = {days[day], 0};
      it->print(XH(day * 24 + 12), 105, day == today ? f.tiny_b : f.tiny, ink,
                TextAlign::BASELINE_CENTER, letter);
    }
  }

  // ---- Usage so far ----
  int nx = -1, ny = -1;
  if (d.hour >= 0 && d.used >= 0) {
    std::vector<int> v = ParsePoints(d.points);
    if ((int) v.size() > d.hour + 1) v.resize(d.hour + 1);
    int last = v.empty() ? -1 : v.back();
    while ((int) v.size() < d.hour + 1) v.push_back(last);
    v[d.hour] = d.used;  // the ESP's own week value is the freshest

    // y of the curve at every chart column up to now, then hatch under it.
    int ycol[CX1 - CX0 + 1];
    for (auto &y : ycol) y = -1;
    for (int i = 0; i < d.hour; i++) {
      if (v[i] < 0 || v[i + 1] < 0) continue;
      int xa = XH(i), xb = XH(i + 1), ya = YP(v[i]), yb = YP(v[i + 1]);
      for (int x = xa; x <= xb; x++) {
        float t = xb == xa ? 1.0f : (float) (x - xa) / (xb - xa);
        ycol[x - CX0] = (int) lroundf(ya + t * (yb - ya));
      }
    }
    for (int x = CX0; x <= XH(d.hour); x++) {
      int top = ycol[x - CX0];
      if (top < 0) continue;
      for (int y = top + 1; y < CY_BASE; y++) {
        if ((x + y) % 3 == 0) it->draw_pixel_at(x, y, ink);
      }
    }

    // Fable's share of the week, solid under the hatch. HA works it out in
    // the week's units, across Fable-only and week resets (week_chart.yaml);
    // capped at the week's own value so a poll landing between the two
    // sensors can't poke it above the curve.
    if (d.fable >= 0 && !d.fable_share.empty()) {
      std::vector<int> fp = ParsePoints(d.fable_share);
      if ((int) fp.size() > d.hour + 1) fp.resize(d.hour + 1);
      int flast = fp.empty() ? -1 : fp.back();
      while ((int) fp.size() < d.hour + 1) fp.push_back(flast);
      std::vector<float> fw(d.hour + 1, -1.0f);
      for (int i = 0; i <= d.hour; i++) {
        if (fp[i] < 0) continue;
        fw[i] = v[i] >= 0 ? std::min((float) fp[i], (float) v[i]) : (float) fp[i];
      }
      for (int i = 0; i < d.hour; i++) {
        if (fw[i] < 0 || fw[i + 1] < 0) continue;
        int xa = XH(i), xb = XH(i + 1), ya = YP(fw[i]), yb = YP(fw[i + 1]);
        for (int x = xa; x <= xb; x++) {
          float t = xb == xa ? 1.0f : (float) (x - xa) / (xb - xa);
          int top = (int) lroundf(ya + t * (yb - ya));
          for (int y = top; y < CY_BASE; y++) it->draw_pixel_at(x, y, ink);
        }
      }
    }

    // Even pace: 0% at the week's start (or an early reset) to 100% at the reset.
    PatternLine(it, XH(pf), CY_BASE, CX1, CY_TOP, ink, 3, 2, 1);

    for (int i = 0; i < d.hour; i++) {
      if (v[i] < 0 || v[i + 1] < 0) continue;
      PatternLine(it, XH(i), YP(v[i]), XH(i + 1), YP(v[i + 1]), ink, 1, 0, 2);
    }

    nx = XH(d.hour);
    ny = YP(d.used);
    for (int y = CY_TOP; y <= CY_BASE; y += 2) it->draw_pixel_at(nx, y, ink);
  } else {
    PatternLine(it, XH(pf), CY_BASE, CX1, CY_TOP, ink, 3, 2, 1);
  }

  // ---- Projection, and the run-out marker when it lands before the reset ----
  if (nx >= 0) {
    if (d.run_out >= 0 && d.run_out <= 168) {
      const int ex = XH(d.run_out), ey = CY_TOP;
      PatternLine(it, nx, ny, ex, ey, ink, 4, 3, 2);
      for (int o = 0; o < 2; o++) {
        it->line(ex - 3 + o, ey - 3, ex + 3 + o, ey + 3, ink);
        it->line(ex + 3 + o, ey - 3, ex - 3 + o, ey + 3, ink);
      }
      const char *top = "runs out";
      const char *when = d.run_out_label.c_str();
      int w = std::max(TextWidth(it, f.tiny, top), TextWidth(it, f.tiny_b, when));
      // Right of the X if it fits, else left. The paper box knocks out the
      // pace line, which passes behind the label.
      if (ex + 6 + w <= W - 1) {
        it->filled_rectangle(ex + 4, 13, w + 4, 22, paper);
        it->print(ex + 6, 22, f.tiny, ink, TextAlign::BASELINE_LEFT, top);
        it->print(ex + 6, 32, f.tiny_b, ink, TextAlign::BASELINE_LEFT, when);
      } else {
        it->filled_rectangle(ex - 8 - w, 13, w + 4, 22, paper);
        it->print(ex - 6, 22, f.tiny, ink, TextAlign::BASELINE_RIGHT, top);
        it->print(ex - 6, 32, f.tiny_b, ink, TextAlign::BASELINE_RIGHT, when);
      }
    } else if (d.proj_end >= 0) {
      PatternLine(it, nx, ny, CX1, YP(d.proj_end), ink, 4, 3, 2);
    }
    it->filled_circle(nx, ny, 3, ink);
  }

  // ---- Strip ----
  if (d.invert_strip) {
    it->filled_rectangle(0, 110, W, 18, ink);
  } else {
    it->horizontal_line(0, 110, W, ink);
  }
  it->print(W / 2, 123, f.strip, d.invert_strip ? paper : ink,
            TextAlign::BASELINE_CENTER, d.strip.c_str());
}

}  // namespace week_chart
