import operator
from functools import reduce
import esphome.config_validation as cv
from esphome.core import CORE, ID

from esphome.const import (
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
    CONF_IGNORE_STRAPPING_WARNING,
    CONF_ALLOW_OTHER_USES,
    CONF_INVERTED,
)


class PinRegistry(dict):
    def __init__(self):
        super().__init__()
        self.pins_used = {}

    def reset(self):
        self.pins_used = {}

    def get_count(self, key, number):
        """
        Get the number of places a given pin is used.
        :param key: The ID of the defining component
        :param number: The pin number
        :return: The number of places the pin is used.
        """
        pin_key = (key, number)
        return self.pins_used[pin_key] if pin_key in self.pins_used else 0

    def register(self, name, schema, final_validate=None):
        """
        Register a pin schema
        :param name:
        :param schema:
        :param final_validate:
        :return:
        """

        def decorator(fun):
            self[name] = (fun, schema, final_validate)
            return fun

        return decorator

    def validate(self, conf, key=None):
        """
        Validate a pin against a registered schema
        :param conf The pin config
        :param key: an optional scalar key (e.g. platform)
        :return: The transformed result
        """
        from esphome.config import path_context

        key = self.get_key(conf) if key is None else key
        # Element 1 is the pin validation function
        # evaluate here so a validation failure skips the rest
        result = self[key][1](conf)
        if CONF_NUMBER in result:
            # key maps to the pin schema
            if isinstance(key, ID):
                key = key.id
            pin_key = (key, result[CONF_NUMBER])
            if pin_key not in self.pins_used:
                self.pins_used[pin_key] = []
            # client_id identifies the instance of the providing component
            client_id = result.get(key)
            self.pins_used[pin_key].append((path_context.get(), client_id, result))
        # return the validated pin config
        return result

    def get_key(self, conf):
        """
        Is there a key in conf corresponding to a registered pin schema?
        If not, fall back to the default platform schema.
        :param conf The config for the component
        :return: the schema key
        """
        keys = list(filter(lambda k: k in conf, self))
        return keys[0] if keys else CORE.target_platform

    def get_to_code(self, key):
        """
        Return the code generator function for a pin schema, stored as tuple element 0
        :param conf: The pin config
        :param key An optional specific key
        :return: The awaitable coroutine
        """
        key = self.get_key(key) if isinstance(key, dict) else key
        return self[key][0]

    def final_validate(self, fconf):
        """
        Run the final validation for all pins, and check for reuse
        :param fconf: The full config
        """
        for (key, _), pin_list in self.pins_used.items():
            count = len(pin_list)  # number of places same pin used.
            final_val_fun = self[key][2]  # final validation function
            for pin_path, client_id, pin_config in pin_list:
                with fconf.catch_error([cv.ROOT_CONFIG_PATH] + pin_path):
                    if final_val_fun is not None:
                        # Get the containing path of the config providing this pin.
                        parent_path = fconf.get_path_for_id(client_id)[:-1]
                        parent_config = fconf.get_config_for_path(parent_path)
                        final_val_fun(pin_config, parent_config)
                    allow_others = pin_config.get(CONF_ALLOW_OTHER_USES, False)
                    if count != 1 and not allow_others:
                        raise cv.Invalid(
                            f"Pin {pin_config[CONF_NUMBER]} is used in multiple places"
                        )
                    if count == 1 and allow_others:
                        raise cv.Invalid(
                            f"Pin {pin_config[CONF_NUMBER]} incorrectly sets {CONF_ALLOW_OTHER_USES}: true"
                        )


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
        if internal:
            return PIN_SCHEMA_REGISTRY.validate(value, CORE.target_platform)
        return PIN_SCHEMA_REGISTRY.validate(value)

    return validator


def _internal_number_creator(mode):
    def validator(value):
        if isinstance(value, dict):
            if CONF_MODE in value or CONF_INVERTED in value:
                raise cv.Invalid(
                    "This variable only supports pin numbers, not full pin schemas "
                    "(with inverted and mode)."
                )
            value_d = value
        else:
            value_d = {CONF_NUMBER: value}
        value_d = _set_mode(value_d, mode)
        return PIN_SCHEMA_REGISTRY.validate(value_d, CORE.target_platform)[CONF_NUMBER]

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
    CONF_INPUT,
    CONF_OUTPUT,
    CONF_OPEN_DRAIN,
    CONF_PULLUP,
    CONF_PULLDOWN,
)


def gpio_validate_modes(value):
    if not value[CONF_INPUT] and not value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be input or output")
    return value


def gpio_base_schema(
    pin_type,
    number_validator,
    modes=GPIO_STANDARD_MODES,
    mode_validator=gpio_validate_modes,
    invertable=True,
):
    """
    Generate a base gpio pin schema
    :param pin_type: The type for the pin variable
    :param number_validator: A validator for the pin number
    :param modes: The available modes, default is all standard modes
    :param mode_validator: A validator function for the pin mode
    :param invertable: If the pin supports hardware inversion
    :return: A schema for the pin
    """
    mode_default = len(modes) == 1
    mode_dict = dict(
        map(lambda m: (cv.Optional(m, default=mode_default), cv.boolean), modes)
    )

    schema = cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(pin_type),
            cv.Required(CONF_NUMBER): number_validator,
            cv.Optional(CONF_ALLOW_OTHER_USES): cv.boolean,
            cv.Optional(CONF_MODE, default={}): cv.All(mode_dict, mode_validator),
        }
    )
    if invertable:
        return schema.extend({cv.Optional(CONF_INVERTED, default=False): cv.boolean})
    return schema
