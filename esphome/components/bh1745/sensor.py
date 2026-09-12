from typing import Any

import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR_TEMPERATURE,
    CONF_GAIN,
    CONF_GLASS_ATTENUATION_FACTOR,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_INTEGRATION_TIME,
    CONF_NAME,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_LIGHTBULB,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_KELVIN,
    UNIT_LUX,
)
from esphome.types import ConfigType

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["i2c"]

CONF_BH1745_ID = "bh1745_id"

UNIT_COUNTS = "#"

CONF_RED_CHANNEL = "red_channel"
CONF_GREEN_CHANNEL = "green_channel"
CONF_BLUE_CHANNEL = "blue_channel"
CONF_CLEAR_CHANNEL = "clear_channel"

bh1745_ns = cg.esphome_ns.namespace("bh1745")

BH1745Component = bh1745_ns.class_(
    "BH1745Component", cg.PollingComponent, i2c.I2CDevice
)

AdcGain = bh1745_ns.enum("AdcGain", is_class=True)
ADC_GAINS = {
    "1X": AdcGain.ADC_GAIN_1X,
    "2X": AdcGain.ADC_GAIN_2X,
    "16X": AdcGain.ADC_GAIN_16X,
}

MeasurementTime = bh1745_ns.enum("MeasurementTime", is_class=True)
MEASUREMENT_TIMES = {
    160: MeasurementTime.MEASUREMENT_TIME_160MS,
    320: MeasurementTime.MEASUREMENT_TIME_320MS,
    640: MeasurementTime.MEASUREMENT_TIME_640MS,
    1280: MeasurementTime.MEASUREMENT_TIME_1280MS,
    2560: MeasurementTime.MEASUREMENT_TIME_2560MS,
    5120: MeasurementTime.MEASUREMENT_TIME_5120MS,
}

color_channel_schema = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS,
        icon=ICON_LIGHTBULB,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)


def validate_measurement_time(value: Any) -> Any:
    value = cv.positive_time_period_milliseconds(value).total_milliseconds
    return cv.enum(MEASUREMENT_TIMES, int=True)(value)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BH1745Component),
            cv.Optional(CONF_GAIN, default="1X"): cv.enum(ADC_GAINS, upper=True),
            cv.Optional(
                CONF_INTEGRATION_TIME, default="160ms"
            ): validate_measurement_time,
            cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=1.0): cv.float_range(
                min=1.0
            ),
            cv.Optional(CONF_RED_CHANNEL): color_channel_schema,
            cv.Optional(CONF_GREEN_CHANNEL): color_channel_schema,
            cv.Optional(CONF_BLUE_CHANNEL): color_channel_schema,
            cv.Optional(CONF_CLEAR_CHANNEL): color_channel_schema,
            cv.Optional(CONF_ILLUMINANCE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_LUX,
                    icon=ICON_LIGHTBULB,
                    accuracy_decimals=1,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_COLOR_TEMPERATURE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_KELVIN,
                    icon=ICON_THERMOMETER,
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x38))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_adc_gain(config[CONF_GAIN]))
    cg.add(var.set_measurement_time(config[CONF_INTEGRATION_TIME]))
    cg.add(var.set_glass_attenuation_factor(config[CONF_GLASS_ATTENUATION_FACTOR]))

    if (conf := config.get(CONF_RED_CHANNEL)) is not None:
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_red_counts_sensor(sens))
    if (conf := config.get(CONF_GREEN_CHANNEL)) is not None:
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_green_counts_sensor(sens))
    if (conf := config.get(CONF_BLUE_CHANNEL)) is not None:
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_blue_counts_sensor(sens))
    if (conf := config.get(CONF_CLEAR_CHANNEL)) is not None:
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_clear_counts_sensor(sens))
    if (conf := config.get(CONF_ILLUMINANCE)) is not None:
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_illuminance_sensor(sens))
    if (conf := config.get(CONF_COLOR_TEMPERATURE)) is not None:
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_color_temperature_sensor(sens))
