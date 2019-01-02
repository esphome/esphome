from esphomeyaml.const import CONF_INVERTED, CONF_MODE, CONF_NUMBER, CONF_PCF8574, \
    CONF_SETUP_PRIORITY
from esphomeyaml.core import CORE, EsphomeyamlError
from esphomeyaml.cpp_generator import IntLiteral, RawExpression
from esphomeyaml.cpp_types import GPIOInputPin, GPIOOutputPin


def generic_gpio_pin_expression_(conf, mock_obj, default_mode):
    if conf is None:
        return
    number = conf[CONF_NUMBER]
    inverted = conf.get(CONF_INVERTED)
    if CONF_PCF8574 in conf:
        from esphomeyaml.components import pcf8574

        for hub in CORE.get_variable(conf[CONF_PCF8574]):
            yield None

        if default_mode == u'INPUT':
            mode = pcf8574.PCF8675_GPIO_MODES[conf.get(CONF_MODE, u'INPUT')]
            yield hub.make_input_pin(number, mode, inverted)
            return
        if default_mode == u'OUTPUT':
            yield hub.make_output_pin(number, inverted)
            return

        raise EsphomeyamlError(u"Unknown default mode {}".format(default_mode))
    if len(conf) == 1:
        yield IntLiteral(number)
        return
    mode = RawExpression(conf.get(CONF_MODE, default_mode))
    yield mock_obj(number, mode, inverted)


def gpio_output_pin_expression(conf):
    for exp in generic_gpio_pin_expression_(conf, GPIOOutputPin, 'OUTPUT'):
        yield None
    yield exp


def gpio_input_pin_expression(conf):
    for exp in generic_gpio_pin_expression_(conf, GPIOInputPin, 'INPUT'):
        yield None
    yield exp


def setup_component(obj, config):
    if CONF_SETUP_PRIORITY in config:
        CORE.add(obj.set_setup_priority(config[CONF_SETUP_PRIORITY]))
