// Full-screen status banner for the states where there is nothing useful to
// plot. One layout, distinct wording per condition -- the whole point is that
// "the token is dead" and "Home Assistant hasn't handed us a token yet" look
// different, because only the first one needs the user to do anything.
//
// Layout matches the old hardcoded TOKEN EXPIRED screen: an inverted headline
// bar, a plain-English detail line, a smaller advice line, and the time.

void DrawStatusBanner(esphome::display::Display *it,
                      esphome::font::Font *font_head,
                      esphome::font::Font *font_body,
                      esphome::font::Font *font_small,
                      esphome::Color black, esphome::Color white,
                      const char *headline, const char *detail,
                      const char *advice, esphome::ESPTime now) {
  const int W = it->get_width();
  const int H = it->get_height();

  it->filled_rectangle(0, 18, W, 34, black);
  it->printf(W / 2, 35, font_head, white, TextAlign::CENTER, "%s", headline);
  it->printf(W / 2, 66, font_body, black, TextAlign::CENTER, "%s", detail);
  it->printf(W / 2, 88, font_small, black, TextAlign::CENTER, "%s", advice);

  if (now.is_valid()) {
    it->strftime(W / 2, H - 14, font_small, black, TextAlign::TOP_CENTER,
                 "Since %H:%M", now);
  }
}
