#pragma once

namespace esphome::fendt_caravan {

/// Reads a number the panel wrote with a decimal comma, possibly padded with spaces or followed by a unit
/// ("13,9 V", "  0", "21,5^C"). `text` is edited in place. False when no digits were read.
bool parse_panel_float(char *text, float &out);
bool parse_panel_int(const char *text, int &out);
/// "1", "01", "on" or "true" is true and "0", "00", "off" or "false" is false, in any case and padded with spaces
/// or line ends; false for anything else
bool parse_panel_bool(const char *text, bool &out);

}  // namespace esphome::fendt_caravan
