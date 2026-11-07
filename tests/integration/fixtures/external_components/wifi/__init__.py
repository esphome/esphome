"""Host-only stub of the wifi component for integration tests.

HOST-ONLY TEST COMPONENT: this shadows the real wifi component for EVERY
fixture that uses the shared external_components directory. Any host fixture
with a wifi block gets this stub, not the real component: fixed scan results,
is_connected() hardwired true, and save_wifi_sta that only logs. See
wifi_component.h for the full behavior.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_SSID, CONF_USE_ADDRESS
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/tests"]

wifi_ns = cg.esphome_ns.namespace("wifi")
WiFiComponent = wifi_ns.class_("WiFiComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WiFiComponent),
        # Accepted for fixture realism; the stub ignores them
        cv.Optional(CONF_SSID): cv.string,
        cv.Optional(CONF_PASSWORD): cv.string,
        # Read by StorageJSON via CORE.address whenever a wifi block exists
        cv.Optional(CONF_USE_ADDRESS, default="localhost"): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


def check_placeholder_credentials(config: ConfigType) -> None:
    """Compile-time hook the esphome CLI imports from the wifi module; no-op here."""


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_WIFI")
