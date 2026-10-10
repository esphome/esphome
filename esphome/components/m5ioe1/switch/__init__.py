import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_PIN, DEVICE_CLASS_SWITCH, ICON_PULSE

from .. import BASE_SCHEMA, CONF_M5IOE1_ID, m5ioe1_ns

DEPENDENCIES = ["m5ioe1"]

CONF_AW8737A = "aw8737a"

CONF_PULSE_NUM = "pulse_num"

AW8737ASwitch = m5ioe1_ns.class_("AW8737ASwitch", cg.Component, switch.Switch)

CONFIG_SCHEMA = (
    switch.switch_schema(
        AW8737ASwitch,
        device_class=DEVICE_CLASS_SWITCH,
        icon=ICON_PULSE,
        default_restore_mode="RESTORE_DEFAULT_OFF",
    )
    .extend(
        {
            cv.Required(CONF_PIN): cv.int_range(min=0, max=13),
            cv.Required(CONF_PULSE_NUM): cv.int_range(min=0, max=3),
        }
    )
    .extend(BASE_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)

    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5IOE1_ID])

    cg.add(var.set_pin(config[CONF_PIN]))
    cg.add(var.set_pulse_num(config[CONF_PULSE_NUM]))
