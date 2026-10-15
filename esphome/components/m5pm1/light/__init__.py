import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_NUMBER, CONF_OUTPUT_ID, CONF_PIN
from esphome.types import ConfigType

from .. import BASE_SCHEMA, CONF_M5PM1_ID, M5PM1_GPIO_OUTPUT_PIN_SCHEMA, m5pm1_ns

M5PM1AddressableLight = m5pm1_ns.class_("M5PM1AddressableLight", light.AddressableLight)

CONF_NUM_LED = "num_led"


def _prepare_neopixel_pin(config: ConfigType) -> ConfigType:
    """Add the fixed M5PM1 NeoPixel GPIO when it is omitted."""
    m5pm1_id = config.get(CONF_M5PM1_ID)
    pin = config.get(CONF_PIN)
    if pin is None:
        pin = {}
    elif not isinstance(pin, dict):
        raise cv.Invalid(f"'{CONF_PIN}' must be a mapping")

    pin = dict(pin)
    pin.setdefault(CONF_M5PM1_ID, m5pm1_id)
    pin.setdefault(CONF_NUMBER, 0)
    config[CONF_PIN] = pin
    return config


def _validate_neopixel_pin(config: ConfigType) -> ConfigType:
    pin = config[CONF_PIN]
    if pin[CONF_NUMBER] != 0:
        raise cv.Invalid(
            "M5PM1 NeoPixel only supports GPIO0", path=[CONF_PIN, CONF_NUMBER]
        )
    return config


CONFIG_SCHEMA = cv.All(
    _prepare_neopixel_pin,
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(M5PM1AddressableLight),
            cv.Required(CONF_PIN): M5PM1_GPIO_OUTPUT_PIN_SCHEMA,
            cv.Required(CONF_NUM_LED): cv.int_range(min=1, max=32),
        }
    ).extend(BASE_SCHEMA),
    _validate_neopixel_pin,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_parented(var, config[CONF_M5PM1_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_num_led(config[CONF_NUM_LED]))
