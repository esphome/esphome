import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import (
  climate,
  uart,
  sensor,
  binary_sensor,
  number,
  switch,
  select
)
from esphome.const import ( 
  CONF_TEMPERATURE,
  CONF_ID,
  STATE_CLASS_MEASUREMENT,
  DEVICE_CLASS_FREQUENCY,
  DEVICE_CLASS_TEMPERATURE,
  DEVICE_CLASS_PROBLEM,
  DEVICE_CLASS_HEAT,
  UNIT_CELSIUS,
  UNIT_HERTZ,
  UNIT_EMPTY,
  ICON_THERMOMETER,
)

CODEOWNERS = ["@Danny-Dunn"]
DEPENDENCIES = ['uart', 'climate']
AUTO_LOAD = ['sensor', 'binary_sensor', 'switch', 'number', 'select']

ICON_SINE = "mdi:sine-wave"
ICON_FAN = "mdi:fan"
ICON_ALERT = "mdi:alert"
ICON_PREHEAT = "mdi:heating_coil"

CONF_COMPRESSOR_FREQUENCY = "compressor_frequency"
CONF_FAN_VELOCITY = "fan_velocity"
CONF_CONFLICTED = "conflicted"
CONF_PREHEAT = "preheat"
CONF_VERTICAL_AIRFLOW = "vertical_airflow"
CONF_INJECT_ENABLE_SWITCH = "inject_enable"
CONF_REMOTE_TEMP_NUMBER = "remote_temperature"
CONF_TEMPERATURE_OFFSET_NUMBER = "temperature_offset"
CONF_CONTROL_TEMP = "control_temperature"

climate_mistubishi_ns = cg.esphome_ns.namespace('climate_mitsubishi')
ClimateMitsubishi = climate_mistubishi_ns.class_(
  'ClimateMitsubishi', climate.Climate, uart.UARTDevice, cg.Component
)

ClimateMitsubishiInjectEnableSwitch = climate_mistubishi_ns.class_(
  'ClimateMitsubishiInjectEnableSwitch', switch.Switch, cg.Component
)
ClimateMitsubishiRemoteTemperatureNumber = climate_mistubishi_ns.class_(
  'ClimateMitsubishiRemoteTemperatureNumber', number.Number, cg.Component
)
ClimateMitsubishiTemperatureOffsetNumber = climate_mistubishi_ns.class_(
  'ClimateMitsubishiTemperatureOffsetNumber', number.Number, cg.Component
)
ClimateMitsubishiVerticalAirflowSelect = climate_mistubishi_ns.class_(
  'ClimateMitsubishiVerticalAirflowSelect', select.Select, cg.Component
)

AirflowVerticalDirection = climate_mistubishi_ns.enum("AirflowVerticalDirection", True)

AIRFLOW_VERTICAL_DIRECTION_OPTIONS = {
  "Auto": AirflowVerticalDirection.AUTO,
  "1": AirflowVerticalDirection.DOWN_1,
  "2": AirflowVerticalDirection.DOWN_2,
  "3": AirflowVerticalDirection.DOWN_3,
  "4": AirflowVerticalDirection.DOWN_4,
}

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend({
  cv.GenerateID(): cv.declare_id(ClimateMitsubishi),
  cv.Optional(CONF_COMPRESSOR_FREQUENCY): sensor.sensor_schema(
    unit_of_measurement=UNIT_HERTZ,
    icon=ICON_SINE,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_FREQUENCY,
    state_class=STATE_CLASS_MEASUREMENT
  ),
  cv.Optional(CONF_FAN_VELOCITY): sensor.sensor_schema(
    unit_of_measurement=UNIT_EMPTY,
    icon=ICON_FAN,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_FREQUENCY,
    state_class=STATE_CLASS_MEASUREMENT
  ),
  cv.Optional(CONF_CONFLICTED): binary_sensor.binary_sensor_schema(
    icon=ICON_ALERT,
    device_class=DEVICE_CLASS_PROBLEM,
  ),
  cv.Optional(CONF_CONTROL_TEMP): sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    icon=ICON_THERMOMETER,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
  ),
  cv.Optional(CONF_PREHEAT): binary_sensor.binary_sensor_schema(),
  cv.Optional(CONF_REMOTE_TEMP_NUMBER):cv.Schema({
    cv.GenerateID(CONF_ID): cv.declare_id(ClimateMitsubishiRemoteTemperatureNumber)
  }).extend(cv.COMPONENT_SCHEMA).extend(number.NUMBER_SCHEMA),
  cv.Optional(CONF_TEMPERATURE_OFFSET_NUMBER):cv.Schema({
    cv.GenerateID(CONF_ID): cv.declare_id(ClimateMitsubishiTemperatureOffsetNumber)
  }).extend(cv.COMPONENT_SCHEMA).extend(number.NUMBER_SCHEMA),
  cv.Optional(CONF_INJECT_ENABLE_SWITCH): cv.Schema({
    cv.GenerateID(CONF_ID): cv.declare_id(ClimateMitsubishiInjectEnableSwitch),
  }).extend(cv.COMPONENT_SCHEMA).extend(switch.SWITCH_SCHEMA),
  cv.Optional(CONF_VERTICAL_AIRFLOW): select.select_schema().extend({
    cv.GenerateID(CONF_ID): cv.declare_id(ClimateMitsubishiVerticalAirflowSelect),
  }).extend(cv.COMPONENT_SCHEMA),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


InjectTemperatureAction = climate_mistubishi_ns.class_("InjectTemperatureAction", automation.Action)

# Set vertical airflow direction action
@automation.register_action(
  "climate.mitsubishi.inject_temperature",
  InjectTemperatureAction,
  cv.Schema(
    {
      cv.GenerateID(): cv.use_id(ClimateMitsubishi),
      cv.Required(CONF_TEMPERATURE): cv.templatable(
          cv.float_
      ),
    }
  ),
)
def mitsubishi_climate_inject_temperature_to_code(config, action_id, template_arg, args):
  paren = yield cg.get_variable(config[CONF_ID])
  var = cg.new_Pvariable(action_id, template_arg, paren)
  template_ = yield cg.templatable(
    config[CONF_TEMPERATURE], args, cv.float_
  )
  cg.add(var.set_direction(template_))
  return var

def to_code(config):
  var = cg.new_Pvariable(config[CONF_ID])
  yield cg.register_component(var, config)
  yield uart.register_uart_device(var, config)
  yield climate.register_climate(var, config)

  if CONF_COMPRESSOR_FREQUENCY in config:
    compressor_sens = yield sensor.new_sensor(config[CONF_COMPRESSOR_FREQUENCY])
    cg.add(var.set_compressor_frequency_sensor(compressor_sens))

  if CONF_FAN_VELOCITY in config:
    fan_velocity_sens = yield sensor.new_sensor(config[CONF_FAN_VELOCITY])
    cg.add(var.set_fan_velocity_sensor(fan_velocity_sens))

  if CONF_CONFLICTED in config:
    conflicted_sens = yield binary_sensor.new_binary_sensor(config[CONF_CONFLICTED])
    cg.add(var.set_conflicted_sensor(conflicted_sens))

  if CONF_PREHEAT in config:
    preheat_sens = yield binary_sensor.new_binary_sensor(config[CONF_PREHEAT])
    cg.add(var.set_preheat_sensor(preheat_sens))

  if CONF_CONTROL_TEMP in config:
    control_temp_sens = yield sensor.new_sensor(config[CONF_CONTROL_TEMP])
    cg.add(var.set_control_temperature_sensor(control_temp_sens))

  if CONF_INJECT_ENABLE_SWITCH in config:
    print(config[CONF_INJECT_ENABLE_SWITCH])
    inject_enable_switch = yield switch.new_switch(config[CONF_INJECT_ENABLE_SWITCH])
    cg.add(inject_enable_switch.set_climate(var))

  if CONF_REMOTE_TEMP_NUMBER in config:
    control_temp_number = yield number.new_number(config[CONF_REMOTE_TEMP_NUMBER],
                                                  min_value=10,
                                                  max_value=30,
                                                  step=0.1)
    cg.add(control_temp_number.set_climate(var))
    cg.add(var.set_remote_temperature_number(control_temp_number))
  if CONF_TEMPERATURE_OFFSET_NUMBER in config:
    temp_offset_number = yield number.new_number(config[CONF_TEMPERATURE_OFFSET_NUMBER],
                                                  min_value=-5,
                                                  max_value=5,
                                                  step=0.1)
    cg.add(temp_offset_number.set_climate(var))
    #cg.add(var.set_temperature_offset_number(temp_offset_number))
  if CONF_VERTICAL_AIRFLOW in config:
    print(config[CONF_VERTICAL_AIRFLOW])
    vertical_airflow_select = yield select.new_select(config[CONF_VERTICAL_AIRFLOW], options=["Auto","1","2","3","4","5","Swing"])
    cg.add(vertical_airflow_select.set_climate(var))
    cg.add(var.set_vertical_airflow_select(vertical_airflow_select))