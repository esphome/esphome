import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID, CONF_INVERTED, CONF_ICON, ICON_POWER, CONF_PIPSOLAR_ID
from . import PipsolarComponent, pipsolar_ns

DEPENDENCIES = ['uart']

CONF_OUTPUT_SOURCE_PRIORITY_UTILITY = 'output_source_priority_utility'
CONF_OUTPUT_SOURCE_PRIORITY_SOLAR = 'output_source_priority_solar'
CONF_OUTPUT_SOURCE_PRIORITY_BATTERY = 'output_source_priority_battery'
CONF_INPUT_VOLTAGE_RANGE = 'input_voltage_range'
CONF_PV_OK_CONDITION_FOR_PARALLEL = 'pv_ok_condition_for_parallel'
CONF_PV_POWER_BALANCE = 'pv_power_balance'

PipsolarSwitch = pipsolar_ns.class_('PipsolarSwitch', switch.Switch, cg.Component)
PIPSWITCH_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(PipsolarSwitch),

    cv.Optional(CONF_INVERTED): cv.invalid("Pipsolar switches do not support inverted mode!"),

    cv.Optional(CONF_ICON, default=ICON_POWER): switch.icon
}).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_PIPSOLAR_ID): cv.use_id(PipsolarComponent),
    cv.Optional(CONF_OUTPUT_SOURCE_PRIORITY_UTILITY): PIPSWITCH_SCHEMA,
    cv.Optional(CONF_OUTPUT_SOURCE_PRIORITY_SOLAR): PIPSWITCH_SCHEMA,
    cv.Optional(CONF_OUTPUT_SOURCE_PRIORITY_BATTERY): PIPSWITCH_SCHEMA,
    cv.Optional(CONF_INPUT_VOLTAGE_RANGE): PIPSWITCH_SCHEMA,
    cv.Optional(CONF_PV_OK_CONDITION_FOR_PARALLEL): PIPSWITCH_SCHEMA,
    cv.Optional(CONF_PV_POWER_BALANCE): PIPSWITCH_SCHEMA,
})


def to_code(config):
  paren = yield cg.get_variable(config[CONF_PIPSOLAR_ID])
  if CONF_OUTPUT_SOURCE_PRIORITY_UTILITY in config:
    conf = config[CONF_OUTPUT_SOURCE_PRIORITY_UTILITY]
    var = cg.new_Pvariable(conf[CONF_ID])
    yield cg.register_component(var, conf)
    yield switch.register_switch(var, conf)
    cg.add(paren.set_output_source_priority_utility_switch(var))
    cg.add(var.set_parent(paren))
    cg.add(var.set_on_command("POP00"))
  if CONF_OUTPUT_SOURCE_PRIORITY_SOLAR in config:
    conf = config[CONF_OUTPUT_SOURCE_PRIORITY_SOLAR]
    var = cg.new_Pvariable(conf[CONF_ID])
    yield cg.register_component(var, conf)
    yield switch.register_switch(var, conf)
    cg.add(paren.set_output_source_priority_solar_switch(var))
    cg.add(var.set_parent(paren))
    cg.add(var.set_on_command("POP01"))
  if CONF_OUTPUT_SOURCE_PRIORITY_BATTERY in config:
    conf = config[CONF_OUTPUT_SOURCE_PRIORITY_BATTERY]
    var = cg.new_Pvariable(conf[CONF_ID])
    yield cg.register_component(var, conf)
    yield switch.register_switch(var, conf)
    cg.add(paren.set_output_source_priority_battery_switch(var))
    cg.add(var.set_parent(paren))
    cg.add(var.set_on_command("POP02"))
  if CONF_INPUT_VOLTAGE_RANGE in config:
    conf = config[CONF_INPUT_VOLTAGE_RANGE]
    var = cg.new_Pvariable(conf[CONF_ID])
    yield cg.register_component(var, conf)
    yield switch.register_switch(var, conf)
    cg.add(paren.set_input_voltage_range_switch(var))
    cg.add(var.set_parent(paren))
    cg.add(var.set_on_command("PGR01"))
    cg.add(var.set_off_command("PGR00"))
  if CONF_PV_OK_CONDITION_FOR_PARALLEL in config:
    conf = config[CONF_PV_OK_CONDITION_FOR_PARALLEL]
    var = cg.new_Pvariable(conf[CONF_ID])
    yield cg.register_component(var, conf)
    yield switch.register_switch(var, conf)
    cg.add(paren.set_pv_ok_condition_for_parallel_switch(var))
    cg.add(var.set_parent(paren))
    cg.add(var.set_on_command("PPVOKC1"))
    cg.add(var.set_off_command("PPVOKC0"))
  if CONF_PV_POWER_BALANCE in config:
    conf = config[CONF_PV_POWER_BALANCE]
    var = cg.new_Pvariable(conf[CONF_ID])
    yield cg.register_component(var, conf)
    yield switch.register_switch(var, conf)
    cg.add(paren.set_pv_power_balance_switch(var))
    cg.add(var.set_parent(paren))
    cg.add(var.set_on_command("PSPB1"))
    cg.add(var.set_off_command("PSPB0"))
