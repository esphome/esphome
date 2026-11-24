from esphome import core
import esphome.codegen as cg
from esphome.components import binary_sensor, i2c
import esphome.config_validation as cv
from esphome.const import CONF_DEBOUNCE, DEVICE_CLASS_MOTION
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@kahrendt"]

qwiic_pir_ns = cg.esphome_ns.namespace("qwiic_pir")

DebounceMode = qwiic_pir_ns.enum("DebounceMode", is_class=True)
DEBOUNCE_MODE_OPTIONS = DebounceMode.options(
    "RAW", "NATIVE", "HYBRID", prefix="DEBOUNCE_MODE_"
)

CONF_DEBOUNCE_MODE = "debounce_mode"

QwiicPIRComponent = qwiic_pir_ns.class_(
    "QwiicPIRComponent", cg.Component, i2c.I2CDevice, binary_sensor.BinarySensor
)


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
    cv.only_when(CONF_DEBOUNCE, CONF_DEBOUNCE_MODE, "NATIVE"),
)


async def to_code(config: ConfigType) -> None:
    # Without a debounce time the device is set to 1 ms; NATIVE is the only mode that takes one
    debounce = config.get(CONF_DEBOUNCE)
    var = await binary_sensor.new_binary_sensor(
        config,
        config[CONF_DEBOUNCE_MODE],
        1 if debounce is None else debounce.total_milliseconds,
    )
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
