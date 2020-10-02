import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import sensor, uart
from esphome.const import CONF_CO2, CONF_ID, CONF_HUMIDITY, ICON_MOLECULE_CO2, \
    UNIT_PARTS_PER_MILLION, UNIT_PERCENT, ICON_WATER_PERCENT

DEPENDENCIES = ['uart']

CONF_AUTOMATIC_BASELINE_CALIBRATION = 'automatic_baseline_calibration'

mhz14a_ns = cg.esphome_ns.namespace('mhz14a')
MHZ14AComponent = mhz14a_ns.class_('MHZ14AComponent', cg.PollingComponent, uart.UARTDevice)
MHZ14ACalibrateZeroAction = mhz14a_ns.class_('MHZ14ACalibrateZeroAction', automation.Action)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MHZ14AComponent),
    cv.Required(CONF_CO2): sensor.sensor_schema(UNIT_PARTS_PER_MILLION, ICON_MOLECULE_CO2, 0),
    cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 0),
}).extend(cv.polling_component_schema('60s')).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    if CONF_CO2 in config:
        sens = yield sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))


CALIBRATION_ACTION_SCHEMA = maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(MHZ14AComponent),
})


@automation.register_action('mhz14a.calibrate_zero', MHZ14ACalibrateZeroAction,
                            CALIBRATION_ACTION_SCHEMA)
def mhz14a_calibration_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)
