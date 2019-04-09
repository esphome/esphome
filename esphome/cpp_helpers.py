from esphome.const import CONF_INVERTED, CONF_MODE, CONF_NUMBER, CONF_PCF8574, \
    CONF_SETUP_PRIORITY, CONF_MCP23017
from esphome.core import CORE, EsphomeError, coroutine
from esphome.cpp_generator import IntLiteral, RawExpression
from esphome.cpp_types import GPIOInputPin, GPIOOutputPin


@coroutine
def generic_gpio_pin_expression_(conf, mock_obj, default_mode):
    if conf is None:
        return
    number = conf[CONF_NUMBER]
    inverted = conf.get(CONF_INVERTED)
    if CONF_PCF8574 in conf:
        from esphome.components import pcf8574

        hub = yield CORE.get_variable(conf[CONF_PCF8574])

        if default_mode == u'INPUT':
            mode = pcf8574.PCF8675_GPIO_MODES[conf.get(CONF_MODE, u'INPUT')]
            yield hub.make_input_pin(number, mode, inverted)
            return
        if default_mode == u'OUTPUT':
            yield hub.make_output_pin(number, inverted)
            return

        raise EsphomeError(u"Unknown default mode {}".format(default_mode))
    if CONF_MCP23017 in conf:
        from esphome.components import mcp23017

        hub = yield CORE.get_variable(conf[CONF_MCP23017])

        if default_mode == u'INPUT':
            mode = mcp23017.MCP23017_GPIO_MODES[conf.get(CONF_MODE, u'INPUT')]
            yield hub.make_input_pin(number, mode, inverted)
            return
        if default_mode == u'OUTPUT':
            yield hub.make_output_pin(number, inverted)
            return

        raise EsphomeError(u"Unknown default mode {}".format(default_mode))
    if len(conf) == 1:
        yield IntLiteral(number)
        return
    mode = RawExpression(conf.get(CONF_MODE, default_mode))
    yield mock_obj(number, mode, inverted)


@coroutine
def gpio_output_pin_expression(conf):
    yield generic_gpio_pin_expression_(conf, GPIOOutputPin, 'OUTPUT')


@coroutine
def gpio_input_pin_expression(conf):
    yield generic_gpio_pin_expression_(conf, GPIOInputPin, 'INPUT')


def setup_component(obj, config):
    if CONF_SETUP_PRIORITY in config:
        CORE.add(obj.set_setup_priority(config[CONF_SETUP_PRIORITY]))
