import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import sensor
from esphome.components import spi
from esphome.automation import maybe_simple_id
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
    UNIT_EMPTY,
    UNIT_DECIBEL_MILLIWATT,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["sensor"]

CODEOWNERS = ["@gabest11"]

CONF_GDO0 = "gdo0"
CONF_BANDWIDTH = "bandwidth"
# CONF_FREQUENCY = "frequency"
CONF_RSSI = "rssi"
CONF_LQI = "lqi"

cc1101_ns = cg.esphome_ns.namespace("cc1101")
CC1101 = cc1101_ns.class_("CC1101", cg.PollingComponent, spi.SPIDevice)

BeginTxAction = cc1101_ns.class_("BeginTxAction", automation.Action)
EndTxAction = cc1101_ns.class_("EndTxAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CC1101),
            cv.Optional(CONF_GDO0): pins.gpio_output_pin_schema,
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
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)

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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if CONF_GDO0 in config:
        gdo0 = await cg.gpio_pin_expression(config[CONF_GDO0])
        cg.add(var.set_config_gdo0(gdo0))
    cg.add(var.set_config_bandwidth(config[CONF_BANDWIDTH]))
    cg.add(var.set_config_frequency(config[CONF_FREQUENCY]))
    if CONF_RSSI in config:
        rssi = await sensor.new_sensor(config[CONF_RSSI])
        cg.add(var.set_config_rssi_sensor(rssi))
    if CONF_LQI in config:
        lqi = await sensor.new_sensor(config[CONF_LQI])
        cg.add(var.set_config_lqi_sensor(lqi))
