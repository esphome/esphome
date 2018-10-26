import voluptuous as vol

from esphomeyaml import config_validation as cv
from esphomeyaml.components.uart import UARTComponent
from esphomeyaml.const import CONF_DATA, CONF_HIGH, CONF_ID, CONF_LOW, CONF_SYNC, CONF_UART_ID
from esphomeyaml.helpers import App, Pvariable, esphomelib_ns, get_variable

CONF_SONOFF_RF_BRIDGE_ID = 'sonoff_rf_bridge_id'
SonoffRFBridge = esphomelib_ns.SonoffRFBridge
SonoffRFBinarySensor = esphomelib_ns.SonoffRFBinarySensor
SonoffRFSwitch = esphomelib_ns.SonoffRFSwitch

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(SonoffRFBridge),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
})


def validate_data(value):
    value = cv.string_strict(value)
    if len(value) != 32:
        raise vol.Invalid("Data must be 32 characters long!")
    for x in value:
        if x not in ('0', '1'):
            raise vol.Invalid(u"Each character in data must either be 0 or 1, not {}".format(x))
    return value


FRAME_SCHEMA = vol.Schema({
    vol.Required(CONF_SYNC): cv.uint16_t,
    vol.Required(CONF_LOW): cv.uint16_t,
    vol.Required(CONF_HIGH): cv.uint16_t,
    vol.Required(CONF_DATA): validate_data,
})


def get_args(config):
    d = 0
    for v in config[CONF_DATA]:
        d <<= 1
        d |= v == '1'
    return [config[CONF_SYNC], config[CONF_LOW], config[CONF_HIGH], d]


def to_code(config):
    for uart in get_variable(config[CONF_UART_ID]):
        yield
    rhs = App.make_sonoff_rf_bridge(uart)
    Pvariable(config[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_SONOFF_RF_BRIDGE'
