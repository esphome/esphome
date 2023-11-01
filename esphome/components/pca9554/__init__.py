import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_NUMBER,
    CONF_MODE,
    CONF_INVERTED,
    CONF_OUTPUT,
)
from esphome.core import CORE, ID

CODEOWNERS = ["@hwstar", "@clydebarrow"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
CONF_PIN_COUNT = "pin_count"
pca9554_ns = cg.esphome_ns.namespace("pca9554")

PCA9554Component = pca9554_ns.class_("PCA9554Component", cg.Component, i2c.I2CDevice)
PCA9554GPIOPin = pca9554_ns.class_(
    "PCA9554GPIOPin", cg.GPIOPin, cg.Parented.template(PCA9554Component)
)

CONF_PCA9554 = "pca9554"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(PCA9554Component),
            cv.Optional(CONF_PIN_COUNT, default=8): cv.one_of(4, 8, 16),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        i2c.i2c_device_schema(0x20)
    )  # Note: 0x20 for the non-A part. The PCA9554A parts start at addess 0x38
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_pin_count(config[CONF_PIN_COUNT]))
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


def validate_pin(config):
    pca_id = config[CONF_PCA9554].id

    # when pin config validation is performed, the entire YAML has been read, but depending on the component order,
    # the pca9554 component may not yet have been processed, so its id property could be either the original string,
    # or an ID object.
    def matcher(p):
        id = p[CONF_ID]
        if isinstance(id, ID):
            return id.id == pca_id
        return id == pca_id

    pca = list(filter(matcher, CORE.raw_config[CONF_PCA9554]))
    if not pca:
        raise cv.Invalid(f"No pca9554 component found with id matching {pca_id}")
    count = pca[0][CONF_PIN_COUNT]
    if config[CONF_NUMBER] >= count:
        raise cv.Invalid(f"Pin number must be in range 0-{count - 1}")
    return config


PCA9554_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(PCA9554GPIOPin),
        cv.Required(CONF_PCA9554): cv.use_id(PCA9554Component),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=15),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
            },
            validate_mode,
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    },
    validate_pin,
)


@pins.PIN_SCHEMA_REGISTRY.register("pca9554", PCA9554_PIN_SCHEMA)
async def pca9554_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_PCA9554])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
