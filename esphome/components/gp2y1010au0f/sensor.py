import esphome.codegen as cg
from esphome.components import output, sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_OUTPUT,
    ICON_CHEMICAL_WEAPON,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)

DEPENDENCIES = ["voltage_sampler"]

gp2y1010au0f_ns = cg.esphome_ns.namespace("gp2y1010au0f")
GP2Y1010AU0FSensor = gp2y1010au0f_ns.class_(
    "GP2Y1010AU0FSensor", sensor.Sensor, cg.PollingComponent
)

CONF_ADC_VOLTAGE_MULTIPLIER = "adc_voltage_multiplier"
CONF_ADC_VOLTAGE_OFFSET = "adc_voltage_offset"
CONF_SAMPLE_DURATION = "sample_duration"
CONF_SAMPLE_WAIT_BEFORE = "sample_wait_before"
CONF_SAMPLE_WAIT_AFTER = "sample_wait_after"
CONF_SAMPLE_WAIT_OFF = "sample_wait_off"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        GP2Y1010AU0FSensor,
        unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
        icon=ICON_CHEMICAL_WEAPON,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(GP2Y1010AU0FSensor),
            cv.Required(voltage_sampler.CONF_VOLTAGE_SAMPLER): cv.use_id(
                voltage_sampler.VoltageSampler
            ),
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
            cv.Optional(CONF_ADC_VOLTAGE_MULTIPLIER, default=1.0): cv.float_,
            cv.Optional(CONF_ADC_VOLTAGE_OFFSET, default=0.0): cv.float_,
            cv.Optional(
                CONF_SAMPLE_DURATION, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SAMPLE_WAIT_BEFORE, default=280): cv.uint32_t,
            cv.Optional(CONF_SAMPLE_WAIT_AFTER, default=40): cv.uint32_t,
            cv.Optional(CONF_SAMPLE_WAIT_OFF, default=9680): cv.uint32_t,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    sens = await cg.get_variable(config[voltage_sampler.CONF_VOLTAGE_SAMPLER])
    cg.add(var.set_adc_source(sens))

    output_ = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_led_output(output_))

    cg.add(var.set_voltage_multiplier(config[CONF_ADC_VOLTAGE_MULTIPLIER]))
    cg.add(var.set_voltage_offset(config[CONF_ADC_VOLTAGE_OFFSET]))
    cg.add(var.set_sample_duration(config[CONF_SAMPLE_DURATION]))
    cg.add(var.set_sample_wait_before(config[CONF_SAMPLE_WAIT_BEFORE]))
    cg.add(var.set_sample_wait_after(config[CONF_SAMPLE_WAIT_AFTER]))
    cg.add(var.set_sample_wait_off(config[CONF_SAMPLE_WAIT_OFF]))
