import esphome.codegen as cg
from esphome.components import text
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_ESPECTRE_ID,
    CONF_TRAFFIC_GENERATOR_TARGET_MAC,
    DOMAIN,
    ESPectreComponent,
    espectre_ns,
    supported_traffic_generator_modes,
)

DEPENDENCIES = ["espectre"]

# "auto" or a MAC address such as aa:bb:cc:dd:ee:ff; the runtime rejects one that is not unicast.
TARGET_MAC_MIN_LENGTH = 4
TARGET_MAC_MAX_LENGTH = 17
TARGET_MAC_PATTERN = "^(auto|([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})$"

TargetMacText = espectre_ns.class_("TargetMacText", text.Text)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESPECTRE_ID): cv.use_id(ESPectreComponent),
        cv.Required(CONF_TRAFFIC_GENERATOR_TARGET_MAC): text.text_schema(
            TargetMacText,
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
            "which is not supported on ESP32-C6 or with the ht_vht CSI capture profile"
        )


FINAL_VALIDATE_SCHEMA = cv.All(
    fv.unique_platform_key(
        domain="text",
        platform=DOMAIN,
        keys=(CONF_ESPECTRE_ID,),
        message="Only one traffic_generator_target_mac text is allowed for ESPectre hub '{}'",
    ),
    _final_validate,
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ESPECTRE_ID])
    conf = config[CONF_TRAFFIC_GENERATOR_TARGET_MAC]
    var = cg.new_Pvariable(conf[CONF_ID], hub)
    await text.register_text(
        var,
        conf,
        min_length=TARGET_MAC_MIN_LENGTH,
        max_length=TARGET_MAC_MAX_LENGTH,
        pattern=TARGET_MAC_PATTERN,
    )
    cg.add(hub.set_target_mac_text(var))
