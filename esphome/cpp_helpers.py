from esphome.const import CONF_INVERTED, CONF_MODE, CONF_NUMBER, CONF_SETUP_PRIORITY, \
    CONF_UPDATE_INTERVAL
from esphome.core import coroutine
from esphome.cpp_generator import RawExpression, add
from esphome.cpp_types import App, GPIOPin


@coroutine
def gpio_pin_expression(conf):
    """Generate an expression for the given pin option.

    This is a coroutine, you must await it with a 'yield' expression!
    """
    if conf is None:
        return
    from esphome import pins
    for key, (_, func) in pins.PIN_SCHEMA_REGISTRY.items():
        if key in conf:
            yield coroutine(func)(conf)
            return

    number = conf[CONF_NUMBER]
    mode = conf[CONF_MODE]
    inverted = conf.get(CONF_INVERTED)
    yield GPIOPin.new(number, RawExpression(mode), inverted)


@coroutine
def register_component(var, config):
    """Register the given obj as a component.

    This is a coroutine, you must await it with a 'yield' expression!

    :param var: The variable representing the component.
    :param config: The configuration for the component.
    """
    if CONF_SETUP_PRIORITY in config:
        add(var.set_setup_priority(config[CONF_SETUP_PRIORITY]))
    if CONF_UPDATE_INTERVAL in config:
        add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    add(App.register_component(var))


@coroutine
def build_registry_entry(registry, full_config):
    key, config = next((k, v) for k, v in full_config.items() if k in registry)
    builder = coroutine(registry[key][1])
    yield builder(config)


@coroutine
def build_registry_list(registry, config):
    actions = []
    for conf in config:
        action = yield build_registry_entry(registry, conf)
        actions.append(action)
    yield actions
