#include "api_wizard.h"

#ifdef USE_API_WIZARD

#include <array>
#include <cstring>

#include "api_pb2.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/core/string_ref.h"

namespace esphome::api {

static const char *const TAG = "api.wizard";

#ifdef USE_API_WIZARD_INPUTS
// Mixed into the input key so the saved entity ids cannot share a preference key with an entity
static constexpr uint32_t WIZARD_PREFERENCE_SALT = 0x57495A44;

// The terminator is not saved, so the record is 255 bytes
struct WizardSavedEntityId {
  char entity_id[WIZARD_ENTITY_ID_BUFFER_SIZE - 1];
};

static std::array<ESPPreferenceObject, API_WIZARD_INPUT_COUNT> wizard_preferences;  // NOLINT
#endif

#ifdef API_WIZARD_FLASH_STRINGS
size_t wizard_copy_flash_string(const char *flash, char *dst, size_t size) {
  size_t length = 0;
  if (flash != nullptr) {
    while (length < size - 1 && progmem_read_byte(reinterpret_cast<const uint8_t *>(flash + length)) != '\0')
      length++;
    progmem_memcpy(dst, flash, length);
  }
  dst[length] = '\0';
  return length;
}

// Copy a PROGMEM string to the front of scratch and advance scratch past it
static StringRef wizard_string(const char *flash, std::span<char> &scratch) {
  size_t length = wizard_copy_flash_string(flash, scratch.data(), scratch.size());
  StringRef ref(scratch.data(), length);
  scratch = scratch.subspan(length + 1);
  return ref;
}
#else
static StringRef wizard_string(const char *text, std::span<char> & /*scratch*/) { return StringRef(text); }
#endif

#ifdef USE_API_WIZARD_INPUTS
// Rows are read from PROGMEM on ESP8266, so a row is always copied out before use
template<typename Row> static Row wizard_read_row(const Row *rows, size_t index) {
#ifdef API_WIZARD_FLASH_STRINGS
  Row row;
  progmem_memcpy(&row, rows + index, sizeof(Row));
  return row;
#else
  return rows[index];
#endif
}
#endif

#ifdef USE_API_WIZARD_ENTITY_FILTERS
void wizard_fill(WizardEntityFilter &msg, const WizardFilterRow &row, std::span<char> scratch) {
  msg.integration = wizard_string(row.integration, scratch);
  msg.domain = &row.domain;
  msg.device_class = &row.device_class;
  msg.supported_features = &row.supported_features;
}
#endif

#ifdef USE_API_WIZARD_ENTITIES
void wizard_fill(WizardEntityField &msg, const WizardEntityRow &row, std::span<char> scratch) {
  const EntityBase *entity = row.entity();
  msg.key = entity->get_object_id_hash();
#ifdef USE_DEVICES
  msg.device_id = entity->get_device_id();
#endif
  msg.description = wizard_string(row.description, scratch);
}
#endif

#ifdef USE_API_WIZARD_INPUTS
void wizard_fill(WizardInputField &msg, const WizardInputRow &row, std::span<char> scratch) {
  msg.key = row.key;
  msg.description = wizard_string(row.description, scratch);
#ifdef USE_API_WIZARD_ENTITY_FILTERS
  msg.entity_filters = &row.filters;
#endif
  msg.entity_id = StringRef(row.entity_id);
}
#endif

void wizard_fill(WizardPage &msg, const WizardPageRow &row, std::span<char> scratch) {
  msg.title = wizard_string(row.title, scratch);
  msg.description = wizard_string(row.description, scratch);
#ifdef USE_API_WIZARD_ENTITIES
  msg.entities = &row.entities;
#endif
#ifdef USE_API_WIZARD_INPUTS
  msg.inputs = &row.inputs;
#endif
}

#ifdef USE_API_WIZARD_INPUTS

// Call fn(index, row) for each input in page order until it returns true
template<typename F> static void wizard_for_each_input(F &&fn) {
  size_t index = 0;
  for (size_t p = 0; p < API_WIZARD_PAGES.count; p++) {
    const WizardPageRow page = wizard_read_row(API_WIZARD_PAGES.data, p);
    for (size_t i = 0; i < page.inputs.count; i++) {
      if (fn(index++, wizard_read_row(page.inputs.data, i)))
        return;
    }
  }
}

static bool wizard_entity_id_valid(const char *entity_id, size_t length) {
  return length > 0 && length < WIZARD_ENTITY_ID_BUFFER_SIZE && memchr(entity_id, '.', length) != nullptr;
}

void wizard_setup() {
  wizard_for_each_input([](size_t index, const WizardInputRow &row) {
    wizard_preferences[index] =
        global_preferences->make_preference<WizardSavedEntityId>(row.key ^ WIZARD_PREFERENCE_SALT, true);
    WizardSavedEntityId saved;
    if (wizard_preferences[index].load(&saved)) {
      size_t length = strnlen(saved.entity_id, sizeof(saved.entity_id));
      if (wizard_entity_id_valid(saved.entity_id, length)) {
        memcpy(row.entity_id, saved.entity_id, length);
        row.entity_id[length] = '\0';
        ESP_LOGD(TAG, "Loaded saved entity id for wizard input %u", static_cast<unsigned>(index));
      }
    }
    return false;
  });
}

const char *wizard_set_input(const WizardInputSetRequest &msg) {
  if (!wizard_entity_id_valid(msg.entity_id.c_str(), msg.entity_id.size())) {
    ESP_LOGW(TAG, "Ignoring an invalid entity id for wizard input");
    return nullptr;
  }
  const char *result = nullptr;
  wizard_for_each_input([&](size_t index, const WizardInputRow &row) {
    if (row.key != msg.key)
      return false;
    memcpy(row.entity_id, msg.entity_id.c_str(), msg.entity_id.size());
    row.entity_id[msg.entity_id.size()] = '\0';
    WizardSavedEntityId saved{};
    memcpy(saved.entity_id, msg.entity_id.c_str(), msg.entity_id.size());
    if (!wizard_preferences[index].save(&saved) || !global_preferences->sync()) {
      ESP_LOGW(TAG, "Failed to save the entity id of wizard input");
    }
    result = row.entity_id;
    return true;
  });
  if (result == nullptr) {
    ESP_LOGW(TAG, "Ignoring an entity id for an unknown wizard input");
  }
  return result;
}
#endif

}  // namespace esphome::api

#endif  // USE_API_WIZARD
