from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt
from esphome.components.const import CONF_OUT_PIN
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@fornellas"]
DOMAIN = "opentherm42"
MULTI_CONF = True

CONF_IN_PIN = "in_pin"

opentherm42_ns = cg.esphome_ns.namespace("opentherm42")
OpenTherm42Hub = opentherm42_ns.class_("OpenTherm42Hub", cg.Component)


def _validate_rmt_available(config: ConfigType) -> ConfigType:
    """Rejects ESP32 variants without RMT hardware -- datalink_rmt.cpp is the only datalink backend."""
    if (variant := esp32.get_esp32_variant()) in esp32_rmt.VARIANTS_NO_RMT:
        raise cv.Invalid(
            f"opentherm42 needs the RMT peripheral, which {variant} doesn't have"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenTherm42Hub),
            cv.Required(CONF_IN_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_OUT_PIN): pins.internal_gpio_output_pin_schema,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    # datalink_rmt.cpp is the only datalink backend, and it needs the ESP32 RMT peripheral.
    cv.only_on_esp32,
    _validate_rmt_available,
)


async def to_code(config: dict) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await cg.add_pin(var.set_in_pin, config[CONF_IN_PIN])
    await cg.add_pin(var.set_out_pin, config[CONF_OUT_PIN])

    esp32.include_builtin_idf_component("esp_driver_rmt")
