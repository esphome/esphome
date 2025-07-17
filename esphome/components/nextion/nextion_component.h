#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/color.h"
#include "nextion_base.h"

namespace esphome {
namespace nextion {
class NextionComponent;

class NextionComponent : public NextionComponentBase {
 public:
  void update_component_settings() override { this->update_component_settings(false); };

  void update_component_settings(bool force_update) override;

  void set_background_color(Color bco);
  void set_background_pressed_color(Color bco2);
  void set_foreground_color(Color pco);
  void set_foreground_pressed_color(Color pco2);
  void set_font_id(uint8_t font_id);
  void set_visible(bool visible);

 protected:
  /**
   * @brief Constructor initializes component state with visible=true (default state)
   */
  NextionComponent() {
    component_flags_ = {};         // Zero-initialize all state
    component_flags_.visible = 1;  // Set default visibility to true
  }

  NextionBase *nextion_;

  // Color and styling properties
  Color bco_;   // Background color
  Color bco2_;  // Pressed background color
  Color pco_;   // Foreground color
  Color pco2_;  // Pressed foreground color
  uint8_t font_id_ = 0;

  /**
   * @brief Component state management using compact bitfield structure
   *
   * Stores all component state flags and properties in a single 16-bit bitfield
   * for efficient memory usage and improved cache locality.
   *
   * Each component property maintains two state flags:
   * - needs_update: Indicates the property requires synchronization with the display
   * - is_set: Tracks whether the property has been explicitly configured
   *
   * The visible field stores both the update flags and the actual visibility state.
   */
  struct ComponentState {
    // Background color flags
    uint16_t bco_needs_update : 1;
    uint16_t bco_is_set : 1;

    // Pressed background color flags
    uint16_t bco2_needs_update : 1;
    uint16_t bco2_is_set : 1;

    // Foreground color flags
    uint16_t pco_needs_update : 1;
    uint16_t pco_is_set : 1;

    // Pressed foreground color flags
    uint16_t pco2_needs_update : 1;
    uint16_t pco2_is_set : 1;

    // Font ID flags
    uint16_t font_id_needs_update : 1;
    uint16_t font_id_is_set : 1;

    // Visibility flags
    uint16_t visible_needs_update : 1;
    uint16_t visible_is_set : 1;
    uint16_t visible : 1;  // Actual visibility state

    // Reserved bits for future expansion
    uint16_t reserved : 3;
  } component_flags_;
};
}  // namespace nextion
}  // namespace esphome
