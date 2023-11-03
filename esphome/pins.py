import operator
import esphome.config_validation as cv
from esphome.core import ID
from functools import reduce

from esphome.const import (
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
    CONF_IGNORE_STRAPPING_WARNING,
    CONF_OTHER_USES,
    CONF_ANALOG,
    CONF_INVERTED,
    CONF_ID,
    CONF_ESPHOME,
)
from esphome.core import CORE
from esphome.core.config import CONF_CHECK_PIN_USE


def _final_validate(parent_id_key, fun):
    def validator(fconf, pin_config):
        parent_path = fconf.get_path_for_id(pin_config[parent_id_key])[:-1]
        parent_config = fconf.get_config_for_path(parent_path)

        pin_path = fconf.get_path_for_id(pin_config[CONF_ID])[:-1]
        with cv.prepend_path([cv.ROOT_CONFIG_PATH] + pin_path):
            fun(pin_config, parent_config)

    return validator


class PinRegistry(dict):
    def __init__(self):
        super().__init__()
        self.pins_used = {}

    def increment_count(self, component, config):
        if (
            CORE.raw_config[CONF_ESPHOME].get(CONF_CHECK_PIN_USE) is False
            or CONF_NUMBER not in config
            or config.get(CONF_OTHER_USES) is True
        ):
            return
        key = config[component] if component in config else component
        if isinstance(key, ID):
            key = key.id
        pin_key = (key, config[CONF_NUMBER])
        if pin_key not in self.pins_used:
            self.pins_used[pin_key] = 0
        self.pins_used[pin_key] += 1
        if self.pins_used[pin_key] > 1:
            raise cv.Invalid(
                f"{key} pin {config[CONF_NUMBER]} is used in multiple places"
            )

    def register(self, name, schema, final_validate=None):
        if final_validate is not None:
            final_validate = _final_validate(name, final_validate)

        def decorator(fun):
            self[name] = (fun, schema, final_validate)
            return fun

        return decorator


PIN_SCHEMA_REGISTRY = PinRegistry()


def _set_mode(value, default_mode):
    if CONF_MODE not in value:
        return {**value, CONF_MODE: default_mode}
    mode = value[CONF_MODE]
    if not isinstance(mode, str):
        return value
    # mode is a string, try parsing it like arduino pin modes
    PIN_MODES = {
        "INPUT": {
            CONF_INPUT: True,
        },
        "OUTPUT": {
            CONF_OUTPUT: True,
        },
        "INPUT_PULLUP": {
            CONF_INPUT: True,
            CONF_PULLUP: True,
        },
        "OUTPUT_OPEN_DRAIN": {
            CONF_OUTPUT: True,
            CONF_OPEN_DRAIN: True,
        },
        "INPUT_PULLDOWN_16": {
            CONF_INPUT: True,
            CONF_PULLDOWN: True,
        },
        "INPUT_PULLDOWN": {
            CONF_INPUT: True,
            CONF_PULLDOWN: True,
        },
        "INPUT_OUTPUT_OPEN_DRAIN": {
            CONF_INPUT: True,
            CONF_OUTPUT: True,
            CONF_OPEN_DRAIN: True,
        },
    }
    if mode.upper() not in PIN_MODES:
        raise cv.Invalid(f"Unknown pin mode {mode}", [CONF_MODE])
    return {**value, CONF_MODE: PIN_MODES[mode.upper()]}


def _schema_creator(default_mode, internal: bool = False):
    def validator(value):
        if not isinstance(value, dict):
            return validator({CONF_NUMBER: value})
        value = _set_mode(value, default_mode)
        keys = list(filter(lambda k: k in value, PIN_SCHEMA_REGISTRY))
        key = CORE.target_platform if internal or not keys else keys[0]
        value = PIN_SCHEMA_REGISTRY[key][1](value)
        PIN_SCHEMA_REGISTRY.increment_count(key, value)
        return value

    return validator


def _internal_number_creator(mode):
    def validator(value):
        value_d = {CONF_NUMBER: value}
        value_d = _set_mode(value_d, mode)
        return PIN_SCHEMA_REGISTRY[CORE.target_platform][1](value_d)[CONF_NUMBER]

    return validator


def gpio_flags_expr(mode):
    """Convert the given mode dict to a gpio Flags expression"""
    import esphome.codegen as cg

    FLAGS_MAPPING = {
        CONF_INPUT: cg.gpio_Flags.FLAG_INPUT,
        CONF_OUTPUT: cg.gpio_Flags.FLAG_OUTPUT,
        CONF_OPEN_DRAIN: cg.gpio_Flags.FLAG_OPEN_DRAIN,
        CONF_PULLUP: cg.gpio_Flags.FLAG_PULLUP,
        CONF_PULLDOWN: cg.gpio_Flags.FLAG_PULLDOWN,
    }
    active_flags = [v for k, v in FLAGS_MAPPING.items() if mode.get(k)]
    if not active_flags:
        return cg.gpio_Flags.FLAG_NONE

    return reduce(operator.or_, active_flags)


gpio_pin_schema = _schema_creator
internal_gpio_pin_number = _internal_number_creator
gpio_output_pin_schema = _schema_creator(
    {
        CONF_OUTPUT: True,
    }
)
gpio_input_pin_schema = _schema_creator(
    {
        CONF_INPUT: True,
    }
)
gpio_input_pullup_pin_schema = _schema_creator(
    {
        CONF_INPUT: True,
        CONF_PULLUP: True,
    }
)
internal_gpio_output_pin_schema = _schema_creator(
    {
        CONF_OUTPUT: True,
    },
    internal=True,
)
internal_gpio_output_pin_number = _internal_number_creator({CONF_OUTPUT: True})
internal_gpio_input_pin_schema = _schema_creator(
    {
        CONF_INPUT: True,
    },
    internal=True,
)
internal_gpio_input_pin_number = _internal_number_creator({CONF_INPUT: True})
internal_gpio_input_pullup_pin_schema = _schema_creator(
    {
        CONF_INPUT: True,
        CONF_PULLUP: True,
    },
    internal=True,
)
internal_gpio_input_pullup_pin_number = _internal_number_creator(
    {
        CONF_INPUT: True,
        CONF_PULLUP: True,
    }
)


def check_strapping_pin(conf, strapping_pin_list, logger):
    import esphome.config_validation as cv

    num = conf[CONF_NUMBER]
    if num in strapping_pin_list and not conf.get(CONF_IGNORE_STRAPPING_WARNING):
        logger.warning(
            f"GPIO{num} is a strapping PIN and should only be used for I/O with care.\n"
            "Attaching external pullup/down resistors to strapping pins can cause unexpected failures.\n"
            "See https://esphome.io/guides/faq.html#why-am-i-getting-a-warning-about-strapping-pins",
        )
    # mitigate undisciplined use of strapping:
    if num not in strapping_pin_list and conf.get(CONF_IGNORE_STRAPPING_WARNING):
        raise cv.Invalid(f"GPIO{num} is not a strapping pin")


GPIO_STANDARD_MODES = (
    CONF_ANALOG,
    CONF_INPUT,
    CONF_OUTPUT,
    CONF_OPEN_DRAIN,
    CONF_PULLUP,
    CONF_PULLDOWN,
)


def gpio_base_schema(pin_type, validator, modes=GPIO_STANDARD_MODES, invertable=True):
    """
    Generate a base gpio pin schema
    :param pin_type: The type for the pin variable
    :param validator: A validator for the pin number
    :param modes: The available modes, default is all standard modes
    :param invertable: If the pin supports hardware inversion
    :return: A schema for the pin
    """
    mode_dict = dict(map(lambda m: (cv.Optional(m, default=False), cv.boolean), modes))
    schema = cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(pin_type),
            cv.Required(CONF_NUMBER): validator,
            cv.Optional(CONF_OTHER_USES): cv.boolean,
            cv.Optional(CONF_MODE, default={}): cv.Schema(mode_dict),
        }
    )
    if invertable:
        return schema.extend({cv.Optional(CONF_INVERTED, default=False): cv.boolean})
    return schema
