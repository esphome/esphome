"""Reproduces issue #16798: publishing entity state from inside ~APIConnection().

When an API client is torn down, ~APIConnection() runs while the client's slot is
mid-removal. On real hardware voice_assistant unsubscribes there and its
on_client_disconnected automation publishes a light, which reenters
APIServer::on_light_update() and dereferences the half-removed (null) client slot.

voice_assistant cannot be built on the host platform (it pulls in the ESP32-only
audio component), so this test component installs a tiny destroy hook
(api::api_connection_destroy_test_hook, compiled in only when
USE_API_CONNECTION_TEST_HOOKS is defined) that publishes a light from inside
~APIConnection(), reproducing the exact reentrancy.
"""

import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LIGHT_ID
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/tests"]

api_connection_destroy_light_ns = cg.esphome_ns.namespace(
    "api_connection_destroy_light"
)
ApiConnectionDestroyLight = api_connection_destroy_light_ns.class_(
    "ApiConnectionDestroyLight", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ApiConnectionDestroyLight),
        cv.Required(CONF_LIGHT_ID): cv.use_id(light.LightState),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_API_CONNECTION_TEST_HOOKS")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    light_var = await cg.get_variable(config[CONF_LIGHT_ID])
    cg.add(var.set_light(light_var))
