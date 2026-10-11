#pragma once

#include "esphome/core/defines.h"

#ifdef USE_API_WIZARD

#include <cstddef>
#include <cstdint>

#include "esphome/core/hal.h"
#include "proto.h"
#include "esphome/core/string_ref.h"

namespace esphome::api {

class WizardInputSetRequest;

/// Size of the buffer holding the entity id of a wizard input. Home Assistant entity ids are at most 255 bytes.
static constexpr size_t WIZARD_ENTITY_ID_BUFFER_SIZE = 256;

/// The wizard as zstd compressed JSON (see DeviceWizardResponse), built by the generated code
/// (components/api/wizard.py) and kept in flash. API_WIZARD_DATA_SIZE bytes long.
extern const uint8_t API_WIZARD_DATA[] PROGMEM;

#ifdef USE_API_WIZARD_INPUTS
/// Where the entity id of an input is kept, found by the key the client uses for it.
struct WizardInputEntry {
  uint32_t key;     // FNV-1 hash of the ESPHome id of the input
  char *entity_id;  // RAM buffer of WIZARD_ENTITY_ID_BUFFER_SIZE bytes, shared with the homeassistant entity
};

/// The inputs of the wizard, in flash. API_WIZARD_INPUT_COUNT entries long.
extern const WizardInputEntry API_WIZARD_INPUTS[] PROGMEM;

/// Apply a WizardInputSetRequest: validate it and copy the entity id into the input's buffer. Nothing is stored
/// across restarts, so the client sends the choices again after every connection.
/// Returns the buffer, or nullptr when the request was ignored.
const char *wizard_set_input(const WizardInputSetRequest &msg);
#endif

#ifdef USE_API_WIZARD_STANDALONE_INPUTS
/// An input of the wizard that is not tied to an entity of the device. The entity ID the user picks is only ever read
/// by lambdas, for example `id(input).entity_id()`. It lives in the same RAM buffer a linked input uses.
class WizardInput {
 public:
  explicit WizardInput(const char *entity_id) : entity_id_(entity_id) {}
  /// The Home Assistant entity ID, empty until the wizard sets one.
  StringRef entity_id() const { return StringRef(this->entity_id_); }
  bool has_entity_id() const { return this->entity_id_[0] != '\0'; }

 protected:
  const char *entity_id_;
};
#endif

}  // namespace esphome::api

#endif  // USE_API_WIZARD
