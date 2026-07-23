int GetTextBounds(esphome::display::Display* it, esphome::font::Font *font, const char *buffer)
{
    // out-params for get_text_bounds(); we only use width
    int x1 = 0;
    int y1 = 0;
    int width = 0;
    int height = 0;
    it->get_text_bounds(0, 0, buffer, font, TextAlign::TOP_LEFT, &x1, &y1, &width, &height);
    return width;
}

int GetTextWidth(esphome::display::Display* it, esphome::font::Font *font, const char* formatting, const char *raw_text){
    char temp_buffer[80];
    snprintf(temp_buffer, sizeof(temp_buffer), formatting, raw_text);
    return GetTextBounds(it, font, temp_buffer);
}

int GetTextWidth(esphome::display::Display* it, esphome::font::Font *font, const char* formatting){
    char temp_buffer[80];
    snprintf(temp_buffer, sizeof(temp_buffer), "%s", formatting);
    return GetTextBounds(it, font, temp_buffer);
}

int GetTextWidth(esphome::display::Display* it, esphome::font::Font *font, const char* formatting, float& raw_text){
    char temp_buffer[80];
    snprintf(temp_buffer, sizeof(temp_buffer), formatting, raw_text);
    return GetTextBounds(it, font, temp_buffer);
}

int GetTextWidth(esphome::display::Display* it, esphome::font::Font *font, const char* formatting, float& raw_text1, float& raw_text2){
    char temp_buffer[80];
    snprintf(temp_buffer, sizeof(temp_buffer), formatting, raw_text1, raw_text2);
    return GetTextBounds(it, font, temp_buffer);
}

// Calculate the width of time format
int GetTextWidth(esphome::display::Display* it, esphome::font::Font *font, const char* formatting, esphome::ESPTime time){
    auto c_tm = time.to_c_tm();
    size_t buffer_length = 80;
    char temp_buffer[buffer_length];
    strftime(temp_buffer, buffer_length, formatting, &c_tm);
    return GetTextBounds(it, font, temp_buffer);
}