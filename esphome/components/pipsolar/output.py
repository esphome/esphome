import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_CHANNEL, CONF_ID
from . import PipsolarComponent, pipsolar_ns

DEPENDENCIES = ["pipsolar"]

PipsolarOutput = pipsolar_ns.class_("PipsolarOutput", output.FloatOutput)
CONF_PIPSOLAR_ID = 'pipsolar_id'
CONF_POSSIBLE_VALUES = 'possible_values'

BATTERY_RECHARGE_VOLTAGE_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(PipsolarOutput),
    cv.Optional(CONF_POSSIBLE_VALUES,default=[44.0,45.0,46.0,47.0,48.0,49.0,50.0,51.0]): cv.All(
            cv.ensure_list(
                cv.positive_float,
            ),
            cv.Length(min=1),
        ),
})
BATTERY_UNDER_VOLTAGE_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(PipsolarOutput),
    # 40.0 -> 48.0
    cv.Optional(CONF_POSSIBLE_VALUES,default=[40.0,40.1,42,43,44,45,46,47,48.0]): cv.All(
            cv.ensure_list(
                cv.positive_float,
            ),
            cv.Length(min=1),
        ),
})
BATTERY_FLOAT_VOLTAGE_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(PipsolarOutput),
    # 48.0 -> 58.4
    cv.Optional(CONF_POSSIBLE_VALUES,default=[48.0,49.0,50.0,51.0]): cv.All(
            cv.ensure_list(
                cv.positive_float,
            ),
            cv.Length(min=1),
        ),
})
BATTERY_TYPE_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(PipsolarOutput),
    cv.Optional(CONF_POSSIBLE_VALUES,default=[0,1,2]): cv.All(
            cv.ensure_list(
                cv.positive_float,
            ),
            cv.Length(min=1),
        ),
})
CURRENT_MAX_AC_CHARGING_CURRENT_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(PipsolarOutput),
    cv.Optional(CONF_POSSIBLE_VALUES,default=[2,10,20]): cv.All(
            cv.ensure_list(
                cv.positive_float,
            ),
            cv.Length(min=1),
        ),
})
CURRENT_MAX_CHARGING_CURRENT_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(PipsolarOutput),
    cv.Optional(CONF_POSSIBLE_VALUES,default=[10,20,30,40]): cv.All(
            cv.ensure_list(
                cv.positive_float,
            ),
            cv.Length(min=1),
        ),
})
OUTPUT_SOURCE_PRIORITY_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(PipsolarOutput),
    cv.Optional(CONF_POSSIBLE_VALUES,default=[0,1,2]): cv.All(
            cv.ensure_list(
                cv.positive_float,
            ),
            cv.Length(min=1),
        ),
})
CHARGER_SOURCE_PRIORITY_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(PipsolarOutput),
    cv.Optional(CONF_POSSIBLE_VALUES,default=[0,1,2,3]): cv.All(
            cv.ensure_list(
                cv.positive_float,
            ),
            cv.Length(min=1),
        ),
})
BATTERY_REDISCHARGE_VOLTAGE_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(PipsolarOutput),
    cv.Optional(CONF_POSSIBLE_VALUES,default=[00.0,48.0,49,50.0,51.0,52,53,54,55,56,57,58]): cv.All(
            cv.ensure_list(
                cv.positive_float,
            ),
            cv.Length(min=1),
        ),
})



# 3.11 PCVV<nn.n><cr>: Setting battery C.V. (constant voltage) charging voltage 48.0V ~ 58.4V for 48V unit
# battery_bulk_voltage;
# battery_recharge_voltage;     12V unit: 11V/11.3V/11.5V/11.8V/12V/12.3V/12.5V/12.8V
#                               24V unit: 22V/22.5V/23V/23.5V/24V/24.5V/25V/25.5V
#                               48V unit: 44V/45V/46V/47V/48V/49V/50V/51V
# battery_under_voltage;        40.0V ~ 48.0V for 48V unit 
# battery_float_voltage;        48.0V ~ 58.4V for 48V unit 
# battery_type;  00 for AGM, 01 for Flooded battery
# current_max_ac_charging_current;
# output_source_priority; 00 / 01 / 02 
# charger_source_priority;  For HS: 00 for utility first, 01 for solar first, 02 for solar and utility, 03 for only solar charging
#                           For MS/MSX: 00 for utility first, 01 for solar first, 03 for only solar charging 
# battery_redischarge_voltage;  12V unit: 00.0V12V/12.3V/12.5V/12.8V/13V/13.3V/13.5V/13.8V/14V/14.3V/14.5
#                               24V unit: 00.0V/24V/24.5V/25V/25.5V/26V/26.5V/27V/27.5V/28V/28.5V/29V
#                               48V unit: 00.0V48V/49V/50V/51V/52V/53V/54V/55V/56V/57V/58V 

CONF_BATTERY_RECHARGE_VOLTAGE = 'battery_recharge_voltage';
CONF_BATTERY_UNDER_VOLTAGE = 'battery_under_voltage';
CONF_BATTERY_FLOAT_VOLTAGE = 'battery_float_voltage';
CONF_BATTERY_TYPE = 'battery_type';
CONF_CURRENT_MAX_AC_CHARGING_CURRENT = 'current_max_ac_charging_current';
CONF_CURRENT_MAX_CHARGING_CURRENT = 'current_max_charging_current';
CONF_OUTPUT_SOURCE_PRIORITY = 'output_source_priority';
CONF_CHARGER_SOURCE_PRIORITY = 'charger_source_priority';
CONF_BATTERY_REDISCHARGE_VOLTAGE = 'battery_redischarge_voltage';

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_PIPSOLAR_ID): cv.use_id(PipsolarComponent),
        cv.Optional(CONF_BATTERY_RECHARGE_VOLTAGE): BATTERY_RECHARGE_VOLTAGE_SCHEMA,
        cv.Optional(CONF_BATTERY_UNDER_VOLTAGE): BATTERY_UNDER_VOLTAGE_SCHEMA,
        cv.Optional(CONF_BATTERY_FLOAT_VOLTAGE): BATTERY_FLOAT_VOLTAGE_SCHEMA,
        cv.Optional(CONF_BATTERY_TYPE): BATTERY_TYPE_SCHEMA,
        cv.Optional(CONF_CURRENT_MAX_AC_CHARGING_CURRENT): CURRENT_MAX_AC_CHARGING_CURRENT_SCHEMA,
        cv.Optional(CONF_CURRENT_MAX_CHARGING_CURRENT): CURRENT_MAX_CHARGING_CURRENT_SCHEMA,
        cv.Optional(CONF_OUTPUT_SOURCE_PRIORITY): OUTPUT_SOURCE_PRIORITY_SCHEMA,
        cv.Optional(CONF_CHARGER_SOURCE_PRIORITY): CHARGER_SOURCE_PRIORITY_SCHEMA,
        cv.Optional(CONF_BATTERY_REDISCHARGE_VOLTAGE): BATTERY_REDISCHARGE_VOLTAGE_SCHEMA,
    }
)

def to_code(config):
    paren = yield cg.get_variable(config[CONF_PIPSOLAR_ID])
    if CONF_BATTERY_RECHARGE_VOLTAGE in config:
      conf = config[CONF_BATTERY_RECHARGE_VOLTAGE]
      var = cg.new_Pvariable(conf[CONF_ID])
      yield output.register_output(var, conf)
      cg.add(var.set_parent(paren))
      cg.add(var.set_set_command("PBCV%02.1f"))
      if (CONF_POSSIBLE_VALUES) in conf:
        cg.add(var.set_possible_values(conf[CONF_POSSIBLE_VALUES]))
    if CONF_BATTERY_UNDER_VOLTAGE in config:
      conf = config[CONF_BATTERY_UNDER_VOLTAGE]
      var = cg.new_Pvariable(conf[CONF_ID])
      yield output.register_output(var, conf)
      cg.add(var.set_parent(paren))
      cg.add(var.set_set_command("PSDV%02.1f"))
      if (CONF_POSSIBLE_VALUES) in conf:
        cg.add(var.set_possible_values(conf[CONF_POSSIBLE_VALUES]))
    if CONF_BATTERY_FLOAT_VOLTAGE in config:
      conf = config[CONF_BATTERY_FLOAT_VOLTAGE]
      var = cg.new_Pvariable(conf[CONF_ID])
      yield output.register_output(var, conf)
      cg.add(var.set_parent(paren))
      cg.add(var.set_set_command("PBFT%02.1f"))
      if (CONF_POSSIBLE_VALUES) in conf:
        cg.add(var.set_possible_values(conf[CONF_POSSIBLE_VALUES]))
    if CONF_BATTERY_TYPE in config:
      conf = config[CONF_BATTERY_TYPE]
      var = cg.new_Pvariable(conf[CONF_ID])
      yield output.register_output(var, conf)
      cg.add(var.set_parent(paren))
      cg.add(var.set_set_command("PBT%02.0f"))
      if (CONF_POSSIBLE_VALUES) in conf:
        cg.add(var.set_possible_values(conf[CONF_POSSIBLE_VALUES]))
    if CONF_CURRENT_MAX_AC_CHARGING_CURRENT in config:
      conf = config[CONF_CURRENT_MAX_AC_CHARGING_CURRENT]
      var = cg.new_Pvariable(conf[CONF_ID])
      yield output.register_output(var, conf)
      cg.add(var.set_parent(paren))
      cg.add(var.set_set_command("MUCHGC0%02.0f"))
      if (CONF_POSSIBLE_VALUES) in conf:
        cg.add(var.set_possible_values(conf[CONF_POSSIBLE_VALUES]))
    if CONF_CURRENT_MAX_CHARGING_CURRENT in config:
      conf = config[CONF_CURRENT_MAX_CHARGING_CURRENT]
      var = cg.new_Pvariable(conf[CONF_ID])
      yield output.register_output(var, conf)
      cg.add(var.set_parent(paren))
      cg.add(var.set_set_command("MCHGC0%02.0f"))
      if (CONF_POSSIBLE_VALUES) in conf:
        cg.add(var.set_possible_values(conf[CONF_POSSIBLE_VALUES]))
    if CONF_OUTPUT_SOURCE_PRIORITY in config:
      conf = config[CONF_OUTPUT_SOURCE_PRIORITY]
      var = cg.new_Pvariable(conf[CONF_ID])
      yield output.register_output(var, conf)
      cg.add(var.set_parent(paren))
      cg.add(var.set_set_command("POP%02.0f"))
      if (CONF_POSSIBLE_VALUES) in conf:
        cg.add(var.set_possible_values(conf[CONF_POSSIBLE_VALUES]))
    if CONF_CHARGER_SOURCE_PRIORITY in config:
      conf = config[CONF_CHARGER_SOURCE_PRIORITY]
      var = cg.new_Pvariable(conf[CONF_ID])
      yield output.register_output(var, conf)
      cg.add(var.set_parent(paren))
      cg.add(var.set_set_command("PCP%02.0f"))
      if (CONF_POSSIBLE_VALUES) in conf:
        cg.add(var.set_possible_values(conf[CONF_POSSIBLE_VALUES]))
    if CONF_BATTERY_REDISCHARGE_VOLTAGE in config:
      conf = config[CONF_BATTERY_REDISCHARGE_VOLTAGE]
      var = cg.new_Pvariable(conf[CONF_ID])
      yield output.register_output(var, conf)
      cg.add(var.set_parent(paren))
      cg.add(var.set_set_command("PBDV%02.1f"))
      if (CONF_POSSIBLE_VALUES) in conf:
        cg.add(var.set_possible_values(conf[CONF_POSSIBLE_VALUES]))
