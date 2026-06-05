#include "api_connection_destroy_light.h"

#include "esphome/components/api/api_connection.h"

namespace esphome::api_connection_destroy_light {

// The destroy hook is a plain function pointer, so stash the light in a file-local
// pointer for it to reach. Only one instance is ever configured in the test.
static light::LightState *g_light = nullptr;

static void publish_light_on_connection_destroy(api::APIConnection * /*conn*/) {
  if (g_light != nullptr) {
    // Reentrant publish from inside ~APIConnection(): drives APIServer::on_light_update()
    // while the disconnecting client's slot is mid-removal (issue #16798).
    g_light->publish_state();
  }
}

void ApiConnectionDestroyLight::setup() {
  g_light = this->light_;
  api::api_connection_destroy_test_hook = &publish_light_on_connection_destroy;
}

}  // namespace esphome::api_connection_destroy_light
