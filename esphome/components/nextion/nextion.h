#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace nextion {

class NextionTouchComponent;
class Nextion;

using nextion_writer_t = std::function<void(Nextion &)>;

class Nextion : public PollingComponent, public uart::UARTDevice {
 public:
  /**
   * Set the text of a component to a static string.
   * @param component The component name.
   * @param text The static text to set.
   *
   * Example:
   * ```cpp
   * it.set_component_text("textview", "Hello World!");
   * ```
   *
   * This will set the `txt` property of the component `textview` to `Hello World`.
   */
  void set_component_text(const char *component, const char *text);
  /**
   * Set the text of a component to a formatted string
   * @param component The component name.
   * @param format The printf-style format string.
   * @param ... The arguments to the format.
   *
   * Example:
   * ```cpp
   * it.set_component_text_printf("textview", "The uptime is: %.0f", id(uptime_sensor).state);
   * ```
   *
   * This will change the text on the component named `textview` to `The uptime is:` Then the value of `uptime_sensor`.
   * with zero decimals of accuracy (whole number).
   * For example when `uptime_sensor` = 506, then, `The uptime is: 506` will be displayed.
   */
  void set_component_text_printf(const char *component, const char *format, ...) __attribute__((format(printf, 3, 4)));
  /**
   * Set the integer value of a component
   * @param component The component name.
   * @param value The value to set.
   *
   * Example:
   * ```cpp
   * it.set_component_value("gauge", 50);
   * ```
   *
   * This will change the property `value` of the component `gauge` to 50.
   */
  void set_component_value(const char *component, int value);
  /**
   * Set the picture of an image component.
   * @param component The component name.
   * @param value The picture name.
   *
   * Example:
   * ```cpp
   * it.set_component_picture("pic", "4");
   * ```
   *
   * This will change the image of the component `pic` to the image with ID `4`.
   */
  void set_component_picture(const char *component, const char *picture) {
    this->send_command_printf("%s.val=%s", component, picture);
  }
  /**
   * Set the background color of a component.
   * @param component The component name.
   * @param color The color (as a string).
   *
   * Example:
   * ```cpp
   * it.set_component_background_color("button", "17013");
   * ```
   *
   * This will change the background color of the component `button` to blue.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void set_component_background_color(const char *component, const char *color);
  /**
   * Set the pressed background color of a component.
   * @param component The component name.
   * @param color The color (as a string).
   *
   * Example:
   * ```cpp
   * it.set_component_pressed_background_color("button", "17013");
   * ```
   *
   * This will change the pressed background color of the component `button` to blue. This is the background color that
   * is shown when the component is pressed. Use this [color
   * picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to Nextion HMI
   * colors.
   */
  void set_component_pressed_background_color(const char *component, const char *color);
  /**
   * Set the font color of a component.
   * @param component The component name.
   * @param color The color (as a string).
   *
   * Example:
   * ```cpp
   * it.set_component_font_color("textview", "17013");
   * ```
   *
   * This will change the font color of the component `textview` to a blue color.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void set_component_font_color(const char *component, const char *color);
  /**
   * Set the pressed font color of a component.
   * @param component The component name.
   * @param color The color (as a string).
   *
   * Example:
   * ```cpp
   * it.set_component_pressed_font_color("button", "17013");
   * ```
   *
   * This will change the pressed font color of the component `button` to a blue color.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void set_component_pressed_font_color(const char *component, const char *color);
  /**
   * Set the coordinates of a component on screen.
   * @param component The component name.
   * @param x The x coordinate.
   * @param y The y coordinate.
   *
   * Example:
   * ```cpp
   * it.set_component_coordinates("pic", 55, 100);
   * ```
   *
   * This will move the position of the component `pic` to the x coordinate `55` and y coordinate `100`.
   */
  void set_component_coordinates(const char *component, int x, int y);
  /**
   * Set the font id for a component.
   * @param component The component name.
   * @param font_id The ID of the font (number).
   *
   * Example:
   * ```cpp
   * it.set_component_font("textview", "3");
   * ```
   *
   * Changes the font of the component named `textveiw`. Font IDs are set in the Nextion Editor.
   */
  void set_component_font(const char *component, uint8_t font_id);
#ifdef USE_TIME
  /**
   * Send the current time to the nextion display.
   * @param time The time instance to send (get this with id(my_time).now() ).
   */
  void set_nextion_rtc_time(time::ESPTime time);
#endif

  /**
   * Show the page with a given name.
   * @param page The name of the page.
   *
   * Example:
   * ```cpp
   * it.goto_page("main");
   * ```
   *
   * Switches to the page named `main`. Pages are named in the Nextion Editor.
   */
  void goto_page(const char *page);
  /**
   * Hide a component.
   * @param component The component name.
   *
   * Example:
   * ```cpp
   * hide_component("button");
   * ```
   *
   * Hides the component named `button`.
   */
  void hide_component(const char *component);
  /**
   * Show a component.
   * @param component The component name.
   *
   * Example:
   * ```cpp
   * show_component("button");
   * ```
   *
   * Shows the component named `button`.
   */
  void show_component(const char *component);
  /**
   * Enable touch for a component.
   * @param component The component name.
   *
   * Example:
   * ```cpp
   * enable_component_touch("button");
   * ```
   *
   * Enables touch for component named `button`.
   */
  void enable_component_touch(const char *component);
  /**
   * Disable touch for a component.
   * @param component The component name.
   *
   * Example:
   * ```cpp
   * disable_component_touch("button");
   * ```
   *
   * Disables touch for component named `button`.
   */
  void disable_component_touch(const char *component);
  /**
   * Add waveform data to a waveform component
   * @param component_id The integer component id.
   * @param channel_number The channel number to write to.
   * @param value The value to write.
   */
  void add_waveform_data(int component_id, uint8_t channel_number, uint8_t value);
  /**
   * Display a picture at coordinates.
   * @param picture_id The picture id.
   * @param x1 The x coordinate.
   * @param y1 The y coordniate.
   *
   * Example:
   * ```cpp
   * display_picture(2, 15, 25);
   * ```
   *
   * Displays the picture who has the id `2` at the x coordinates `15` and y coordinates `25`.
   */
  void display_picture(int picture_id, int x_start, int y_start);
  /**
   * Fill a rectangle with a color.
   * @param x1 The starting x coordinate.
   * @param y1 The starting y coordinate.
   * @param width The width to draw.
   * @param height The height to draw.
   * @param color The color to draw with (as a string).
   *
   * Example:
   * ```cpp
   * fill_area(50, 50, 100, 100, "17013");
   * ```
   *
   * Fills an area that starts at x coordiante `50` and y coordinate `50` with a height of `100` and width of `100` with
   * the color of blue. Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to
   * convert color codes to Nextion HMI colors
   */
  void fill_area(int x1, int y1, int width, int height, const char *color);
  /**
   * Draw a line on the screen.
   * @param x1 The starting x coordinate.
   * @param y1 The starting y coordinate.
   * @param x2 The ending x coordinate.
   * @param y2 The ending y coordinate.
   * @param color The color to draw with (as a string).
   *
   * Example:
   * ```cpp
   * it.line(50, 50, 75, 75, "17013");
   * ```
   *
   * Makes a line that starts at x coordinate `50` and y coordinate `50` and ends at x coordinate `75` and y coordinate
   * `75` with the color of blue. Use this [color
   * picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to Nextion HMI
   * colors.
   */
  void line(int x1, int y1, int x2, int y2, const char *color);
  /**
   * Draw a rectangle outline.
   * @param x1 The starting x coordinate.
   * @param y1 The starting y coordinate.
   * @param width The width of the rectangle.
   * @param height The height of the rectangle.
   * @param color The color to draw with (as a string).
   *
   * Example:
   * ```cpp
   * it.rectangle(25, 35, 40, 50, "17013");
   * ```
   *
   * Makes a outline of a rectangle that starts at x coordinate `25` and y coordinate `35` and has a width of `40` and a
   * length of `50` with color of blue. Use this [color
   * picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to Nextion HMI
   * colors.
   */
  void rectangle(int x1, int y1, int width, int height, const char *color);
  /**
   * Draw a circle outline
   * @param center_x The center x coordinate.
   * @param center_y The center y coordinate.
   * @param radius The circle radius.
   * @param color The color to draw with (as a string).
   */
  void circle(int center_x, int center_y, int radius, const char *color);
  /**
   * Draw a filled circled.
   * @param center_x The center x coordinate.
   * @param center_y The center y coordinate.
   * @param radius The circle radius.
   * @param color The color to draw with (as a string).
   *
   * Example:
   * ```cpp
   * it.filled_cricle(25, 25, 10, "17013");
   * ```
   *
   * Makes a filled circle at the x cordinates `25` and y coordinate `25` with a radius of `10` with a color of blue.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void filled_circle(int center_x, int center_y, int radius, const char *color);

  /** Set the brightness of the backlight.
   *
   * @param brightness The brightness, from 0 to 100.
   *
   * Example:
   * ```cpp
   * it.set_backlight_brightness(30);
   * ```
   *
   * Changes the brightness of the display to 30%.
   */
  void set_backlight_brightness(uint8_t brightness);
  /**
   * Set the touch sleep timeout of the display.
   * @param timeout Timeout in seconds.
   *
   * Example:
   * ```cpp
   * it.set_touch_sleep_timeout(30);
   * ```
   *
   * After 30 seconds the display will go to sleep. Note: the display will only wakeup by a restart or by setting up
   * `thup`.
   */
  void set_touch_sleep_timeout(uint16_t timeout);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void register_touch_component(NextionTouchComponent *obj) { this->touch_.push_back(obj); }
  void setup() override;
  void set_brightness(float brightness) { this->brightness_ = brightness; }
  float get_setup_priority() const override;
  void update() override;
  void loop() override;
  void set_writer(const nextion_writer_t &writer);

  /**
   * Manually send a raw command to the display and don't wait for an acknowledgement packet.
   * @param command The command to write, for example "vis b0,0".
   */
  void send_command_no_ack(const char *command);
  /**
   * Manually send a raw formatted command to the display.
   * @param format The printf-style command format, like "vis %s,0"
   * @param ... The format arguments
   * @return Whether the send was successful.
   */
  bool send_command_printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

  void set_wait_for_ack(bool wait_for_ack);

 protected:
  bool ack_();
  bool read_until_ack_();

  std::vector<NextionTouchComponent *> touch_;
  optional<nextion_writer_t> writer_;
  bool wait_for_ack_{true};
  float brightness_{1.0};
};

class NextionTouchComponent : public binary_sensor::BinarySensorInitiallyOff {
 public:
  void set_page_id(uint8_t page_id) { page_id_ = page_id; }
  void set_component_id(uint8_t component_id) { component_id_ = component_id; }
  void process(uint8_t page_id, uint8_t component_id, bool on);

 protected:
  uint8_t page_id_;
  uint8_t component_id_;
};

}  // namespace nextion
}  // namespace esphome
