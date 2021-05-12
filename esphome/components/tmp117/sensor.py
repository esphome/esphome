import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_TEMPERATURE,
    ICON_EMPTY,
    UNIT_CELSIUS,
)

DEPENDENCIES = ["i2c"]

tmp117_ns = cg.esphome_ns.namespace("tmp117")
TMP117Component = tmp117_ns.class_(
    "TMP117Component", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(UNIT_CELSIUS, ICON_EMPTY, 1, DEVICE_CLASS_TEMPERATURE)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(TMP117Component),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x48))
)


def determine_config_register(polling_period):
    if polling_period >= 16.0:
        # 64 averaged conversions, max conversion time
        # 0000 00 111 11 00000
        # 0000 0011 1110 0000
        return 0x03E0
    if polling_period >= 8.0:
        # 64 averaged conversions, high conversion time
        # 0000 00 110 11 00000
        # 0000 0011 0110 0000
        return 0x0360
    if polling_period >= 4.0:
        # 64 averaged conversions, mid conversion time
        # 0000 00 101 11 00000
        # 0000 0010 1110 0000
        return 0x02E0
    if polling_period >= 1.0:
        # 64 averaged conversions, min conversion time
        # 0000 00 000 11 00000
        # 0000 0000 0110 0000
        return 0x0060
    if polling_period >= 0.5:
        # 32 averaged conversions, min conversion time
        # 0000 00 000 10 00000
        # 0000 0000 0100 0000
        return 0x0040
    if polling_period >= 0.25:
        # 8 averaged conversions, mid conversion time
        # 0000 00 010 01 00000
        # 0000 0001 0010 0000
        return 0x0120
    if polling_period >= 0.125:
        # 8 averaged conversions, min conversion time
        # 0000 00 000 01 00000
        # 0000 0000 0010 0000
        return 0x0020
    # 1 averaged conversions, min conversion time
    # 0000 00 000 00 00000
    # 0000 0000 0000 0000
    return 0x0000


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    yield sensor.register_sensor(var, config)

    update_period = config[CONF_UPDATE_INTERVAL].total_seconds
    cg.add(var.set_config(determine_config_register(update_period)))
