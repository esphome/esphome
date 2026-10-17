#include "pylontech.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

// Helper macros
#define PARSE_TOKEN_INT(field, field_name) \
  { \
    auto val = parse_number<int>(token_buf); \
    if (val.has_value() && in_range<decltype(field)>(*val)) { \
      (field) = *val; \
    } else { \
      ESP_LOGV(TAG, "invalid " field_name " in line %s", buffer); \
      this->reject_line_(); \
      return; \
    } \
  }

#define PARSE_INT(field, field_name) \
  { \
    get_token(cursor, token_buf); \
    PARSE_TOKEN_INT(field, field_name) \
  }

#define PARSE_STR(field, field_name) \
  { \
    get_token(cursor, field); \
    if (strlen(field) < 2) { \
      ESP_LOGV(TAG, "too short " field_name " in line %s", buffer); \
      this->reject_line_(); \
      return; \
    } \
  }

namespace esphome::pylontech {

ESPHOME_LOG_TAG(TAG, "pylontech");

#ifdef PYLONTECH_CELL_BATTERY_COUNT
// The pack drops a command that follows its reply without a pause
static constexpr uint32_t BAT_COMMAND_DELAY_MS = 20;
#endif

// Rejects values that would wrap in a narrowed field
template<typename T> static bool in_range(int value) {
  if constexpr (sizeof(T) >= sizeof(int)) {
    return true;
  } else {
    return value >= std::numeric_limits<T>::min() && value <= std::numeric_limits<T>::max();
  }
}

#ifdef PYLONTECH_LISTENER_COUNT
// Copies the next whitespace separated token, at most TEXT_SENSOR_MAX_LEN - 1 characters, and moves cursor past it
static void get_token(const char *&cursor, char *token_buf) {
  // Skip leading whitespace
  while (*cursor == ' ' || *cursor == '\t') {
    cursor++;
  }

  if (*cursor == '\0') {
    token_buf[0] = 0;
    return;
  }

  const char *start = cursor;

  // Find end of field
  while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
    cursor++;
  }

  size_t token_len = std::min(static_cast<size_t>(cursor - start), static_cast<size_t>(TEXT_SENSOR_MAX_LEN - 1));
  memcpy(token_buf, start, token_len);
  token_buf[token_len] = 0;
}

// Slots are kept in battery order, so a battery's slot index is the number of lower batteries in the mask
static uint8_t slot_index(battery_mask_t batteries, uint8_t bat_index) {
  return __builtin_popcount(batteries & ((1u << bat_index) - 1u));
}
#endif

PylontechComponent::PylontechComponent() {}

void PylontechComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "pylontech:");

#ifdef PYLONTECH_LISTENER_COUNT
  for (PylontechListener *listener : this->listeners_) {
    listener->dump_config();
  }
#endif

  LOG_UPDATE_INTERVAL(this);
}

void PylontechComponent::setup() {
  while (this->available() != 0) {
    this->read();
  }
}

void PylontechComponent::update() {
#ifdef PYLONTECH_CELL_BATTERY_COUNT
  // A "bat N" that was not sent yet, or whose reply never finished, must not get in the way of "pwr"
  this->bat_command_due_ = false;
  this->late_reply_ = this->bat_reply_ != 0;
  this->bat_reply_ = 0;
#endif
  this->write_str("pwr\n");
}

void PylontechComponent::loop() {
  // Drain the UART quickly; pylontech sends large bursts. Logging and publishing wait until it is idle.
  size_t avail = this->available();
#ifdef PYLONTECH_LISTENER_COUNT
  if (avail == 0) {
#ifdef PYLONTECH_CELL_BATTERY_COUNT
    if (this->bat_command_due_ &&
        static_cast<int32_t>(App.get_loop_component_start_time() - this->bat_command_at_) >= 0) {
      this->bat_command_due_ = false;
      if (this->cell_polling_enabled_) {
        char cmd[8];  // "bat 16\n"
        snprintf(cmd, sizeof(cmd), "bat %u\n", this->bat_reply_);
        this->write_str(cmd);
      } else {
        this->bat_reply_ = 0;  // turned off during the pause: nothing is coming
      }
    }
#endif
    this->dispatch_one_();
    return;
  }
#endif
  uint8_t buf[64];
  while (avail > 0) {
    size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read)) {
      break;
    }
    avail -= to_read;
#ifdef PYLONTECH_LISTENER_COUNT
    for (size_t i = 0; i < to_read; i++) {
      const char *line = this->line_reader_.feed(buf[i]);
      if (line != nullptr) {
        this->process_line_(line);
      } else if (this->line_reader_.overflowed()) {
        this->line_reader_.log_if_overflowed(TAG);
        // Tlow.Id sits near the start, so a too long header still sets the column layout
        const char *partial = this->line_reader_.partial();
        if (strncmp(partial, "Power", 5) == 0) {
          this->parse_header_(partial);
        }
      }
    }
#endif
  }
}

#ifdef PYLONTECH_LISTENER_COUNT
void PylontechComponent::dispatch_one_() {
  for (uint8_t i = 0; this->pending_ != 0 && i < PYLONTECH_BATTERY_COUNT; i++) {
    const battery_mask_t bit = 1u << i;
    if ((this->pending_ & bit) == 0) {
      continue;
    }
    this->pending_ &= ~bit;
    PylontechListener::LineContents *l = &this->slots_[i];
    ESP_LOGV(TAG, "Publishing battery %u", l->bat_num);
    for (PylontechListener *listener : this->listeners_) {
      if (listener->get_battery() == l->bat_num) {
        listener->on_line_read(l);
      }
    }
    return;
  }
#ifdef PYLONTECH_CELL_BATTERY_COUNT
  if (this->cells_pending_ != 0) {
    const uint8_t bat_index = __builtin_ctz(this->cells_pending_);
    this->cells_pending_ &= ~(1u << bat_index);
    const uint8_t slot = slot_index(this->cell_batteries_, bat_index);
    ESP_LOGV(TAG, "Publishing cells of battery %u", bat_index + 1);
    for (PylontechListener *listener : this->listeners_) {
      if (listener->get_battery() == bat_index + 1) {
        listener->on_cells_read(this->cell_slots_[slot], this->cells_read_[slot]);
      }
    }
    return;
  }
#endif
  if (this->rejected_ != 0) {
    ESP_LOGW(TAG, "Rejected %u malformed lines", this->rejected_);
    this->rejected_ = 0;
  }
}

void PylontechComponent::parse_header_(const char *buffer) {
  this->has_tlow_id_ = strstr(buffer, "Tlow.Id") != nullptr;
#ifdef PYLONTECH_CELL_BATTERY_COUNT
  // The header opens a "pwr" reply, so what follows are battery lines even after a late "bat N" reply
  this->bat_reply_ = 0;
  this->late_reply_ = false;
  this->pwr_reply_ = true;
#endif
  ESP_LOGV(TAG, "header line %s Tlow.Id: %s", this->has_tlow_id_ ? LOG_STR_LITERAL("with") : LOG_STR_LITERAL("without"),
           buffer);
}

// Runs while the UART drains: no logging above VERBOSE and no publishing here
void PylontechComponent::process_line_(const char *buffer) {
#ifdef PYLONTECH_CELL_BATTERY_COUNT
  if (strcmp(buffer, "$$") == 0) {
    // Every reply ends with "$$"; after the "pwr" reply one "bat N" follows per battery with cell sensors.
    // A "$$" that ends neither of the two, such as a late reply to an abandoned "bat N", sends nothing.
    if (this->bat_reply_ != 0) {
      this->cells_pending_ |= 1u << (this->bat_reply_ - 1);
      this->send_next_bat_();
    } else if (this->pwr_reply_) {
      this->pwr_reply_ = false;
      this->send_next_bat_();
    }
    return;
  }
  // The "pwr" header always starts a "pwr" reply, whatever was expected
  if (this->bat_reply_ != 0 && strncmp(buffer, "Power", 5) != 0) {
    this->parse_cell_line_(buffer);
    return;
  }
#endif
  // clang-format off
  // example lines to parse:
  // Power Volt   Curr   Tempr  Tlow   Thigh  Vlow   Vhigh  Base.St  Volt.St  Curr.St  Temp.St  Coulomb  Time                 B.V.St   B.T.St   MosTempr M.T.St
  // 1     50548  8910   25000  24200  25000  3368   3371   Charge   Normal   Normal   Normal   97%      2021-06-30 20:49:45  Normal  Normal  22700    Normal
  // 1     46012  1255   9100   5300   5500   3047   3091   SysError Low      Normal   Normal   4%       2025-11-28 17:56:33  Low      Normal  7800     Normal
  // newer firmware example:
  // Power Volt Curr Tempr Tlow Tlow.Id Thigh Thigh.Id Vlow Vlow.Id Vhigh Vhigh.Id Base.St Volt.St Curr.St Temp.St Coulomb Time                B.V.St B.T.St MosTempr M.T.St SysAlarm.St
  // 1     49405 0   17600 13700 8      14500 0        3293 2       3294   0       Idle    Normal  Normal  Normal  60%     2025-12-05 00:53:41 Normal Normal 16600    Normal Normal
  // clang-format on

  const char *cursor = buffer;
  char token_buf[TEXT_SENSOR_MAX_LEN] = {0};

  get_token(cursor, token_buf);
  auto bat = parse_number<int>(token_buf);
  if (!bat.has_value()) {
    if (strcmp(token_buf, "Power") == 0) {
      // header line i.e. "Power Volt   Curr" and so on
      this->parse_header_(buffer);
    } else {
      ESP_LOGV(TAG, "unknown line %s", buffer);
    }
    return;
  }
#ifdef PYLONTECH_CELL_BATTERY_COUNT
  if (this->late_reply_) {
    ESP_LOGV(TAG, "dropped late line %s", buffer);  // cell rows of the abandoned "bat N" look like battery lines
    return;
  }
#endif
  const unsigned bat_index = static_cast<unsigned>(*bat) - 1u;
  if (bat_index >= MAX_BATTERIES || (this->batteries_ & (1u << bat_index)) == 0) {
    return;  // no listener for this battery
  }
  const battery_mask_t slot_bit = 1u << slot_index(this->batteries_, bat_index);
  // Cleared until the whole line parses, so a bad line never publishes a half written slot
  this->pending_ &= ~slot_bit;
  PylontechListener::LineContents &l = this->slots_[__builtin_ctz(slot_bit)];
  l.bat_num = *bat;

  PARSE_INT(l.volt, "Volt");
  PARSE_INT(l.curr, "Curr");
  PARSE_INT(l.tempr, "Tempr");
  PARSE_INT(l.tlow, "Tlow");
  if (this->has_tlow_id_) {
    get_token(cursor, token_buf);  // Skip Tlow.Id
  }
  PARSE_INT(l.thigh, "Thigh");
  if (this->has_tlow_id_) {
    get_token(cursor, token_buf);  // Skip Thigh.Id
  }
  PARSE_INT(l.vlow, "Vlow");
  if (this->has_tlow_id_) {
    get_token(cursor, token_buf);  // Skip Vlow.Id
  }
  PARSE_INT(l.vhigh, "Vhigh");
  if (this->has_tlow_id_) {
    get_token(cursor, token_buf);  // Skip Vhigh.Id
  }
  PARSE_STR(l.base_st, "Base.St");
  PARSE_STR(l.volt_st, "Volt.St");
  PARSE_STR(l.curr_st, "Curr.St");
  PARSE_STR(l.temp_st, "Temp.St");
  get_token(cursor, token_buf);
  if (char *pct = strchr(token_buf, '%')) {
    *pct = '\0';
  }
  PARSE_TOKEN_INT(l.coulomb, "Coulomb");
  get_token(cursor, token_buf);  // Skip Date
  get_token(cursor, token_buf);  // Skip Time
  get_token(cursor, token_buf);  // Skip B.V.St
  get_token(cursor, token_buf);  // Skip B.T.St
  PARSE_INT(l.mostempr, "Mostempr");

  this->pending_ |= slot_bit;
}

#ifdef PYLONTECH_CELL_BATTERY_COUNT
// Picks the next cell battery above the one just read and schedules "bat N" for it; the cycle ends when none
// is left or cell polling was turned off
void PylontechComponent::send_next_bat_() {
  // Battery n is bit n - 1, so clearing the low bat_reply_ bits leaves the batteries above it
  const battery_mask_t remaining =
      this->cell_batteries_ & static_cast<battery_mask_t>(~((1u << this->bat_reply_) - 1u));
  if (remaining == 0 || !this->cell_polling_enabled_) {
    this->bat_reply_ = 0;
    return;
  }
  const uint8_t bat_index = __builtin_ctz(remaining);
  this->bat_reply_ = bat_index + 1;
  // The slot is about to be rewritten, so an unpublished record must not go out half filled
  this->cells_pending_ &= ~(1u << bat_index);
  this->cells_read_[slot_index(this->cell_batteries_, bat_index)] = 0;
  // loop() writes the command once the pause has passed and the line is idle
  this->bat_command_due_ = true;
  this->bat_command_at_ = App.get_loop_component_start_time() + BAT_COMMAND_DELAY_MS;
}

// Runs while the UART drains, like process_line_
void PylontechComponent::parse_cell_line_(const char *buffer) {
  // clang-format off
  // example lines to parse, the reply to "bat N":
  // Battery  Volt     Curr     Tempr    Base State   Volt. State  Curr. State  Temp. State  SOC          Coulomb      BAL
  // 0        3308     -914     18300    Dischg       Normal       Normal       Normal       70%          34015 mAH    N
  // clang-format on

  const char *cursor = buffer;
  char token_buf[TEXT_SENSOR_MAX_LEN] = {0};

  get_token(cursor, token_buf);
  auto cell = parse_number<int>(token_buf);
  if (!cell.has_value() || *cell < 0 || *cell >= MAX_CELLS) {
    // the header, the echoed command or "Command completed successfully"
    ESP_LOGV(TAG, "unknown line %s", buffer);
    return;
  }
  PylontechListener::CellContents c{};
  PARSE_INT(c.volt, "Volt");
  get_token(cursor, token_buf);  // Skip Curr
  int32_t tempr;                 // m°C
  PARSE_INT(tempr, "Tempr");
  tempr /= 10;
  if (!in_range<int16_t>(tempr)) {
    ESP_LOGV(TAG, "invalid Tempr in line %s", buffer);
    this->reject_line_();
    return;
  }
  c.tempr = tempr;

  const uint8_t slot = slot_index(this->cell_batteries_, this->bat_reply_ - 1);
  this->cell_slots_[slot][*cell] = c;
  this->cells_read_[slot] |= 1u << *cell;
}
#endif
#endif

}  // namespace esphome::pylontech

#undef PARSE_TOKEN_INT
#undef PARSE_INT
#undef PARSE_STR
