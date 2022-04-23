import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
)

CONF_PRESSURE_RANGE = "pressure_range"
CONF_CONTINUES_MODE = "continues_mode"
CONF_SLEEP_TIME = "sleep_time"

DEPENDENCIES = ["i2c"]

xgzp6897d_ns = cg.esphome_ns.namespace("xgzp6897d")
XGZP6897DOversampling = xgzp6897d_ns.enum("XGZP6897DOversampling")
OVERSAMPLING_OPTIONS = {
    "256X": XGZP6897DOversampling.XGZP6897D_OVERSAMPLING_256X,
    "512X": XGZP6897DOversampling.XGZP6897D_OVERSAMPLING_512X,
    "1024X": XGZP6897DOversampling.XGZP6897D_OVERSAMPLING_1024X,
    "2048X": XGZP6897DOversampling.XGZP6897D_OVERSAMPLING_2048X,
    "4096X": XGZP6897DOversampling.XGZP6897D_OVERSAMPLING_4096X,
    "8192X": XGZP6897DOversampling.XGZP6897D_OVERSAMPLING_8192X,
    "16384X": XGZP6897DOversampling.XGZP6897D_OVERSAMPLING_16384X,
    "32768X": XGZP6897DOversampling.XGZP6897D_OVERSAMPLING_32768X,
}

XGZP6897DComponent = xgzp6897d_ns.class_(
    "XGZP6897DComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XGZP6897DComponent),
            cv.Required(CONF_PRESSURE_RANGE): cv.float_range(min=0.0, max=260.0),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_OVERSAMPLING, default="1024X"): cv.enum(
                OVERSAMPLING_OPTIONS, upper=True
            ),
            cv.Optional(CONF_CONTINUES_MODE, default=False): cv.boolean,
            cv.Optional(CONF_SLEEP_TIME, default=0): cv.int_range(min=0, max=15),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6D))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    conf = config[CONF_PRESSURE_RANGE]
    if conf > 131:
        cg.add(var.set_kvalue(32))
    elif conf > 65:
        cg.add(var.set_kvalue(64))
    elif conf > 32:
        cg.add(var.set_kvalue(128))
    elif conf > 16:
        cg.add(var.set_kvalue(256))
    elif conf > 8:
        cg.add(var.set_kvalue(512))
    elif conf > 4:
        cg.add(var.set_kvalue(1024))
    elif conf >= 2:
        cg.add(var.set_kvalue(2048))
    elif conf >= 1:
        cg.add(var.set_kvalue(4096))
    else:
        cg.add(var.set_kvalue(8192))

    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_temperature_sensor(sens))

    if CONF_PRESSURE in config:
        conf = config[CONF_PRESSURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_pressure_sensor(sens))

    cg.add(var.set_oversampling(config[CONF_OVERSAMPLING]))
    cg.add(var.set_continuous_mode(config[CONF_CONTINUES_MODE]))
    cg.add(var.set_sleep_time(config[CONF_SLEEP_TIME]))
