import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_IDLE_TIME,
    CONF_IIR_FILTER,
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
)

CODEOWNERS = ["@martgras"]
DEPENDENCIES = ["i2c"]

bmp3xx_ns = cg.esphome_ns.namespace("bmp3xx")
Oversampling = bmp3xx_ns.enum("Oversampling")
OVERSAMPLING_OPTIONS = {
    "NONE": Oversampling.OVERSAMPLING_NONE,
    "2X": Oversampling.OVERSAMPLING_X2,
    "4X": Oversampling.OVERSAMPLING_X4,
    "8X": Oversampling.OVERSAMPLING_X8,
    "16X": Oversampling.OVERSAMPLING_X16,
    "32x": Oversampling.OVERSAMPLING_X32,
}

IIRFilter = bmp3xx_ns.enum("IIRFilter")
IIR_FILTER_OPTIONS = {
    "OFF": IIRFilter.IIR_FILTER_OFF,
    "2X": IIRFilter.IIR_FILTER_2,
    "4X": IIRFilter.IIR_FILTER_4,
    "8X": IIRFilter.IIR_FILTER_8,
    "16X": IIRFilter.IIR_FILTER_16,
    "32X": IIRFilter.IIR_FILTER_32,
    "64X": IIRFilter.IIR_FILTER_64,
    "128X": IIRFilter.IIR_FILTER_128,
}

TimeStandby = bmp3xx_ns.enum("TimeStandby")
TIMESTANDBY_OPTIONS = {
    0: TimeStandby.TIME_STANDBY_OFF,  # signal forced mode
    5: TimeStandby.TIME_STANDBY_5MS,
    10: TimeStandby.TIME_STANDBY_10MS,
    20: TimeStandby.TIME_STANDBY_20MS,
    40: TimeStandby.TIME_STANDBY_40MS,
    80: TimeStandby.TIME_STANDBY_80MS,
    160: TimeStandby.TIME_STANDBY_160MS,
    320: TimeStandby.TIME_STANDBY_320MS,
    640: TimeStandby.TIME_STANDBY_640MS,
    1280: TimeStandby.TIME_STANDBY_1280MS,
    2560: TimeStandby.TIME_STANDBY_2560MS,
    5120: TimeStandby.TIME_STANDBY_5120MS,
    10240: TimeStandby.TIME_STANDBY_10240MS,
    20480: TimeStandby.TIME_STANDBY_20480MS,
    40960: TimeStandby.TIME_STANDBY_40960MS,
    81920: TimeStandby.TIME_STANDBY_81920MS,
    163840: TimeStandby.TIME_STANDBY_163840MS,
    327680: TimeStandby.TIME_STANDBY_327680MS,
    655360: TimeStandby.TIME_STANDBY_655360MS,
}

BMP3XXComponent = bmp3xx_ns.class_(
    "BMP3XXComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BMP3XXComponent),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="2X"): cv.enum(
                        OVERSAMPLING_OPTIONS, upper=True
                    ),
                }
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="16X"): cv.enum(
                        OVERSAMPLING_OPTIONS, upper=True
                    ),
                }
            ),
            cv.Optional(CONF_IIR_FILTER, default="OFF"): cv.enum(
                IIR_FILTER_OPTIONS, upper=True
            ),
            cv.Optional(CONF_IDLE_TIME, default=0): cv.enum(TIMESTANDBY_OPTIONS),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x77))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_iir_filter_config(config[CONF_IIR_FILTER]))
    cg.add(var.set_standby_time_config(config[CONF_IDLE_TIME]))
    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_temperature_sensor(sens))
        cg.add(var.set_temperature_oversampling_config(conf[CONF_OVERSAMPLING]))

    if CONF_PRESSURE in config:
        conf = config[CONF_PRESSURE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_pressure_sensor(sens))
        cg.add(var.set_pressure_oversampling_config(conf[CONF_OVERSAMPLING]))
