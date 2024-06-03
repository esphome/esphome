import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.automation import maybe_simple_id
from esphome.components import sensor
from esphome.components import spi
from esphome.components import remote_base
from esphome.components import voltage_sampler
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
    CONF_PROTOCOL,
    CONF_CODE,
    CONF_TEMPERATURE,
    UNIT_EMPTY,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_CELSIUS,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["sensor", "remote_base", "voltage_sampler"]
MULTI_CONF = True

CODEOWNERS = ["@gabest11"]

CONF_GDO0_PIN = "gdo0_pin"
CONF_GDO0_ADC_ID = "gdo0_adc_id"
CONF_BANDWIDTH = "bandwidth"
# CONF_FREQUENCY = "frequency"
CONF_RSSI = "rssi"
CONF_LQI = "lqi"
CONF_CC1101_ID = "cc1101_id"

ns = cg.esphome_ns.namespace("cc1101")

CC1101 = ns.class_("CC1101", cg.PollingComponent, spi.SPIDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CC1101),
            cv.Optional(CONF_GDO0_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_GDO0_ADC_ID): cv.use_id(voltage_sampler.VoltageSampler),
            cv.Optional(CONF_BANDWIDTH, default=200): cv.uint32_t,
            cv.Optional(CONF_FREQUENCY, default=433920): cv.uint32_t,
            cv.Optional(CONF_RSSI): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_LQI): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if CONF_GDO0_PIN in config:
        gdo0_pin = await cg.gpio_pin_expression(config[CONF_GDO0_PIN])
        cg.add(var.set_config_gdo0_pin(gdo0_pin))
    if CONF_GDO0_ADC_ID in config:
        gdo0_adc_id = await cg.get_variable(config[CONF_GDO0_ADC_ID])
        cg.add(var.set_config_gdo0_adc_pin(gdo0_adc_id))
    cg.add(var.set_config_bandwidth(config[CONF_BANDWIDTH]))
    cg.add(var.set_config_frequency(config[CONF_FREQUENCY]))
    if CONF_RSSI in config:
        rssi = await sensor.new_sensor(config[CONF_RSSI])
        cg.add(var.set_config_rssi_sensor(rssi))
    if CONF_LQI in config:
        lqi = await sensor.new_sensor(config[CONF_LQI])
        cg.add(var.set_config_lqi_sensor(lqi))
    if CONF_TEMPERATURE in config:
        temperature = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_config_temperature_sensor(temperature))


BeginTxAction = ns.class_("BeginTxAction", automation.Action)
EndTxAction = ns.class_("EndTxAction", automation.Action)

CC1101_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(CC1101),
    }
)


@automation.register_action("cc1101.begin_tx", BeginTxAction, CC1101_ACTION_SCHEMA)
@automation.register_action("cc1101.end_tx", EndTxAction, CC1101_ACTION_SCHEMA)
async def cc1101_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


CC1101RawAction = ns.class_("CC1101RawAction", remote_base.RCSwitchRawAction)

CC1101_TRANSMIT_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101),
        }
    )
    .extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)
    .extend(remote_base.RC_SWITCH_RAW_SCHEMA)
    .extend(remote_base.RC_SWITCH_TRANSMITTER)
)


@remote_base.register_action(
    "rc_switch_raw_cc1101", CC1101RawAction, CC1101_TRANSMIT_SCHEMA
)
async def rc_switch_raw_cc1101_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL],
        args,
        remote_base.RCSwitchBase,
        to_exp=remote_base.build_rc_switch_protocol,
    )
    cg.add(var.set_protocol(proto))
    cg.add(var.set_code(await cg.templatable(config[CONF_CODE], args, cg.std_string)))
    await cg.register_parented(var, config[CONF_CC1101_ID])
