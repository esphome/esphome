import esphome.codegen as cg
from esphome.components import output, sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_OUTPUT,
    CONF_SENSOR,
    DEVICE_CLASS_PM25,
    ICON_CHEMICAL_WEAPON,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)

DEPENDENCIES = ["output"]
AUTO_LOAD = ["voltage_sampler"]
CODEOWNERS = ["@zry98"]

CONF_ADC_VOLTAGE_OFFSET = "adc_voltage_offset"
CONF_ADC_VOLTAGE_MULTIPLIER = "adc_voltage_multiplier"

gp2y1010au0f_ns = cg.esphome_ns.namespace("gp2y1010au0f")
GP2Y1010AU0FSensor = gp2y1010au0f_ns.class_(
    "GP2Y1010AU0FSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        GP2Y1010AU0FSensor,
        unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_PM25,
        state_class=STATE_CLASS_MEASUREMENT,
        icon=ICON_CHEMICAL_WEAPON,
    )
    .extend(
        {
            cv.Required(CONF_SENSOR): cv.use_id(voltage_sampler.VoltageSampler),
            cv.Optional(CONF_ADC_VOLTAGE_OFFSET, default=0.0): cv.float_,
            cv.Optional(CONF_ADC_VOLTAGE_MULTIPLIER, default=1.0): cv.float_,
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    # the ADC sensor to read voltage from
    adc_sensor = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_adc_source(adc_sensor))
    cg.add(
        var.set_voltage_refs(
            config[CONF_ADC_VOLTAGE_OFFSET], config[CONF_ADC_VOLTAGE_MULTIPLIER]
        )
    )

    # the binary output to control the module's internal IR LED
    led_output = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_led_output(led_output))
