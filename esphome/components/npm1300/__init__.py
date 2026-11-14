import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@tomaszduda23"]
DEPENDENCIES = ["i2c"]
DOMAIN = "npm1300"
MULTI_CONF = True

CONF_NPM1300_ID = "npm1300_id"
CONF_THERMISTOR_RESISTANCE = "thermistor_resistance"
CONF_THERMISTOR_BETA = "thermistor_beta"
CONF_VBUS_CURRENT_LIMIT = "vbus_current_limit"

npm1300_ns = cg.esphome_ns.namespace("npm1300")
NPM1300Component = npm1300_ns.class_(
    "NPM1300Component", cg.PollingComponent, i2c.I2CDevice
)

NPM1300_CHILD_SCHEMA = cv.Schema(
    {cv.GenerateID(CONF_NPM1300_ID): cv.use_id(NPM1300Component)}
)

# ADCNTCRSEL codes
THERMISTOR_RESISTANCES = {10000: 1, 47000: 2, 100000: 3}


def _thermistor_resistance(value: object) -> float:
    value = cv.resistance(value)
    if round(value) not in THERMISTOR_RESISTANCES:
        raise cv.Invalid("Must be one of 10kOhm, 47kOhm, 100kOhm")
    return value


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NPM1300Component),
            cv.Optional(CONF_THERMISTOR_RESISTANCE): _thermistor_resistance,
            cv.Optional(CONF_THERMISTOR_BETA, default=3380): cv.int_range(
                min=1, max=65535
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6B))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    if (resistance := config.get(CONF_THERMISTOR_RESISTANCE)) is not None:
        cg.add(
            var.set_thermistor(
                THERMISTOR_RESISTANCES[round(resistance)], config[CONF_THERMISTOR_BETA]
            )
        )
