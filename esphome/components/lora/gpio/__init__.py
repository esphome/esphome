import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_OUTPUT, CONF_NUMBER, CONF_INVERTED, CONF_MODE
from .. import CONF_LORA, Lora, lora_ns

LoraGPIOPin = lora_ns.class_("LoraGPIOPin", cg.GPIOPin)


def validate_mode(value):
    if not (value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be output")
    return value


Lora_PIN_SCHEMA = pins.gpio_base_schema(
    LoraGPIOPin,
    cv.int_range(min=0, max=17),
    modes=[CONF_OUTPUT],
    mode_validator=validate_mode,
    invertable=True,
).extend(
    {
        cv.Required(CONF_LORA): cv.use_id(Lora),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_LORA, Lora_PIN_SCHEMA)
async def lora_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_LORA])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
