import base64
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_KEY
from esphome.core import CORE
from esphome.components.esp32 import add_idf_sdkconfig_option

DEPENDENCIES = ["esp32"]
CONFLICTS_WITH = ["esp32_ble_tracker", "esp32_ble_beacon"]

openhaystack_ns = cg.esphome_ns.namespace("openhaystack")
OpenHaystack = openhaystack_ns.class_("OpenHaystack", cg.Component)


def _validate_key(value):
    try:
        key = base64.b64decode(value[CONF_KEY])
        if len(key) != 28:
            raise cv.Invalid(
                "Invalid key size. Make sure you're using the base64 advertisement key."
            )
    except base64.binascii.Error as base64_error:
        raise cv.Invalid(
            "Invalid key. Make sure you're using the base64 advertisement key."
        ) from base64_error

    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenHaystack),
            cv.Required(CONF_KEY): cv.string,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_key,
)


async def to_code(config):
    encoded_adv_pub_key = config[CONF_KEY]
    adv_pub_key = base64.b64decode(encoded_adv_pub_key).hex()
    adv_pub_key_arr = [
        int(adv_pub_key[i : i + 2], 16) for i in range(0, len(adv_pub_key), 2)
    ]
    var = cg.new_Pvariable(config[CONF_ID], adv_pub_key_arr)
    await cg.register_component(var, config)

    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
