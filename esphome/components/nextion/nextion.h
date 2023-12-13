#pragma once

#include <deque>
#include <vector>

#include "esphome/core/defines.h"
#include "esphome/core/time.h"

#include "esphome/components/uart/uart.h"
#include "nextion_base.h"
#include "nextion_component.h"
#include "esphome/components/display/display_color_utils.h"

#ifdef USE_NEXTION_TFT_UPLOAD
#ifdef ARDUINO
#ifdef USE_ESP32
#include <HTTPClient.h>
#endif  // USE_ESP32
#ifdef USE_ESP8266
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#endif  // USE_ESP8266
#elif defined(USE_ESP_IDF)
#include <esp_http_client.h>
#endif  // ARDUINO vs ESP-IDF
#endif  // USE_NEXTION_TFT_UPLOAD

namespace esphome {
namespace nextion {

class Nextion;
class NextionComponentBase;

using nextion_writer_t = std::function<void(Nextion &)>;

static const std::string COMMAND_DELIMITER{static_cast<char>(255), static_cast<char>(255), static_cast<char>(255)};

class Nextion : public NextionBase, public PollingComponent, public uart::UARTDevice {
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
  void set_component_picture(const char *component, const char *picture);
  /**
   * Set the background color of a component.
   * @param component The component name.
   * @param color The color (as a uint16_t).
   *
   * Example:
   * ```cpp
   * it.set_component_background_color("button", 63488);
   * ```
   *
   * This will change the background color of the component `button` to red.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void set_component_background_color(const char *component, uint16_t color);
  /**
   * Set the background color of a component.
   * @param component The component name.
   * @param color The color (as a string).
   *
   * Example:
   * ```cpp
   * it.set_component_background_color("button", "RED");
   * ```
   *
   * This will change the background color of the component `button` to red.
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void set_component_background_color(const char *component, const char *color);
  /**
   * Set the background color of a component.
   * @param component The component name.
   * @param color The color (as Color).
   *
   * Example:
   * ```cpp
   * auto blue = Color(0, 0, 255);
   * it.set_component_background_color("button", blue);
   * ```
   *
   * This will change the background color of the component `button` to blue.
   */
  void set_component_background_color(const char *component, Color color) override;
  /**
   * Set the pressed background color of a component.
   * @param component The component name.
   * @param color The color (as a uint16_t).
   *
   * Example:
   * ```cpp
   * it.set_component_pressed_background_color("button", 63488);
   * ```
   *
   * This will change the pressed background color of the component `button` to red. This is the background color that
   * is shown when the component is pressed.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void set_component_pressed_background_color(const char *component, uint16_t color);
  /**
   * Set the pressed background color of a component.
   * @param component The component name.
   * @param color The color (as a string).
   *
   * Example:
   * ```cpp
   * it.set_component_pressed_background_color("button", "RED");
   * ```
   *
   * This will change the pressed background color of the component `button` to red. This is the background color that
   * is shown when the component is pressed.
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void set_component_pressed_background_color(const char *component, const char *color);
  /**
   * Set the pressed background color of a component.
   * @param component The component name.
   * @param color The color (as Color).
   *
   * Example:
   * ```cpp
   * auto red = Color(255, 0, 0);
   * it.set_component_pressed_background_color("button", red);
   * ```
   *
   * This will change the pressed background color of the component `button` to red. This is the background color that
   * is shown when the component is pressed.
   */
  void set_component_pressed_background_color(const char *component, Color color) override;
  /**
   * Set the foreground color of a component.
   * @param component The component name.
   * @param color The color (as a uint16_t).
   *
   * Example:
   * ```cpp
   * it.set_component_foreground_color("button", 63488);
   * ```
   *
   * This will change the foreground color of the component `button` to red.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void set_component_foreground_color(const char *component, uint16_t color);
  /**
   * Set the foreground color of a component.
   * @param component The component name.
   * @param color The color (as a string).
   *
   * Example:
   * ```cpp
   * it.set_component_foreground_color("button", "RED");
   * ```
   *
   * This will change the foreground color of the component `button` to red.
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void set_component_foreground_color(const char *component, const char *color);
  /**
   * Set the foreground color of a component.
   * @param component The component name.
   * @param color The color (as Color).
   *
   * Example:
   * ```cpp
   * it.set_component_foreground_color("button", Color::BLACK);
   * ```
   *
   * This will change the foreground color of the component `button` to black.
   */
  void set_component_foreground_color(const char *component, Color color) override;
  /**
   * Set the pressed foreground color of a component.
   * @param component The component name.
   * @param color The color (as a uint16_t).
   *
   * Example:
   * ```cpp
   * it.set_component_pressed_foreground_color("button", 63488 );
   * ```
   *
   * This will change the pressed foreground color of the component `button` to red. This is the foreground color that
   * is shown when the component is pressed.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void set_component_pressed_foreground_color(const char *component, uint16_t color);
  /**
   * Set the pressed foreground color of a component.
   * @param component The component name.
   * @param color The color (as a string).
   *
   * Example:
   * ```cpp
   * it.set_component_pressed_foreground_color("button", "RED");
   * ```
   *
   * This will change the pressed foreground color of the component `button` to red. This is the foreground color that
   * is shown when the component is pressed.
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void set_component_pressed_foreground_color(const char *component, const char *color);
  /**
   * Set the pressed foreground color of a component.
   * @param component The component name.
   * @param color The color (as Color).
   *
   * Example:
   * ```cpp
   * auto blue = Color(0, 0, 255);
   * it.set_component_pressed_foreground_color("button", blue);
   * ```
   *
   * This will change the pressed foreground color of the component `button` to blue. This is the foreground color that
   * is shown when the component is pressed.
   */
  void set_component_pressed_foreground_color(const char *component, Color color) override;

  /**
   * Set the picture id of a component.
   * @param component The component name.
   * @param pic_id The picture ID.
   *
   * Example:
   * ```cpp
   * it.set_component_pic("textview", 1);
   * ```
   *
   * This will change the picture id of the component `textview`.
   */
  void set_component_pic(const char *component, uint8_t pic_id);
  /**
   * Set the background picture id of component.
   * @param component The component name.
   * @param pic_id The picture ID.
   *
   * Example:
   * ```cpp
   * it.set_component_picc("textview", 1);
   * ```
   *
   * This will change the background picture id of the component `textview`.
   */
  void set_component_picc(const char *component, uint8_t pic_id);

  /**
   * Set the font color of a component.
   * @param component The component name.
   * @param color The color (as a uint16_t).
   *
   * Example:
   * ```cpp
   * it.set_component_font_color("textview", 63488);
   * ```
   *
   * This will change the font color of the component `textview` to a red color.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void set_component_font_color(const char *component, uint16_t color);
  /**
   * Set the font color of a component.
   * @param component The component name.
   * @param color The color (as a string).
   *
   * Example:
   * ```cpp
   * it.set_component_font_color("textview", "RED");
   * ```
   *
   * This will change the font color of the component `textview` to a red color.
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void set_component_font_color(const char *component, const char *color);
  /**
   * Set the font color of a component.
   * @param component The component name.
   * @param color The color (as Color).
   *
   * Example:
   * ```cpp
   * it.set_component_font_color("textview", Color::BLACK);
   * ```
   *
   * This will change the font color of the component `textview` to black.
   */
  void set_component_font_color(const char *component, Color color) override;
  /**
   * Set the pressed font color of a component.
   * @param component The component name.
   * @param color The color (as a uint16_t).
   *
   * Example:
   * ```cpp
   * it.set_component_pressed_font_color("button", 63488);
   * ```
   *
   * This will change the pressed font color of the component `button` to a red.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void set_component_pressed_font_color(const char *component, uint16_t color);
  /**
   * Set the pressed font color of a component.
   * @param component The component name.
   * @param color The color (as a string).
   *
   * Example:
   * ```cpp
   * it.set_component_pressed_font_color("button", "RED");
   * ```
   *
   * This will change the pressed font color of the component `button` to a red color.
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void set_component_pressed_font_color(const char *component, const char *color);
  /**
   * Set the pressed font color of a component.
   * @param component The component name.
   * @param color The color (as Color).
   *
   * Example:
   * ```cpp
   * it.set_component_pressed_font_color("button", Color::BLACK);
   * ```
   *
   * This will change the pressed font color of the component `button` to black.
   */
  void set_component_pressed_font_color(const char *component, Color color) override;
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
  void set_component_font(const char *component, uint8_t font_id) override;
  /**
   * Send the current time to the nextion display.
   * @param time The time instance to send (get this with id(my_time).now() ).
   */
  void set_nextion_rtc_time(ESPTime time);

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
   * Show the page with a given id.
   * @param page The id of the page.
   *
   * Example:
   * ```cpp
   * it.goto_page(2);
   * ```
   *
   * Switches to the page named `main`. Pages are named in the Nextion Editor.
   */
  void goto_page(uint8_t page);
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
  void hide_component(const char *component) override;
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
  void show_component(const char *component) override;
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
  void open_waveform_channel(int component_id, uint8_t channel_number, uint8_t value);
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
   * @param color The color to draw with (number).
   *
   * Example:
   * ```cpp
   * fill_area(50, 50, 100, 100, 63488);
   * ```
   *
   * Fills an area that starts at x coordinate `50` and y coordinate `50` with a height of `100` and width of `100` with
   * the red color.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void fill_area(int x1, int y1, int width, int height, uint16_t color);
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
   * fill_area(50, 50, 100, 100, "RED");
   * ```
   *
   * Fills an area that starts at x coordinate `50` and y coordinate `50` with a height of `100` and width of `100` with
   * the red color.
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void fill_area(int x1, int y1, int width, int height, const char *color);
  /**
   * Fill a rectangle with a color.
   * @param x1 The starting x coordinate.
   * @param y1 The starting y coordinate.
   * @param width The width to draw.
   * @param height The height to draw.
   * @param color The color to draw with (as Color).
   *
   * Example:
   * ```cpp
   * auto blue = Color(0, 0, 255);
   * fill_area(50, 50, 100, 100, blue);
   * ```
   *
   * Fills an area that starts at x coordinate `50` and y coordinate `50` with a height of `100` and width of `100` with
   * blue color.
   */
  void fill_area(int x1, int y1, int width, int height, Color color);
  /**
   * Draw a line on the screen.
   * @param x1 The starting x coordinate.
   * @param y1 The starting y coordinate.
   * @param x2 The ending x coordinate.
   * @param y2 The ending y coordinate.
   * @param color The color to draw with (number).
   *
   * Example:
   * ```cpp
   * it.line(50, 50, 75, 75, 63488);
   * ```
   *
   * Makes a line that starts at x coordinate `50` and y coordinate `50` and ends at x coordinate `75` and y coordinate
   * `75` with the red color.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void line(int x1, int y1, int x2, int y2, uint16_t color);
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
   * it.line(50, 50, 75, 75, "BLUE");
   * ```
   *
   * Makes a line that starts at x coordinate `50` and y coordinate `50` and ends at x coordinate `75` and y coordinate
   * `75` with the blue color.
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void line(int x1, int y1, int x2, int y2, const char *color);
  /**
   * Draw a line on the screen.
   * @param x1 The starting x coordinate.
   * @param y1 The starting y coordinate.
   * @param x2 The ending x coordinate.
   * @param y2 The ending y coordinate.
   * @param color The color to draw with (as Color).
   *
   * Example:
   * ```cpp
   * auto blue = Color(0, 0, 255);
   * it.line(50, 50, 75, 75, blue);
   * ```
   *
   * Makes a line that starts at x coordinate `50` and y coordinate `50` and ends at x coordinate `75` and y coordinate
   * `75` with blue color.
   */
  void line(int x1, int y1, int x2, int y2, Color color);
  /**
   * Draw a rectangle outline.
   * @param x1 The starting x coordinate.
   * @param y1 The starting y coordinate.
   * @param width The width of the rectangle.
   * @param height The height of the rectangle.
   * @param color The color to draw with (number).
   *
   * Example:
   * ```cpp
   * it.rectangle(25, 35, 40, 50, 63488);
   * ```
   *
   * Makes a outline of a rectangle that starts at x coordinate `25` and y coordinate `35` and has a width of `40` and a
   * length of `50` with the red color.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void rectangle(int x1, int y1, int width, int height, uint16_t color);
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
   * it.rectangle(25, 35, 40, 50, "BLUE");
   * ```
   *
   * Makes a outline of a rectangle that starts at x coordinate `25` and y coordinate `35` and has a width of `40` and a
   * length of `50` with the blue color.
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void rectangle(int x1, int y1, int width, int height, const char *color);
  /**
   * Draw a rectangle outline.
   * @param x1 The starting x coordinate.
   * @param y1 The starting y coordinate.
   * @param width The width of the rectangle.
   * @param height The height of the rectangle.
   * @param color The color to draw with (as Color).
   *
   * Example:
   * ```cpp
   * auto blue = Color(0, 0, 255);
   * it.rectangle(25, 35, 40, 50, blue);
   * ```
   *
   * Makes a outline of a rectangle that starts at x coordinate `25` and y coordinate `35` and has a width of `40` and a
   * length of `50` with blue color.
   */
  void rectangle(int x1, int y1, int width, int height, Color color);
  /**
   * Draw a circle outline
   * @param center_x The center x coordinate.
   * @param center_y The center y coordinate.
   * @param radius The circle radius.
   * @param color The color to draw with (number).
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void circle(int center_x, int center_y, int radius, uint16_t color);
  /**
   * Draw a circle outline
   * @param center_x The center x coordinate.
   * @param center_y The center y coordinate.
   * @param radius The circle radius.
   * @param color The color to draw with (as a string).
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void circle(int center_x, int center_y, int radius, const char *color);
  /**
   * Draw a circle outline
   * @param center_x The center x coordinate.
   * @param center_y The center y coordinate.
   * @param radius The circle radius.
   * @param color The color to draw with (as Color).
   */
  void circle(int center_x, int center_y, int radius, Color color);
  /**
   * Draw a filled circled.
   * @param center_x The center x coordinate.
   * @param center_y The center y coordinate.
   * @param radius The circle radius.
   * @param color The color to draw with (number).
   *
   * Example:
   * ```cpp
   * it.filled_cricle(25, 25, 10, 63488);
   * ```
   *
   * Makes a filled circle at the x coordinate `25` and y coordinate `25` with a radius of `10` with the red color.
   * Use this [color picker](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to convert color codes to
   * Nextion HMI colors.
   */
  void filled_circle(int center_x, int center_y, int radius, uint16_t color);
  /**
   * Draw a filled circled.
   * @param center_x The center x coordinate.
   * @param center_y The center y coordinate.
   * @param radius The circle radius.
   * @param color The color to draw with (as a string).
   *
   * Example:
   * ```cpp
   * it.filled_cricle(25, 25, 10, "BLUE");
   * ```
   *
   * Makes a filled circle at the x coordinate `25` and y coordinate `25` with a radius of `10` with the blue color.
   * Use [Nextion Instruction Set](https://nextion.tech/instruction-set/#s5) for a list of Nextion HMI colors constants.
   */
  void filled_circle(int center_x, int center_y, int radius, const char *color);
  /**
   * Draw a filled circled.
   * @param center_x The center x coordinate.
   * @param center_y The center y coordinate.
   * @param radius The circle radius.
   * @param color The color to draw with (as Color).
   *
   * Example:
   * ```cpp
   * auto blue = Color(0, 0, 255);
   * it.filled_cricle(25, 25, 10, blue);
   * ```
   *
   * Makes a filled circle at the x coordinate `25` and y coordinate `25` with a radius of `10` with blue color.
   */
  void filled_circle(int center_x, int center_y, int radius, Color color);

  /** Set the brightness of the backlight.
   *
   * @param brightness The brightness percentage from 0 to 1.0.
   *
   * Example:
   * ```cpp
   * it.set_backlight_brightness(.3);
   * ```
   *
   * Changes the brightness of the display to 30%.
   */
  void set_backlight_brightness(float brightness);
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
  /**
   * Sets which page Nextion loads when exiting sleep mode. Note this can be set even when Nextion is in sleep mode.
   * @param page_id The page id, from 0 to the lage page in Nextion. Set 255 (not set to any existing page) to
   * wakes up to current page.
   *
   * Example:
   * ```cpp
   * it.set_wake_up_page(2);
   * ```
   *
   * The display will wake up to page 2.
   */
  void set_wake_up_page(uint8_t page_id = 255);
  /**
   * Sets which page Nextion loads when connecting to ESPHome.
   * @param page_id The page id, from 0 to the lage page in Nextion. Set 255 (not set to any existing page) to
   * wakes up to current page.
   *
   * Example:
   * ```cpp
   * it.set_start_up_page(2);
   * ```
   *
   * The display will go to page 2 when it establishes a connection to ESPHome.
   */
  void set_start_up_page(uint8_t page_id = 255);

  /**
   * Sets if Nextion should auto-wake from sleep when touch press occurs.
   * @param auto_wake True or false. When auto_wake is true and Nextion is in sleep mode,
   * the first touch will only trigger the auto wake mode and not trigger a Touch Event.
   *
   * Example:
   * ```cpp
   * it.set_auto_wake_on_touch(true);
   * ```
   *
   * The display will wake up by touch.
   */
  void set_auto_wake_on_touch(bool auto_wake);
  /**
   * Sets if Nextion should exit the active reparse mode before the "connect" command is sent
   * @param exit_reparse True or false. When exit_reparse is true, the exit reparse command
   * will be sent before requesting the connection from Nextion.
   *
   * Example:
   * ```cpp
   * it.set_exit_reparse_on_start(true);
   * ```
   *
   * The display will be requested to leave active reparse mode before setup.
   */
  void set_exit_reparse_on_start(bool exit_reparse);
  /**
   * Sets Nextion mode between sleep and awake
   * @param True or false. Sleep=true to enter sleep mode or sleep=false to exit sleep mode.
   */
  void sleep(bool sleep);
  /**
   * Sets Nextion Protocol Reparse mode between active or passive
   * @param True or false.
   * active_mode=true to enter active protocol reparse mode
   * active_mode=false to enter passive protocol reparse mode.
   */
  void set_protocol_reparse_mode(bool active_mode);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void register_touch_component(NextionComponentBase *obj) { this->touch_.push_back(obj); }
  void register_switch_component(NextionComponentBase *obj) { this->switchtype_.push_back(obj); }
  void register_binarysensor_component(NextionComponentBase *obj) { this->binarysensortype_.push_back(obj); }
  void register_sensor_component(NextionComponentBase *obj) { this->sensortype_.push_back(obj); }
  void register_textsensor_component(NextionComponentBase *obj) { this->textsensortype_.push_back(obj); }

  void setup() override;
  void set_brightness(float brightness) { this->brightness_ = brightness; }
  float get_setup_priority() const override;
  void update() override;
  void loop() override;
  void set_writer(const nextion_writer_t &writer);

  // This function has been deprecated
  void set_wait_for_ack(bool wait_for_ack);

  /**
   * Manually send a raw formatted command to the display.
   * @param format The printf-style command format, like "vis %s,0"
   * @param ... The format arguments
   * @return Whether the send was successful.
   */
  bool send_command_printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

#ifdef USE_NEXTION_TFT_UPLOAD
  /**
   * Set the tft file URL. https seems problematic with arduino..
   */
  void set_tft_url(const std::string &tft_url) { this->tft_url_ = tft_url; }

#endif

  /**
   * Upload the tft file and soft reset Nextion
   * @return bool True: Transfer completed successfuly, False: Transfer failed.
   */
  bool upload_tft();

  void dump_config() override;

  /**
   * Softreset the Nextion
   */
  void soft_reset();

  /** Add a callback to be notified of sleep state changes.
   *
   * @param callback The void() callback.
   */
  void add_sleep_state_callback(std::function<void()> &&callback);

  /** Add a callback to be notified of wake state changes.
   *
   * @param callback The void() callback.
   */
  void add_wake_state_callback(std::function<void()> &&callback);

  /** Add a callback to be notified when the nextion completes its initialize setup.
   *
   * @param callback The void() callback.
   */
  void add_setup_state_callback(std::function<void()> &&callback);

  /** Add a callback to be notified when the nextion changes pages.
   *
   * @param callback The void(std::string) callback.
   */
  void add_new_page_callback(std::function<void(uint8_t)> &&callback);

  /** Add a callback to be notified when Nextion has a touch event.
   *
   * @param callback The void() callback.
   */
  void add_touch_event_callback(std::function<void(uint8_t, uint8_t, bool)> &&callback);

  void update_all_components();

  /**
   * @brief Set the nextion sensor state object.
   *
   * @param[in] queue_type
   * Index of NextionQueueType.
   *
   * @param[in] name
   * Component/variable name.
   *
   * @param[in] state
   * State to set.
   */
  void set_nextion_sensor_state(int queue_type, const std::string &name, float state);
  void set_nextion_sensor_state(NextionQueueType queue_type, const std::string &name, float state);
  void set_nextion_text_state(const std::string &name, const std::string &state);

  void add_no_result_to_queue_with_set(NextionComponentBase *component, int state_value) override;
  void add_no_result_to_queue_with_set(const std::string &variable_name, const std::string &variable_name_to_send,
                                       int state_value) override;

  void add_no_result_to_queue_with_set(NextionComponentBase *component, const std::string &state_value) override;
  void add_no_result_to_queue_with_set(const std::string &variable_name, const std::string &variable_name_to_send,
                                       const std::string &state_value) override;

  void add_to_get_queue(NextionComponentBase *component) override;

  void add_addt_command_to_queue(NextionComponentBase *component) override;

  void update_components_by_prefix(const std::string &prefix);

  void set_touch_sleep_timeout_internal(uint32_t touch_sleep_timeout) {
    this->touch_sleep_timeout_ = touch_sleep_timeout;
  }
  void set_wake_up_page_internal(uint8_t wake_up_page) { this->wake_up_page_ = wake_up_page; }
  void set_start_up_page_internal(uint8_t start_up_page) { this->start_up_page_ = start_up_page; }
  void set_auto_wake_on_touch_internal(bool auto_wake_on_touch) { this->auto_wake_on_touch_ = auto_wake_on_touch; }
  void set_exit_reparse_on_start_internal(bool exit_reparse_on_start) {
    this->exit_reparse_on_start_ = exit_reparse_on_start;
  }

 protected:
  std::deque<NextionQueue *> nextion_queue_;
  std::deque<NextionQueue *> waveform_queue_;
  uint16_t recv_ret_string_(std::string &response, uint32_t timeout, bool recv_flag);
  void all_components_send_state_(bool force_update = false);
  uint64_t comok_sent_ = 0;
  bool remove_from_q_(bool report_empty = true);
  /**
   * @brief
   * Sends commands ignoring of the Nextion has been setup.
   */
  bool ignore_is_setup_ = false;
  bool nextion_reports_is_setup_ = false;
  uint8_t nextion_event_;

  void process_nextion_commands_();
  void process_serial_();
  bool is_updating_ = false;
  uint32_t touch_sleep_timeout_ = 0;
  int wake_up_page_ = -1;
  int start_up_page_ = -1;
  bool auto_wake_on_touch_ = true;
  bool exit_reparse_on_start_ = false;

  /**
   * Manually send a raw command to the display and don't wait for an acknowledgement packet.
   * @param command The command to write, for example "vis b0,0".
   */
  bool send_command_(const std::string &command);
  void add_no_result_to_queue_(const std::string &variable_name);
  bool add_no_result_to_queue_with_ignore_sleep_printf_(const std::string &variable_name, const char *format, ...)
      __attribute__((format(printf, 3, 4)));
  void add_no_result_to_queue_with_command_(const std::string &variable_name, const std::string &command);

  bool add_no_result_to_queue_with_printf_(const std::string &variable_name, const char *format, ...)
      __attribute__((format(printf, 3, 4)));

  void add_no_result_to_queue_with_set_internal_(const std::string &variable_name,
                                                 const std::string &variable_name_to_send, int state_value,
                                                 bool is_sleep_safe = false);

  void add_no_result_to_queue_with_set_internal_(const std::string &variable_name,
                                                 const std::string &variable_name_to_send,
                                                 const std::string &state_value, bool is_sleep_safe = false);

  void check_pending_waveform_();

#ifdef USE_NEXTION_TFT_UPLOAD
#ifdef USE_ESP8266
  WiFiClient *wifi_client_{nullptr};
  BearSSL::WiFiClientSecure *wifi_client_secure_{nullptr};
  WiFiClient *get_wifi_client_();
#endif
  int content_length_ = 0;
  int tft_size_ = 0;
#ifdef ARDUINO
  /**
   * will request chunk_size chunks from the web server
   * and send each to the nextion
   * @param HTTPClient http HTTP client handler.
   * @param int range_start Position of next byte to transfer.
   * @return position of last byte transferred, -1 for failure.
   */
  int upload_by_chunks_(HTTPClient *http, int range_start);

  bool upload_with_range_(uint32_t range_start, uint32_t range_end);

  /**
   * start update tft file to nextion.
   *
   * @param const uint8_t *file_buf
   * @param size_t buf_size
   * @return true if success, false for failure.
   */
  bool upload_from_buffer_(const uint8_t *file_buf, size_t buf_size);
  /**
   * Ends the upload process, restart Nextion and, if successful,
   * restarts ESP
   * @param bool url successful True: Transfer completed successfuly, False: Transfer failed.
   * @return bool True: Transfer completed successfuly, False: Transfer failed.
   */
  bool upload_end_(bool successful);
#elif defined(USE_ESP_IDF)
  /**
   * will request 4096 bytes chunks from the web server
   * and send each to Nextion
   * @param std::string url Full url for download.
   * @param int range_start Position of next byte to transfer.
   * @return position of last byte transferred, -1 for failure.
   */
  int upload_range(const std::string &url, int range_start);
  /**
   * Ends the upload process, restart Nextion and, if successful,
   * restarts ESP
   * @param bool url successful True: Transfer completed successfuly, False: Transfer failed.
   * @return bool True: Transfer completed successfuly, False: Transfer failed.
   */
  bool upload_end(bool successful);
#endif  // ARDUINO vs ESP-IDF

#endif  // USE_NEXTION_TFT_UPLOAD

  bool get_is_connected_() { return this->is_connected_; }

  bool check_connect_();

  std::vector<NextionComponentBase *> touch_;
  std::vector<NextionComponentBase *> switchtype_;
  std::vector<NextionComponentBase *> sensortype_;
  std::vector<NextionComponentBase *> textsensortype_;
  std::vector<NextionComponentBase *> binarysensortype_;
  CallbackManager<void()> setup_callback_{};
  CallbackManager<void()> sleep_callback_{};
  CallbackManager<void()> wake_callback_{};
  CallbackManager<void(uint8_t)> page_callback_{};
  CallbackManager<void(uint8_t, uint8_t, bool)> touch_callback_{};

  optional<nextion_writer_t> writer_;
  float brightness_{1.0};

  std::string device_model_;
  std::string firmware_version_;
  std::string serial_number_;
  std::string flash_size_;

  void remove_front_no_sensors_();

#ifdef USE_NEXTION_TFT_UPLOAD
  std::string tft_url_;
  uint8_t *transfer_buffer_{nullptr};
  size_t transfer_buffer_size_;
  bool upload_first_chunk_sent_ = false;
#endif

#ifdef NEXTION_PROTOCOL_LOG
  void print_queue_members_();
#endif
  void reset_(bool reset_nextion = true);

  std::string command_data_;
  bool is_connected_ = false;
  uint32_t startup_override_ms_ = 8000;
  uint32_t max_q_age_ms_ = 8000;
  uint32_t started_ms_ = 0;
  bool sent_setup_commands_ = false;
};
}  // namespace nextion
}  // namespace esphome
