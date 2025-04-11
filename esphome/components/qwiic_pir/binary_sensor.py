from esphome import core
import esphome.codegen as cg
from esphome.components import binary_sensor, i2c
import esphome.config_validation as cv
from esphome.const import CONF_DEBOUNCE, DEVICE_CLASS_MOTION

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@kahrendt"]

qwiic_pir_ns = cg.esphome_ns.namespace("qwiic_pir")

DebounceMode = qwiic_pir_ns.enum("DebounceMode")
DEBOUNCE_MODE_OPTIONS = {
    "RAW": DebounceMode.RAW_DEBOUNCE_MODE,
    "NATIVE": DebounceMode.NATIVE_DEBOUNCE_MODE,
    "HYBRID": DebounceMode.HYBRID_DEBOUNCE_MODE,
}

CONF_DEBOUNCE_MODE = "debounce_mode"

QwiicPIRComponent = qwiic_pir_ns.class_(
    "QwiicPIRComponent", cg.Component, i2c.I2CDevice, binary_sensor.BinarySensor
)


def validate_no_debounce_unless_native(config):
    if CONF_DEBOUNCE in config:
        if config[CONF_DEBOUNCE_MODE] != "NATIVE":
            raise cv.Invalid("debounce can only be set if debounce_mode is NATIVE")
    return config


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(
        QwiicPIRComponent,
        device_class=DEVICE_CLASS_MOTION,
    )
    .extend(
        {
            cv.Optional(CONF_DEBOUNCE): cv.All(
                cv.time_period,
                cv.Range(max=core.TimePeriod(milliseconds=65535)),
            ),
            cv.Optional(CONF_DEBOUNCE_MODE, default="HYBRID"): cv.enum(
                DEBOUNCE_MODE_OPTIONS, upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x12)),
    validate_no_debounce_unless_native,
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if debounce_time_setting := config.get(CONF_DEBOUNCE):
        cg.add(var.set_debounce_time(debounce_time_setting.total_milliseconds))
    else:
        cg.add(var.set_debounce_time(1))  # default to 1 ms if not configured
    cg.add(var.set_debounce_mode(config[CONF_DEBOUNCE_MODE]))
