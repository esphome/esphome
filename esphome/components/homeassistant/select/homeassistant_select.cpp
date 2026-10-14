#include "homeassistant_select.h"

#include <cstring>

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.select");

namespace {

/// Emit the UTF-8 encoding of a code point. Rejects NUL, which would end the option early.
template<typename C> bool emit_utf8(uint32_t code_point, C &on_char) {
  if (code_point == 0 || code_point > 0x10FFFF)
    return false;
  if (code_point < 0x80) {
    on_char(static_cast<char>(code_point));
  } else if (code_point < 0x800) {
    on_char(static_cast<char>(0xC0 | (code_point >> 6)));
    on_char(static_cast<char>(0x80 | (code_point & 0x3F)));
  } else if (code_point < 0x10000) {
    on_char(static_cast<char>(0xE0 | (code_point >> 12)));
    on_char(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    on_char(static_cast<char>(0x80 | (code_point & 0x3F)));
  } else {
    on_char(static_cast<char>(0xF0 | (code_point >> 18)));
    on_char(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
    on_char(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    on_char(static_cast<char>(0x80 | (code_point & 0x3F)));
  }
  return true;
}

/** Walk a list attribute as Home Assistant sends it: the Python repr of a list of strings, such as
 * `['Low', "It's high", 'caf\xe9']`.
 *
 * Calls on_char for each decoded byte of an option and on_option at the end of each option. Returns false
 * on malformed input; the callbacks may already have run by then.
 */
template<typename C, typename O> bool parse_option_list(StringRef src, C &&on_char, O &&on_option) {
  const char *pos = src.c_str();
  const char *const end = pos + src.size();
  auto skip_spaces = [&]() {
    while (pos < end && *pos == ' ')
      pos++;
  };

  skip_spaces();
  if (pos == end || *pos != '[')
    return false;
  pos++;
  skip_spaces();
  if (pos < end && *pos == ']') {
    pos++;
  } else {
    while (true) {
      skip_spaces();
      if (pos == end || (*pos != '\'' && *pos != '"'))
        return false;
      const char quote = *pos++;
      while (true) {
        if (pos == end)
          return false;
        const char c = *pos++;
        if (c == quote)
          break;
        if (c != '\\') {
          on_char(c);
          continue;
        }
        if (pos == end)
          return false;
        const char escape = *pos++;
        switch (escape) {
          case '\\':
          case '\'':
          case '"':
            on_char(escape);
            break;
          case 'n':
            on_char('\n');
            break;
          case 'r':
            on_char('\r');
            break;
          case 't':
            on_char('\t');
            break;
          case 'x':
          case 'u':
          case 'U': {
            const size_t digits = escape == 'x' ? 2 : (escape == 'u' ? 4 : 8);
            if (static_cast<size_t>(end - pos) < digits)
              return false;
            auto code_point = parse_hex<uint32_t>(pos, digits);
            if (!code_point.has_value() || !emit_utf8(*code_point, on_char))
              return false;
            pos += digits;
            break;
          }
          default:
            return false;
        }
      }
      on_option();
      skip_spaces();
      if (pos == end)
        return false;
      const char separator = *pos++;
      if (separator == ']')
        break;
      if (separator != ',')
        return false;
    }
  }
  skip_spaces();
  return pos == end;
}

}  // namespace

void HomeassistantSelect::setup() {
  this->options_buffer_ = std::make_unique<char[]>(this->options_buffer_size_);
  this->option_list_ = std::make_unique<const char *[]>(this->max_options_);

  // Subscribe to the options first: Home Assistant answers subscriptions in order, so the options are
  // known by the time the first state arrives.
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, "options", [this](StringRef options) { this->options_changed_(options); });
  api::global_api_server->subscribe_home_assistant_state(this->entity_id_, nullptr,
                                                         [this](StringRef state) { this->state_changed_(state); });
}

void HomeassistantSelect::options_changed_(StringRef options) {
  // First pass: validate and measure the new options, and look for the active option among them, before
  // anything is overwritten. A list that does not fit leaves the current options in place.
  const char *active = this->has_state() ? this->option_at(this->active_index_) : nullptr;
  size_t count = 0;
  size_t bytes = 0;
  size_t match_len = 0;
  bool matching = active != nullptr;
  optional<size_t> new_active;
  bool valid = parse_option_list(
      options,
      [&](char c) {
        bytes++;
        if (matching && active[match_len] == c) {
          match_len++;
        } else {
          matching = false;
        }
      },
      [&]() {
        bytes++;  // NUL terminator
        if (matching && active[match_len] == '\0' && !new_active.has_value())
          new_active = count;
        count++;
        match_len = 0;
        matching = active != nullptr;
      });
  if (!valid) {
    ESP_LOGW(TAG, "'%s': Can't parse options %s", this->entity_id_, options.c_str());
    return;
  }
  if (count > this->max_options_) {
    ESP_LOGE(TAG, "'%s': %zu options exceed max_options (%u)", this->entity_id_, count, this->max_options_);
    return;
  }
  if (bytes > this->options_buffer_size_) {
    ESP_LOGE(TAG, "'%s': Options need %zu bytes, more than options_buffer_size (%u)", this->entity_id_, bytes,
             this->options_buffer_size_);
    return;
  }

  // Second pass: the input is known to be valid and to fit, so write it out
  char *buffer = this->options_buffer_.get();
  size_t offset = 0;
  size_t option_start = 0;
  size_t index = 0;
  parse_option_list(
      options, [&](char c) { buffer[offset++] = c; },
      [&]() {
        buffer[offset++] = '\0';
        this->option_list_[index++] = buffer + option_start;
        option_start = offset;
      });
  // The traits only point at the list, which this select owns; set_options() would copy it to the heap
  this->traits.set_options_static(this->option_list_.get(), count);

  // The active option may have moved to another index, or may be gone
  if (new_active.has_value()) {
    this->active_index_ = *new_active;
  } else {
    this->set_has_state(false);
  }
  ESP_LOGD(TAG, "'%s': Got %zu options", this->entity_id_, count);
}

void HomeassistantSelect::state_changed_(StringRef state) {
  auto index = this->index_of(state.c_str(), state.size());
  if (!index.has_value()) {
    ESP_LOGW(TAG, "'%s': State '%s' is not one of the options", this->entity_id_, state.c_str());
    return;
  }
  if (this->has_state() && this->active_index_ == *index)
    return;
  ESP_LOGD(TAG, "'%s': Got state '%s'", this->entity_id_, state.c_str());
  this->publish_state(*index);
}

void HomeassistantSelect::dump_config() {
  LOG_SELECT("", "Homeassistant Select", this);
  ESP_LOGCONFIG(TAG,
                "  Entity ID: '%s'\n"
                "  Max options: %u\n"
                "  Options buffer size: %u",
                this->entity_id_, this->max_options_, this->options_buffer_size_);
}

float HomeassistantSelect::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

void HomeassistantSelect::control(size_t index) {
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(TAG, "No clients connected to API server");
    return;
  }

#ifdef USE_API_WIZARD_LINKED_INPUTS
  if (this->entity_id_[0] == '\0') {
    ESP_LOGW(TAG, "'%s': No entity ID set yet", this->get_name().c_str());
    return;
  }
#endif

  static constexpr auto SERVICE_SELECT = StringRef::from_lit("select.select_option");
  static constexpr auto SERVICE_INPUT_SELECT = StringRef::from_lit("input_select.select_option");
  static constexpr auto ENTITY_ID_KEY = StringRef::from_lit("entity_id");
  static constexpr auto OPTION_KEY = StringRef::from_lit("option");
  static constexpr char INPUT_PREFIX[] = "input_";

  api::HomeassistantActionRequest resp;
  if (strncmp(this->entity_id_, INPUT_PREFIX, sizeof(INPUT_PREFIX) - 1) == 0) {
    resp.service = SERVICE_INPUT_SELECT;
  } else {
    resp.service = SERVICE_SELECT;
  }

  resp.data.init(2);
  auto &entity_id = resp.data.emplace_back();
  entity_id.key = ENTITY_ID_KEY;
  entity_id.value = StringRef(this->entity_id_);

  auto &option = resp.data.emplace_back();
  option.key = OPTION_KEY;
  option.value = StringRef(this->option_at(index));

  api::global_api_server->send_homeassistant_action(resp);
}

}  // namespace esphome::homeassistant
