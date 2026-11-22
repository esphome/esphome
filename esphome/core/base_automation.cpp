#include "esphome/core/base_automation.h"

#include <cstring>

namespace esphome {

#ifdef ESPHOME_PROJECT_NAME
void ProjectUpdateComponent::setup() {
  uint32_t hash = fnv1_hash(ESPHOME_PROJECT_NAME);
  ESPPreferenceObject pref = global_preferences->make_preference<char[30]>(hash, true);
  char previous_version[30];
  char current_version[30] = ESPHOME_PROJECT_VERSION_30;
  if (pref.load(&previous_version) && strcmp(previous_version, current_version) < 0)
    this->fire_(previous_version);
  pref.save(&current_version);
  global_preferences->sync();
}
#endif

}  // namespace esphome
