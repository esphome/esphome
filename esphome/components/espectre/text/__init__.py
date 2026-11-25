import esphome.codegen as cg
from esphome.components import text
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_ESPECTRE_ID,
    CONF_TRAFFIC_GENERATOR_TARGET_MAC,
    WIFI_RAW_UNSUPPORTED,
    ESPectreComponent,
    supported_traffic_generator_modes,
)

DEPENDENCIES = ["espectre"]

# "auto" or a MAC address such as aa:bb:cc:dd:ee:ff; the runtime rejects one that is not unicast.
TARGET_MAC_MIN_LENGTH = 4
TARGET_MAC_MAX_LENGTH = 17
TARGET_MAC_PATTERN = "^(auto|([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})$"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESPECTRE_ID): cv.use_id(ESPectreComponent),
        cv.Required(CONF_TRAFFIC_GENERATOR_TARGET_MAC): text.text_schema(
            text.ApplyText,
            entity_category=ENTITY_CATEGORY_CONFIG,
            mode="TEXT",
        ),
    }
)


def _final_validate(config: ConfigType) -> None:
    hub_config = fv.get_declaring_config(config[CONF_ESPECTRE_ID])
    if "wifi_raw" not in supported_traffic_generator_modes(hub_config):
        raise cv.Invalid(
            f"The {CONF_TRAFFIC_GENERATOR_TARGET_MAC} text requires wifi_raw traffic, "
            f"which is {WIFI_RAW_UNSUPPORTED}"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ESPECTRE_ID])
    texts = text.sub_texts(config, parent=hub)
    # The hub publishes the new target once the runtime has applied it
    await texts(
        CONF_TRAFFIC_GENERATOR_TARGET_MAC,
        hub.set_target_mac_text,
        apply="request_traffic_generator_target_mac(value)",
        min_length=TARGET_MAC_MIN_LENGTH,
        max_length=TARGET_MAC_MAX_LENGTH,
        pattern=TARGET_MAC_PATTERN,
    )
    cg.add_define("USE_ESPECTRE_TARGET_MAC_TEXT")
