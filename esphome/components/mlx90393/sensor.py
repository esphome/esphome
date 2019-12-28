import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (CONF_ADDRESS, CONF_ID, CONF_GAIN, CONF_RANGE,
                           CONF_OVERSAMPLING, ICON_MAGNET, UNIT_MICROTESLA,
                           ICON_THERMOMETER, UNIT_CELSIUS, CONF_TEMPERATURE)

DEPENDENCIES = ['i2c']

mlx90393_ns = cg.esphome_ns.namespace('mlx90393')

CONF_FIELD_STRENGTH_X = 'field_strength_x'
CONF_FIELD_STRENGTH_Y = 'field_strength_y'
CONF_FIELD_STRENGTH_Z = 'field_strength_z'

MLX90393Component = mlx90393_ns.class_('MLX90393Component', cg.PollingComponent, i2c.I2CDevice)

MLX90393Gain = mlx90393_ns.enum('MLX90393Gain')
GAIN_OPTIONS = {
    "1X": MLX90393Gain.MLX9039_GAIN_1,
    "1.25X": MLX90393Gain.MLX9039_GAIN_1P25,
    "1.67X": MLX90393Gain.MLX9039_GAIN_1P67,
    "2X": MLX90393Gain.MLX9039_GAIN_2,
    "2.5X": MLX90393Gain.MLX9039_GAIN_2P5,
    "3X": MLX90393Gain.MLX9039_GAIN_3,
    "3.75X": MLX90393Gain.MLX9039_GAIN_3P75,
    "5X": MLX90393Gain.MLX9039_GAIN_5,
}

MLX90393Oversampling = mlx90393_ns.enum('MLX90393Oversampling')
OVERSAMPLING_OPTIONS = {
    "NONE": MLX90393Oversampling.MLX9039_OVERSAMPLING_NONE,
    "2X": MLX90393Oversampling.MLX9039_OVERSAMPLING_2X,
    "4X": MLX90393Oversampling.MLX9039_OVERSAMPLING_4X,
    "8X": MLX90393Oversampling.MLX9039_OVERSAMPLING_8X,
}

MLX90393Range = mlx90393_ns.enum('MLX90393Range')
RANGE_OPTIONS = {
    "16bit": MLX90393Range.MLX9039_RANGE_16BIT,
    "17bit": MLX90393Range.MLX9039_RANGE_17BIT,
    "18bit": MLX90393Range.MLX9039_RANGE_18BIT,
    "19bit": MLX90393Range.MLX9039_RANGE_19BIT,
}


field_strength_schema = sensor.sensor_schema(UNIT_MICROTESLA, ICON_MAGNET, 0)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MLX90393Component),
    cv.Optional(CONF_ADDRESS): cv.i2c_address,
    cv.Optional(CONF_GAIN, default='1X'): cv.enum(GAIN_OPTIONS, upper=True),
    cv.Optional(CONF_RANGE, default='18bit'): cv.enum(RANGE_OPTIONS, lower=True),
    cv.Optional(CONF_OVERSAMPLING, default='8X'): cv.enum(OVERSAMPLING_OPTIONS, upper=True),
    cv.Optional(CONF_FIELD_STRENGTH_X): field_strength_schema,
    cv.Optional(CONF_FIELD_STRENGTH_Y): field_strength_schema,
    cv.Optional(CONF_FIELD_STRENGTH_Z): field_strength_schema,
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x0C))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_range(config[CONF_RANGE]))
    cg.add(var.set_oversampling(config[CONF_OVERSAMPLING]))
    if CONF_FIELD_STRENGTH_X in config:
        sens = yield sensor.new_sensor(config[CONF_FIELD_STRENGTH_X])
        cg.add(var.set_x_sensor(sens))
    if CONF_FIELD_STRENGTH_Y in config:
        sens = yield sensor.new_sensor(config[CONF_FIELD_STRENGTH_Y])
        cg.add(var.set_y_sensor(sens))
    if CONF_FIELD_STRENGTH_Z in config:
        sens = yield sensor.new_sensor(config[CONF_FIELD_STRENGTH_Z])
        cg.add(var.set_z_sensor(sens))
    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
