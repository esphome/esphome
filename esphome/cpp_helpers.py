from esphome.const import CONF_INVERTED, CONF_MODE, CONF_NUMBER, CONF_SETUP_PRIORITY, \
    CONF_UPDATE_INTERVAL, CONF_TYPE_ID
from esphome.core import coroutine, ID, CORE
from esphome.cpp_generator import RawExpression, add, get_variable
from esphome.cpp_types import App, GPIOPin
from esphome.py_compat import text_type


@coroutine
def gpio_pin_expression(conf):
    """Generate an expression for the given pin option.

    This is a coroutine, you must await it with a 'yield' expression!
    """
    if conf is None:
        return
    from esphome import pins
    for key, (func, _) in pins.PIN_SCHEMA_REGISTRY.items():
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
    id_ = text_type(var.base)
    if id_ not in CORE.component_ids:
        raise ValueError(u"Component ID {} was not declared to inherit from Component, "
                         u"or was registered twice. Please create a bug report with your "
                         u"configuration.".format(id_))
    CORE.component_ids.remove(id_)
    if CONF_SETUP_PRIORITY in config:
        add(var.set_setup_priority(config[CONF_SETUP_PRIORITY]))
    if CONF_UPDATE_INTERVAL in config:
        add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    add(App.register_component(var))
    yield var


@coroutine
def register_parented(var, value):
    if isinstance(value, ID):
        paren = yield get_variable(value)
    else:
        paren = value
    add(var.set_parent(paren))


def extract_registry_entry_config(registry, full_config):
    # type: (Registry, ConfigType) -> RegistryEntry
    key, config = next((k, v) for k, v in full_config.items() if k in registry)
    return registry[key], config


@coroutine
def build_registry_entry(registry, full_config):
    registry_entry, config = extract_registry_entry_config(registry, full_config)
    type_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    yield builder(config, type_id)


@coroutine
def build_registry_list(registry, config):
    actions = []
    for conf in config:
        action = yield build_registry_entry(registry, conf)
        actions.append(action)
    yield actions
