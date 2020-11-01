import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import sensor, uart
from esphome.const import CONF_CO2, CONF_ID, CONF_TEMPERATURE, ICON_MOLECULE_CO2, \
    UNIT_PARTS_PER_MILLION, UNIT_CELSIUS, ICON_THERMOMETER

DEPENDENCIES = ['uart']

CONF_AUTOMATIC_BASELINE_CALIBRATION = 'automatic_baseline_calibration'

mhz19_ns = cg.esphome_ns.namespace('mhz19')
MHZ19Component = mhz19_ns.class_('MHZ19Component', cg.PollingComponent, uart.UARTDevice)
MHZ19CalibrateZeroAction = mhz19_ns.class_('MHZ19CalibrateZeroAction', automation.Action)
MHZ19ABCEnableAction = mhz19_ns.class_('MHZ19ABCEnableAction', automation.Action)
MHZ19ABCDisableAction = mhz19_ns.class_('MHZ19ABCDisableAction', automation.Action)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MHZ19Component),
    cv.Required(CONF_CO2): sensor.sensor_schema(UNIT_PARTS_PER_MILLION, ICON_MOLECULE_CO2, 0),
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 0),
    cv.Optional(CONF_AUTOMATIC_BASELINE_CALIBRATION): cv.boolean,
}).extend(cv.polling_component_schema('60s')).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    if CONF_CO2 in config:
        sens = yield sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_AUTOMATIC_BASELINE_CALIBRATION in config:
        cg.add(var.set_abc_enabled(config[CONF_AUTOMATIC_BASELINE_CALIBRATION]))


CALIBRATION_ACTION_SCHEMA = maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(MHZ19Component),
})


@automation.register_action('mhz19.calibrate_zero', MHZ19CalibrateZeroAction,
                            CALIBRATION_ACTION_SCHEMA)
@automation.register_action('mhz19.abc_enable', MHZ19ABCEnableAction,
                            CALIBRATION_ACTION_SCHEMA)
@automation.register_action('mhz19.abc_disable', MHZ19ABCDisableAction,
                            CALIBRATION_ACTION_SCHEMA)
def mhz19_calibration_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)
