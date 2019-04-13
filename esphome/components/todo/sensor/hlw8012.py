from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_CF1_PIN, CONF_CF_PIN, CONF_CHANGE_MODE_EVERY, CONF_CURRENT, \
    CONF_CURRENT_RESISTOR, CONF_ID, CONF_NAME, CONF_POWER, CONF_SEL_PIN, CONF_UPDATE_INTERVAL, \
    CONF_VOLTAGE, CONF_VOLTAGE_DIVIDER


HLW8012Component = sensor.sensor_ns.class_('HLW8012Component', PollingComponent)
HLW8012VoltageSensor = sensor.sensor_ns.class_('HLW8012VoltageSensor', sensor.EmptySensor)
HLW8012CurrentSensor = sensor.sensor_ns.class_('HLW8012CurrentSensor', sensor.EmptySensor)
HLW8012PowerSensor = sensor.sensor_ns.class_('HLW8012PowerSensor', sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = cv.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HLW8012Component),
    cv.Required(CONF_SEL_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_CF_PIN): pins.input_pin,
    cv.Required(CONF_CF1_PIN): pins.input_pin,

    cv.Optional(CONF_VOLTAGE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HLW8012VoltageSensor),
    })),
    cv.Optional(CONF_CURRENT): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HLW8012CurrentSensor),
    })),
    cv.Optional(CONF_POWER): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HLW8012PowerSensor),
    })),

    cv.Optional(CONF_CURRENT_RESISTOR): cv.resistance,
    cv.Optional(CONF_VOLTAGE_DIVIDER): cv.positive_float,
    cv.Optional(CONF_CHANGE_MODE_EVERY): cv.All(cv.uint32_t, cv.Range(min=1)),
    cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_least_one_key(CONF_VOLTAGE, CONF_CURRENT,
                                                               CONF_POWER))


def to_code(config):
    sel = yield gpio_output_pin_expression(config[CONF_SEL_PIN])

    rhs = App.make_hlw8012(sel, config[CONF_CF_PIN], config[CONF_CF1_PIN],
                           config.get(CONF_UPDATE_INTERVAL))
    hlw = Pvariable(config[CONF_ID], rhs)

    if CONF_VOLTAGE in config:
        conf = config[CONF_VOLTAGE]
        sensor.register_sensor(hlw.make_voltage_sensor(conf[CONF_NAME]), conf)
    if CONF_CURRENT in config:
        conf = config[CONF_CURRENT]
        sensor.register_sensor(hlw.make_current_sensor(conf[CONF_NAME]), conf)
    if CONF_POWER in config:
        conf = config[CONF_POWER]
        sensor.register_sensor(hlw.make_power_sensor(conf[CONF_NAME]), conf)
    if CONF_CURRENT_RESISTOR in config:
        cg.add(hlw.set_current_resistor(config[CONF_CURRENT_RESISTOR]))
    if CONF_VOLTAGE_DIVIDER in config:
        cg.add(hlw.set_voltage_divider(config[CONF_VOLTAGE_DIVIDER]))
    if CONF_CHANGE_MODE_EVERY in config:
        cg.add(hlw.set_change_mode_every(config[CONF_CHANGE_MODE_EVERY]))
    register_component(hlw, config)


BUILD_FLAGS = '-DUSE_HLW8012'
