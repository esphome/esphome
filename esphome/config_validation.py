"""Helpers for config validation using voluptuous."""

from dataclasses import dataclass
import logging
import os
import re
from contextlib import contextmanager
import uuid as uuid_
from datetime import datetime
from string import ascii_letters, digits

import voluptuous as vol

from esphome import core
import esphome.codegen as cg
from esphome.config_helpers import Extend, Remove
from esphome.const import (
    ALLOWED_NAME_CHARS,
    CONF_AVAILABILITY,
    CONF_COMMAND_TOPIC,
    CONF_COMMAND_RETAIN,
    CONF_DISABLED_BY_DEFAULT,
    CONF_DISCOVERY,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_INTERNAL,
    CONF_NAME,
    CONF_PAYLOAD_AVAILABLE,
    CONF_PAYLOAD_NOT_AVAILABLE,
    CONF_RETAIN,
    CONF_QOS,
    CONF_SETUP_PRIORITY,
    CONF_STATE_TOPIC,
    CONF_TOPIC,
    CONF_YEAR,
    CONF_MONTH,
    CONF_DAY,
    CONF_HOUR,
    CONF_MINUTE,
    CONF_SECOND,
    CONF_VALUE,
    CONF_UPDATE_INTERVAL,
    CONF_TYPE_ID,
    CONF_TYPE,
    CONF_REF,
    CONF_URL,
    CONF_PATH,
    CONF_USERNAME,
    CONF_PASSWORD,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RP2040,
    TYPE_GIT,
    TYPE_LOCAL,
    VALID_SUBSTITUTIONS_CHARACTERS,
    __version__ as ESPHOME_VERSION,
)
from esphome.core import (
    CORE,
    HexInt,
    IPAddress,
    Lambda,
    TimePeriod,
    TimePeriodMicroseconds,
    TimePeriodMilliseconds,
    TimePeriodNanoseconds,
    TimePeriodSeconds,
    TimePeriodMinutes,
)
from esphome.helpers import list_starts_with, add_class_to_obj
from esphome.schema_extractors import (
    SCHEMA_EXTRACT,
    schema_extractor_list,
    schema_extractor,
    schema_extractor_registry,
    schema_extractor_typed,
)
from esphome.util import parse_esphome_version
from esphome.voluptuous_schema import _Schema
from esphome.yaml_util import make_data_base

_LOGGER = logging.getLogger(__name__)

# pylint: disable=consider-using-f-string
VARIABLE_PROG = re.compile(
    "\\$([{0}]+|\\{{[{0}]*\\}})".format(VALID_SUBSTITUTIONS_CHARACTERS)
)

# pylint: disable=invalid-name

Schema = _Schema
All = vol.All
Coerce = vol.Coerce
Range = vol.Range
Invalid = vol.Invalid
MultipleInvalid = vol.MultipleInvalid
Any = vol.Any
Lower = vol.Lower
Upper = vol.Upper
Length = vol.Length
Exclusive = vol.Exclusive
Inclusive = vol.Inclusive
ALLOW_EXTRA = vol.ALLOW_EXTRA
UNDEFINED = vol.UNDEFINED
RequiredFieldInvalid = vol.RequiredFieldInvalid
# this sentinel object can be placed in an 'Invalid' path to say
# the rest of the error path is relative to the root config path
ROOT_CONFIG_PATH = object()

RESERVED_IDS = [
    # C++ keywords http://en.cppreference.com/w/cpp/keyword
    "alarm",
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "char16_t",
    "char32_t",
    "class",
    "clock",
    "compl",
    "concept",
    "const",
    "constexpr",
    "const_cast",
    "continue",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "requires",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "template",
    "text",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq",
    "App",
    "pinMode",
    "delay",
    "delayMicroseconds",
    "digitalRead",
    "digitalWrite",
    "INPUT",
    "OUTPUT",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "close",
    "pause",
    "sleep",
    "open",
    "setup",
    "loop",
    "uart0",
    "uart1",
    "uart2",
]


class Optional(vol.Optional):
    """Mark a field as optional and optionally define a default for the field.

    When no default is defined, the validated config will not contain the key.
    You can check if the key is defined with 'CONF_<KEY> in config'. Or to access
    the key and return None if it does not exist, call config.get(CONF_<KEY>)

    If a default *is* set, the resulting validated config will always contain the
    default value. You can therefore directly access the value using the
    'config[CONF_<KEY>]' syntax.

    In ESPHome, all configuration defaults should be defined with the Optional class
    during config validation - specifically *not* in the C++ code or the code generation
    phase.
    """

    def __init__(self, key, default=UNDEFINED):
        super().__init__(key, default=default)


class Required(vol.Required):
    """Define a field to be required to be set. The validated configuration is guaranteed
    to contain this key.

    All required values should be acceessed with the `config[CONF_<KEY>]` syntax in code
    - *not* the `config.get(CONF_<KEY>)` syntax.
    """

    def __init__(self, key, msg=None):
        super().__init__(key, msg=msg)


def check_not_templatable(value):
    if isinstance(value, Lambda):
        raise Invalid("This option is not templatable!")


def alphanumeric(value):
    if value is None:
        raise Invalid("string value is None")
    value = str(value)
    if not value.isalnum():
        raise Invalid(f"{value} is not alphanumeric")
    return value


def valid_name(value):
    value = string_strict(value)

    if CORE.vscode:
        # If the value is a substitution, it can't be validated until the substitution
        # is actually made.
        sub_match = VARIABLE_PROG.search(value)
        if sub_match:
            return value

    for c in value:
        if c not in ALLOWED_NAME_CHARS:
            raise Invalid(
                f"'{c}' is an invalid character for names. Valid characters are: "
                f"{ALLOWED_NAME_CHARS} (lowercase, no spaces)"
            )
    return value


def string(value):
    """Validate that a configuration value is a string. If not, automatically converts to a string.

    Note that this can be lossy, for example the input value 60.00 (float) will be turned into
    "60.0" (string). For values where this could be a problem `string_strict` has to be used.
    """
    check_not_templatable(value)
    if isinstance(value, (dict, list)):
        raise Invalid("string value cannot be dictionary or list.")
    if isinstance(value, bool):
        raise Invalid(
            "Auto-converted this value to boolean, please wrap the value in quotes."
        )
    if isinstance(value, str):
        return value
    if value is not None:
        return str(value)
    raise Invalid("string value is None")


def string_strict(value):
    """Like string, but only allows strings, and does not automatically convert other types to
    strings."""
    check_not_templatable(value)
    if isinstance(value, str):
        return value
    raise Invalid(
        f"Must be string, got {type(value)}. did you forget putting quotes around the value?"
    )


def icon(value):
    """Validate that a given config value is a valid icon."""
    value = string_strict(value)
    if not value:
        return value
    if re.match("^[\\w\\-]+:[\\w\\-]+$", value):
        return value
    raise Invalid(
        'Icons must match the format "[icon pack]:[icon]", e.g. "mdi:home-assistant"'
    )


def boolean(value):
    """Validate the given config option to be a boolean.

    This option allows a bunch of different ways of expressing boolean values:
     - instance of boolean
     - 'true'/'false'
     - 'yes'/'no'
     - 'enable'/disable
    """
    check_not_templatable(value)
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        value = value.lower()
        if value in ("true", "yes", "on", "enable"):
            return True
        if value in ("false", "no", "off", "disable"):
            return False
    raise Invalid(
        f"Expected boolean value, but cannot convert {value} to a boolean. Please use 'true' or 'false'"
    )


@schema_extractor_list
def ensure_list(*validators):
    """Validate this configuration option to be a list.

    If the config value is not a list, it is automatically converted to a
    single-item list.

    None and empty dictionaries are converted to empty lists.
    """
    user = All(*validators)
    list_schema = Schema([user])

    def validator(value):
        check_not_templatable(value)
        if value is None or (isinstance(value, dict) and not value):
            return []
        if not isinstance(value, list):
            return [user(value)]
        return list_schema(value)

    return validator


def hex_int(value):
    """Validate the given value to be a hex integer. This is mostly for cosmetic
    purposes of the generated code.
    """
    return HexInt(int_(value))


def int_(value):
    """Validate that the config option is an integer.

    Automatically also converts strings to ints.
    """
    check_not_templatable(value)
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        if int(value) == value:
            return int(value)
        raise Invalid(
            f"This option only accepts integers with no fractional part. Please remove the fractional part from {value}"
        )
    value = string_strict(value).lower()
    base = 10
    if value.startswith("0x"):
        base = 16
    try:
        return int(value, base)
    except ValueError:
        # pylint: disable=raise-missing-from
        raise Invalid(f"Expected integer, but cannot parse {value} as an integer")


def int_range(min=None, max=None, min_included=True, max_included=True):
    """Validate that the config option is an integer in the given range."""
    if min is not None:
        assert isinstance(min, int)
    if max is not None:
        assert isinstance(max, int)
    return All(
        int_,
        Range(min=min, max=max, min_included=min_included, max_included=max_included),
    )


def hex_int_range(min=None, max=None, min_included=True, max_included=True):
    """Validate that the config option is an integer in the given range."""
    return All(
        hex_int,
        Range(min=min, max=max, min_included=min_included, max_included=max_included),
    )


def float_range(min=None, max=None, min_included=True, max_included=True):
    """Validate that the config option is a floating point number in the given range."""
    if min is not None:
        assert isinstance(min, (int, float))
    if max is not None:
        assert isinstance(max, (int, float))
    return All(
        float_,
        Range(min=min, max=max, min_included=min_included, max_included=max_included),
    )


port = int_range(min=1, max=65535)
float_ = Coerce(float)
positive_float = float_range(min=0)
zero_to_one_float = float_range(min=0, max=1)
negative_one_to_one_float = float_range(min=-1, max=1)
positive_int = int_range(min=0)
positive_not_null_int = int_range(min=0, min_included=False)


def validate_id_name(value):
    """Validate that the given value would be a valid C++ identifier name."""
    value = string(value)
    if not value:
        raise Invalid("ID must not be empty")
    if value[0].isdigit():
        raise Invalid("First character in ID cannot be a digit.")
    if "-" in value:
        raise Invalid(
            "Dashes are not supported in IDs, please use underscores instead."
        )

    if CORE.vscode:
        # If the value is a substitution, it can't be validated until the substitution
        # is actually made
        sub_match = VARIABLE_PROG.match(value)
        if sub_match:
            return value

    valid_chars = f"{ascii_letters + digits}_"
    for char in value:
        if char not in valid_chars:
            raise Invalid(
                f"IDs must only consist of upper/lowercase characters, the underscorecharacter and numbers. The character '{char}' cannot be used"
            )
    if value in RESERVED_IDS:
        raise Invalid(f"ID '{value}' is reserved internally and cannot be used")
    if value in CORE.loaded_integrations:
        raise Invalid(
            f"ID '{value}' conflicts with the name of an esphome integration, please use another ID name."
        )
    return value


def use_id(type):
    """Declare that this configuration option should point to an ID with the given type."""

    @schema_extractor("use_id")
    def validator(value):
        if value == SCHEMA_EXTRACT:
            return type

        check_not_templatable(value)
        if value is None:
            return core.ID(None, is_declaration=False, type=type)
        if (
            isinstance(value, core.ID)
            and value.is_declaration is False
            and value.type is type
        ):
            return value

        return core.ID(validate_id_name(value), is_declaration=False, type=type)

    return validator


def declare_id(type):
    """Declare that this configuration option should be used to declare a variable ID
    with the given type.

    If two IDs with the same name exist, a validation error is thrown.
    """

    @schema_extractor("declare_id")
    def validator(value):
        if value == SCHEMA_EXTRACT:
            return type

        check_not_templatable(value)
        if value is None:
            return core.ID(None, is_declaration=True, type=type)

        if isinstance(value, Extend):
            raise Invalid(f"Source for extension of ID '{value.value}' was not found.")

        if isinstance(value, Remove):
            raise Invalid(f"Source for Removal of ID '{value.value}' was not found.")

        return core.ID(validate_id_name(value), is_declaration=True, type=type)

    return validator


def templatable(other_validators):
    """Validate that the configuration option can (optionally) be templated.

    The user can declare a value as template by using the '!lambda' tag. In that case,
    validation is skipped. Otherwise (if the value is not templated) the validator given
    as the first argument to this method is called.
    """
    schema = Schema(other_validators)

    @schema_extractor("templatable")
    def validator(value):
        if value == SCHEMA_EXTRACT:
            return other_validators

        if isinstance(value, Lambda):
            return returning_lambda(value)
        if isinstance(other_validators, dict):
            return schema(value)
        return schema(value)

    return validator


def only_on(platforms):
    """Validate that this option can only be specified on the given target platforms."""
    if not isinstance(platforms, list):
        platforms = [platforms]

    def validator_(obj):
        if CORE.target_platform not in platforms:
            raise Invalid(f"This feature is only available on {platforms}")
        return obj

    return validator_


def only_with_framework(frameworks):
    """Validate that this option can only be specified on the given frameworks."""
    if not isinstance(frameworks, list):
        frameworks = [frameworks]

    def validator_(obj):
        if CORE.target_framework not in frameworks:
            raise Invalid(
                f"This feature is only available with frameworks {frameworks}"
            )
        return obj

    return validator_


only_on_esp32 = only_on(PLATFORM_ESP32)
only_on_esp8266 = only_on(PLATFORM_ESP8266)
only_on_rp2040 = only_on(PLATFORM_RP2040)
only_with_arduino = only_with_framework("arduino")
only_with_esp_idf = only_with_framework("esp-idf")


# Adapted from:
# https://github.com/alecthomas/voluptuous/issues/115#issuecomment-144464666
def has_at_least_one_key(*keys):
    """Validate that at least one of the given keys exist in the config."""

    def validate(obj):
        """Test keys exist in dict."""
        if not isinstance(obj, dict):
            raise Invalid("expected dictionary")

        if not any(k in keys for k in obj):
            raise Invalid(f"Must contain at least one of {', '.join(keys)}.")
        return obj

    return validate


def has_exactly_one_key(*keys):
    """Validate that exactly one of the given keys exist in the config."""

    def validate(obj):
        if not isinstance(obj, dict):
            raise Invalid("expected dictionary")

        number = sum(k in keys for k in obj)
        if number > 1:
            raise Invalid(f"Cannot specify more than one of {', '.join(keys)}.")
        if number < 1:
            raise Invalid(f"Must contain exactly one of {', '.join(keys)}.")
        return obj

    return validate


def has_at_most_one_key(*keys):
    """Validate that at most one of the given keys exist in the config."""

    def validate(obj):
        if not isinstance(obj, dict):
            raise Invalid("expected dictionary")

        number = sum(k in keys for k in obj)
        if number > 1:
            raise Invalid(f"Cannot specify more than one of {', '.join(keys)}.")
        return obj

    return validate


def has_none_or_all_keys(*keys):
    """Validate that none or all of the given keys exist in the config."""

    def validate(obj):
        if not isinstance(obj, dict):
            raise Invalid("expected dictionary")

        number = sum(k in keys for k in obj)
        if number != 0 and number != len(keys):
            raise Invalid(f"Must specify either none or all of {', '.join(keys)}.")
        return obj

    return validate


TIME_PERIOD_ERROR = (
    "Time period {} should be format number + unit, for example 5ms, 5s, 5min, 5h"
)

time_period_dict = All(
    Schema(
        {
            Optional("days"): float_,
            Optional("hours"): float_,
            Optional("minutes"): float_,
            Optional("seconds"): float_,
            Optional("milliseconds"): float_,
            Optional("microseconds"): float_,
        }
    ),
    has_at_least_one_key(
        "days", "hours", "minutes", "seconds", "milliseconds", "microseconds"
    ),
    lambda value: TimePeriod(**value),
)


def time_period_str_colon(value):
    """Validate and transform time offset with format HH:MM[:SS]."""
    if isinstance(value, int):
        raise Invalid("Make sure you wrap time values in quotes")
    if not isinstance(value, str):
        raise Invalid(TIME_PERIOD_ERROR.format(value))

    try:
        parsed = [int(x) for x in value.split(":")]
    except ValueError:
        # pylint: disable=raise-missing-from
        raise Invalid(TIME_PERIOD_ERROR.format(value))

    if len(parsed) == 2:
        hour, minute = parsed
        second = 0
    elif len(parsed) == 3:
        hour, minute, second = parsed
    else:
        raise Invalid(TIME_PERIOD_ERROR.format(value))

    return TimePeriod(hours=hour, minutes=minute, seconds=second)


def time_period_str_unit(value):
    """Validate and transform time period with time unit and integer value."""
    check_not_templatable(value)

    if isinstance(value, int):
        raise Invalid(
            f"Don't know what '{value}' means as it has no time *unit*! Did you mean '{value}s'?"
        )
    if isinstance(value, TimePeriod):
        value = str(value)
    if not isinstance(value, str):
        raise Invalid("Expected string for time period with unit.")

    unit_to_kwarg = {
        "ns": "nanoseconds",
        "nanoseconds": "nanoseconds",
        "us": "microseconds",
        "microseconds": "microseconds",
        "ms": "milliseconds",
        "milliseconds": "milliseconds",
        "s": "seconds",
        "sec": "seconds",
        "seconds": "seconds",
        "min": "minutes",
        "minutes": "minutes",
        "h": "hours",
        "hours": "hours",
        "d": "days",
        "days": "days",
    }

    match = re.match(r"^([-+]?[0-9]*\.?[0-9]*)\s*(\w*)$", value)

    if match is None:
        raise Invalid(f"Expected time period with unit, got {value}")
    kwarg = unit_to_kwarg[one_of(*unit_to_kwarg)(match.group(2))]

    try:
        return TimePeriod(**{kwarg: float(match.group(1))})
    except ValueError as e:
        raise Invalid(e) from e


def time_period_in_milliseconds_(value):
    if value.microseconds is not None and value.microseconds != 0:
        raise Invalid("Maximum precision is milliseconds")
    return TimePeriodMilliseconds(**value.as_dict())


def time_period_in_microseconds_(value):
    if value.nanoseconds is not None and value.nanoseconds != 0:
        raise Invalid("Maximum precision is microseconds")
    return TimePeriodMicroseconds(**value.as_dict())


def time_period_in_nanoseconds_(value):
    return TimePeriodNanoseconds(**value.as_dict())


def time_period_in_seconds_(value):
    if value.nanoseconds is not None and value.nanoseconds != 0:
        raise Invalid("Maximum precision is seconds")
    if value.microseconds is not None and value.microseconds != 0:
        raise Invalid("Maximum precision is seconds")
    if value.milliseconds is not None and value.milliseconds != 0:
        raise Invalid("Maximum precision is seconds")
    return TimePeriodSeconds(**value.as_dict())


def time_period_in_minutes_(value):
    if value.nanoseconds is not None and value.nanoseconds != 0:
        raise Invalid("Maximum precision is minutes")
    if value.microseconds is not None and value.microseconds != 0:
        raise Invalid("Maximum precision is minutes")
    if value.milliseconds is not None and value.milliseconds != 0:
        raise Invalid("Maximum precision is minutes")
    if value.seconds is not None and value.seconds != 0:
        raise Invalid("Maximum precision is minutes")
    return TimePeriodMinutes(**value.as_dict())


def update_interval(value):
    if value == "never":
        return 4294967295  # uint32_t max
    return positive_time_period_milliseconds(value)


time_period = Any(time_period_str_unit, time_period_str_colon, time_period_dict)
positive_time_period = All(time_period, Range(min=TimePeriod()))
positive_time_period_milliseconds = All(
    positive_time_period, time_period_in_milliseconds_
)
positive_time_period_seconds = All(positive_time_period, time_period_in_seconds_)
positive_time_period_minutes = All(positive_time_period, time_period_in_minutes_)
time_period_microseconds = All(time_period, time_period_in_microseconds_)
positive_time_period_microseconds = All(
    positive_time_period, time_period_in_microseconds_
)
positive_time_period_nanoseconds = All(
    positive_time_period, time_period_in_nanoseconds_
)
positive_not_null_time_period = All(
    time_period, Range(min=TimePeriod(), min_included=False)
)


def time_of_day(value):
    return date_time(allowed_date=False, allowed_time=True)(value)


def date_time(allowed_date: bool = True, allowed_time: bool = True):

    pattern_str = r"^"  # Start of string
    if allowed_date:
        pattern_str += (
            r"("  # 1. Optional Date group
            r"\d{4}-\d{1,2}-\d{1,2}"  # Date
            r"(?:\s(?=.+))?"  # Space after date only if time is following
            r")?"  # End optional Date group
        )
    if allowed_time:
        pattern_str += (
            r"("  # 2. Optional Time group
            r"(\d{1,2}:\d{2})"  # 3. Hour/Minute
            r"(:\d{2})?"  # 4. Seconds
            r"("  # 5. Optional AM/PM group
            r"(\s)?"  # 6. Optional Space
            r"(?:AM|PM|am|pm)"  # AM/PM string matching
            r")?"  # End optional AM/PM group
            r")?"  # End optional Time group
        )
    pattern_str += r"$"  # End of string

    pattern = re.compile(pattern_str)

    exc_message = ""
    if allowed_date:
        exc_message += "date"
        if allowed_time:
            exc_message += "/"
    if allowed_time:
        exc_message += "time"

    schema = Schema({})
    if allowed_date:
        schema = schema.extend(
            {
                Optional(CONF_YEAR): int_range(min=1970, max=3000),
                Optional(CONF_MONTH): int_range(min=1, max=12),
                Optional(CONF_DAY): int_range(min=1, max=31),
            }
        )
    if allowed_time:
        schema = schema.extend(
            {
                Optional(CONF_HOUR): int_range(min=0, max=23),
                Optional(CONF_MINUTE): int_range(min=0, max=59),
                Optional(CONF_SECOND): int_range(min=0, max=59),
            }
        )

    def validator(value):
        if isinstance(value, dict):
            return schema(value)
        value = string(value)

        match = pattern.match(value)
        if match is None:
            # pylint: disable=raise-missing-from
            raise Invalid(f"Invalid {exc_message}: {value}")

        if allowed_date:
            has_date = match[1] is not None
        if allowed_time:
            has_time = match[2] is not None
            has_seconds = match[3] is not None
            has_ampm = match[4] is not None
            has_ampm_space = match[5] is not None

        format = ""
        if allowed_date and has_date:
            format += "%Y-%m-%d"
            if allowed_time and has_time:
                format += " "
        if allowed_time and has_time:
            format += "%H:%M"
            if has_seconds:
                format += ":%S"
            if has_ampm_space:
                format += " "
            if has_ampm:
                format += "%p"

        try:
            date_obj = datetime.strptime(value, format)
        except ValueError as err:
            # pylint: disable=raise-missing-from
            raise Invalid(f"Invalid {exc_message}: {err}")

        return_value = {}
        if allowed_date and has_date:
            return_value[CONF_YEAR] = date_obj.year
            return_value[CONF_MONTH] = date_obj.month
            return_value[CONF_DAY] = date_obj.day

        if allowed_time and has_time:
            return_value[CONF_HOUR] = date_obj.hour
            return_value[CONF_MINUTE] = date_obj.minute
            return_value[CONF_SECOND] = date_obj.second if has_seconds else 0

        return schema(return_value)

    return validator


def mac_address(value):
    value = string_strict(value)
    parts = value.split(":")
    if len(parts) != 6:
        raise Invalid("MAC Address must consist of 6 : (colon) separated parts")
    parts_int = []
    if any(len(part) != 2 for part in parts):
        raise Invalid("MAC Address must be format XX:XX:XX:XX:XX:XX")
    for part in parts:
        try:
            parts_int.append(int(part, 16))
        except ValueError:
            # pylint: disable=raise-missing-from
            raise Invalid("MAC Address parts must be hexadecimal values from 00 to FF")

    return core.MACAddress(*parts_int)


def bind_key(value):
    value = string_strict(value)
    parts = [value[i : i + 2] for i in range(0, len(value), 2)]
    if len(parts) != 16:
        raise Invalid("Bind key must consist of 16 hexadecimal numbers")
    parts_int = []
    if any(len(part) != 2 for part in parts):
        raise Invalid("Bind key must be format XX")
    for part in parts:
        try:
            parts_int.append(int(part, 16))
        except ValueError:
            # pylint: disable=raise-missing-from
            raise Invalid("Bind key must be hex values from 00 to FF")

    return "".join(f"{part:02X}" for part in parts_int)


def uuid(value):
    return Coerce(uuid_.UUID)(value)


METRIC_SUFFIXES = {
    "E": 1e18,
    "P": 1e15,
    "T": 1e12,
    "G": 1e9,
    "M": 1e6,
    "k": 1e3,
    "da": 10,
    "d": 1e-1,
    "c": 1e-2,
    "m": 0.001,
    "µ": 1e-6,
    "u": 1e-6,
    "n": 1e-9,
    "p": 1e-12,
    "f": 1e-15,
    "a": 1e-18,
    "": 1,
}


def float_with_unit(quantity, regex_suffix, optional_unit=False):
    pattern = re.compile(
        f"^([-+]?[0-9]*\\.?[0-9]*)\\s*(\\w*?){regex_suffix}$", re.UNICODE
    )

    def validator(value):
        if optional_unit:
            try:
                return float_(value)
            except Invalid:
                pass
        match = pattern.match(string(value))

        if match is None:
            raise Invalid(f"Expected {quantity} with unit, got {value}")

        mantissa = float(match.group(1))
        if match.group(2) not in METRIC_SUFFIXES:
            raise Invalid(f"Invalid {quantity} suffix {match.group(2)}")

        multiplier = METRIC_SUFFIXES[match.group(2)]
        return mantissa * multiplier

    return validator


frequency = float_with_unit("frequency", "(Hz|HZ|hz)?")
resistance = float_with_unit("resistance", "(Ω|Ω|ohm|Ohm|OHM)?")
current = float_with_unit("current", "(a|A|amp|Amp|amps|Amps|ampere|Ampere)?")
voltage = float_with_unit("voltage", "(v|V|volt|Volts)?")
distance = float_with_unit("distance", "(m)")
framerate = float_with_unit("framerate", "(FPS|fps|Fps|FpS|Hz)")
angle = float_with_unit("angle", "(°|deg)", optional_unit=True)
_temperature_c = float_with_unit("temperature", "(°C|° C|°|C)?")
_temperature_k = float_with_unit("temperature", "(° K|° K|K)?")
_temperature_f = float_with_unit("temperature", "(°F|° F|F)?")
decibel = float_with_unit("decibel", "(dB|dBm|db|dbm)", optional_unit=True)
pressure = float_with_unit("pressure", "(bar|Bar)", optional_unit=True)


def temperature(value):
    err = None
    try:
        return _temperature_c(value)
    except Invalid as orig_err:
        err = orig_err

    try:
        kelvin = _temperature_k(value)
        return kelvin - 273.15
    except Invalid:
        pass

    try:
        fahrenheit = _temperature_f(value)
        return (fahrenheit - 32) * (5 / 9)
    except Invalid:
        pass

    raise err


def temperature_delta(value):
    err = None
    try:
        return _temperature_c(value)
    except Invalid as orig_err:
        err = orig_err

    try:
        return _temperature_k(value)
    except Invalid:
        pass

    try:
        fahrenheit = _temperature_f(value)
        return fahrenheit * (5 / 9)
    except Invalid:
        pass

    raise err


_color_temperature_mireds = float_with_unit("Color Temperature", r"(mireds|Mireds)")
_color_temperature_kelvin = float_with_unit("Color Temperature", r"(K|Kelvin)")


def color_temperature(value):
    try:
        val = _color_temperature_mireds(value)
    except Invalid:
        val = 1000000.0 / _color_temperature_kelvin(value)
    if val < 0:
        raise Invalid("Color temperature cannot be negative")
    return val


def validate_bytes(value):
    value = string(value)
    match = re.match(r"^([0-9]+)\s*(\w*?)(?:byte|B|b)?s?$", value)

    if match is None:
        raise Invalid(f"Expected number of bytes with unit, got {value}")

    mantissa = int(match.group(1))
    if match.group(2) not in METRIC_SUFFIXES:
        raise Invalid(f"Invalid metric suffix {match.group(2)}")
    multiplier = METRIC_SUFFIXES[match.group(2)]
    if multiplier < 1:
        raise Invalid(
            f"Only suffixes with positive exponents are supported. Got {match.group(2)}"
        )
    return int(mantissa * multiplier)


def hostname(value):
    value = string(value)
    if re.match(r"^[a-z0-9-]{1,63}$", value, re.IGNORECASE) is not None:
        return value
    raise Invalid(f"Invalid hostname: {value}")


def domain(value):
    value = string(value)
    if re.match(vol.DOMAIN_REGEX, value) is not None:
        return value
    try:
        return str(ipv4(value))
    except Invalid as err:
        raise Invalid(f"Invalid domain: {value}") from err


def domain_name(value):
    value = string_strict(value)
    if not value:
        return value
    if not value.startswith("."):
        raise Invalid("Domain name must start with .")
    if value.startswith(".."):
        raise Invalid("Domain name must start with single .")
    for c in value:
        if not (c.isalnum() or c in "._-"):
            raise Invalid(
                "Domain name can only have alphanumeric characters and _ or -"
            )
    return value


def ssid(value):
    value = string_strict(value)
    if not value:
        raise Invalid("SSID can't be empty.")
    if len(value) > 32:
        raise Invalid("SSID can't be longer than 32 characters")
    return value


def ipv4(value):
    if isinstance(value, list):
        parts = value
    elif isinstance(value, str):
        parts = value.split(".")
    elif isinstance(value, IPAddress):
        return value
    else:
        raise Invalid("IPv4 address must consist of either string or integer list")
    if len(parts) != 4:
        raise Invalid("IPv4 address must consist of four point-separated integers")
    parts_ = list(map(int, parts))
    if not all(0 <= x < 256 for x in parts_):
        raise Invalid("IPv4 address parts must be in range from 0 to 255")
    return IPAddress(*parts_)


def _valid_topic(value):
    """Validate that this is a valid topic name/filter."""
    if value is None:  # Used to disable publishing and subscribing
        return ""
    if isinstance(value, dict):
        raise Invalid("Can't use dictionary with topic")
    value = string(value)
    try:
        raw_value = value.encode("utf-8")
    except UnicodeError as err:
        raise Invalid("MQTT topic name/filter must be valid UTF-8 string.") from err
    if not raw_value:
        raise Invalid("MQTT topic name/filter must not be empty.")
    if len(raw_value) > 65535:
        raise Invalid(
            "MQTT topic name/filter must not be longer than 65535 encoded bytes."
        )
    if "\0" in value:
        raise Invalid("MQTT topic name/filter must not contain null character.")
    return value


def subscribe_topic(value):
    """Validate that we can subscribe using this MQTT topic."""
    value = _valid_topic(value)
    for i in (i for i, c in enumerate(value) if c == "+"):
        if (i > 0 and value[i - 1] != "/") or (
            i < len(value) - 1 and value[i + 1] != "/"
        ):
            raise Invalid(
                "Single-level wildcard must occupy an entire level of the filter"
            )

    index = value.find("#")
    if index != -1:
        if index != len(value) - 1:
            # If there are multiple wildcards, this will also trigger
            raise Invalid(
                "Multi-level wildcard must be the last "
                "character in the topic filter."
            )
        if len(value) > 1 and value[index - 1] != "/":
            raise Invalid("Multi-level wildcard must be after a topic level separator.")

    return value


def publish_topic(value):
    """Validate that we can publish using this MQTT topic."""
    value = _valid_topic(value)
    if "+" in value or "#" in value:
        raise Invalid("Wildcards can not be used in topic names")
    return value


def mqtt_payload(value):
    if value is None:
        return ""
    return string(value)


def mqtt_qos(value):
    try:
        value = int(value)
    except (TypeError, ValueError):
        # pylint: disable=raise-missing-from
        raise Invalid(f"MQTT Quality of Service must be integer, got {value}")
    return one_of(0, 1, 2)(value)


def requires_component(comp):
    """Validate that this option can only be specified when the component `comp` is loaded."""

    def validator(value):
        if comp not in CORE.loaded_integrations:
            raise Invalid(f"This option requires component {comp}")
        return value

    return validator


uint8_t = int_range(min=0, max=255)
uint16_t = int_range(min=0, max=65535)
uint32_t = int_range(min=0, max=4294967295)
uint64_t = int_range(min=0, max=18446744073709551615)
hex_uint8_t = hex_int_range(min=0, max=255)
hex_uint16_t = hex_int_range(min=0, max=65535)
hex_uint32_t = hex_int_range(min=0, max=4294967295)
hex_uint64_t = hex_int_range(min=0, max=18446744073709551615)
i2c_address = hex_uint8_t


def percentage(value):
    """Validate that the value is a percentage.

    The resulting value is an integer in the range 0.0 to 1.0.
    """
    value = possibly_negative_percentage(value)
    return zero_to_one_float(value)


def possibly_negative_percentage(value):
    has_percent_sign = False
    if isinstance(value, str):
        try:
            if value.endswith("%"):
                has_percent_sign = True
                value = float(value[:-1].rstrip()) / 100.0
            else:
                value = float(value)
        except ValueError:
            # pylint: disable=raise-missing-from
            raise Invalid("invalid number")
    try:
        if value > 1:
            msg = "Percentage must not be higher than 100%."
            if not has_percent_sign:
                msg += " Please put a percent sign after the number!"
            raise Invalid(msg)
        if value < -1:
            msg = "Percentage must not be smaller than -100%."
            if not has_percent_sign:
                msg += " Please put a percent sign after the number!"
            raise Invalid(msg)
    except TypeError:
        raise Invalid(  # pylint: disable=raise-missing-from
            "Expected percentage or float between -1.0 and 1.0"
        )
    return negative_one_to_one_float(value)


def percentage_int(value):
    if isinstance(value, str) and value.endswith("%"):
        value = int(value[:-1].rstrip())
    return value


def invalid(message):
    """Mark this value as invalid. Each time *any* value is passed here it will result in a
    validation error with the given message.
    """

    def validator(value):
        raise Invalid(message)

    return validator


def valid(value):
    """A validator that is always valid and returns the value as-is."""
    return value


@contextmanager
def prepend_path(path):
    """A contextmanager helper to prepend a path to all voluptuous errors."""
    if not isinstance(path, (list, tuple)):
        path = [path]
    try:
        yield
    except vol.Invalid as e:
        e.prepend(path)
        raise e


@contextmanager
def remove_prepend_path(path):
    """A contextmanager helper to remove a path from a voluptuous error."""
    if not isinstance(path, (list, tuple)):
        path = [path]
    try:
        yield
    except vol.Invalid as e:
        if list_starts_with(e.path, path):
            # Can't set e.path (namedtuple
            for _ in range(len(path)):
                e.path.pop(0)
        raise e


def one_of(*values, **kwargs):
    """Validate that the config option is one of the given values.

    :param values: The valid values for this type

    :Keyword Arguments:
      - *lower* (``bool``, default=False): Whether to convert the incoming values to lowercase
        strings.
      - *upper* (``bool``, default=False): Whether to convert the incoming values to uppercase
        strings.
      - *int* (``bool``, default=False): Whether to convert the incoming values to integers.
      - *float* (``bool``, default=False): Whether to convert the incoming values to floats.
      - *space* (``str``, default=' '): What to convert spaces in the input string to.
    """
    options = ", ".join(f"'{x}'" for x in values)
    lower = kwargs.pop("lower", False)
    upper = kwargs.pop("upper", False)
    string_ = kwargs.pop("string", False) or lower or upper
    to_int = kwargs.pop("int", False)
    to_float = kwargs.pop("float", False)
    space = kwargs.pop("space", " ")
    if kwargs:
        raise ValueError

    @schema_extractor("one_of")
    def validator(value):
        if value == SCHEMA_EXTRACT:
            return values

        if string_:
            value = string(value)
            value = value.replace(" ", space)
        if to_int:
            value = int_(value)
        if to_float:
            value = float_(value)
        if lower:
            value = Lower(value)
        if upper:
            value = Upper(value)
        if value not in values:
            import difflib

            options_ = [str(x) for x in values]
            option = str(value)
            matches = difflib.get_close_matches(option, options_)
            if matches:
                matches_str = ", ".join(f"'{x}'" for x in matches)
                raise Invalid(f"Unknown value '{value}', did you mean {matches_str}?")
            raise Invalid(f"Unknown value '{value}', valid options are {options}.")
        return value

    return validator


def enum(mapping, **kwargs):
    """Validate this config option against an enum mapping.

    The mapping should be a dictionary with the key representing the config value name and
    a value representing the expression to set during code generation.

    Accepts all kwargs of one_of.
    """
    assert isinstance(mapping, dict)
    one_of_validator = one_of(*mapping, **kwargs)

    @schema_extractor("enum")
    def validator(value):
        if value == SCHEMA_EXTRACT:
            return mapping

        value = one_of_validator(value)
        value = add_class_to_obj(value, core.EnumValue)
        value.enum_value = mapping[value]
        return value

    return validator


LAMBDA_ENTITY_ID_PROG = re.compile(r"\Wid\(\s*([a-zA-Z0-9_]+\.[.a-zA-Z0-9_]+)\s*\)")


def lambda_(value):
    """Coerce this configuration option to a lambda."""
    if not isinstance(value, Lambda):
        value = make_data_base(Lambda(string_strict(value)), value)
    entity_id_parts = re.split(LAMBDA_ENTITY_ID_PROG, value.value)
    if len(entity_id_parts) != 1:
        entity_ids = " ".join(
            f"'{entity_id_parts[i]}'" for i in range(1, len(entity_id_parts), 2)
        )
        raise Invalid(
            f"Lambda contains reference to entity-id-style ID {entity_ids}. The id() wrapper only works for ESPHome-internal types. For importing states from Home Assistant use the 'homeassistant' sensor platforms."
        )
    return value


def returning_lambda(value):
    """Coerce this configuration option to a lambda.

    Additionally, make sure the lambda returns something.
    """
    value = lambda_(value)
    if "return" not in value.value:
        raise Invalid(
            "Lambda doesn't contain a 'return' statement, but the lambda "
            "is expected to return a value. \n"
            "Please make sure the lambda contains at least one "
            "return statement."
        )
    return value


def dimensions(value):
    if isinstance(value, list):
        if len(value) != 2:
            raise Invalid(f"Dimensions must have a length of two, not {len(value)}")
        try:
            width, height = int(value[0]), int(value[1])
        except ValueError:
            # pylint: disable=raise-missing-from
            raise Invalid("Width and height dimensions must be integers")
        if width <= 0 or height <= 0:
            raise Invalid("Width and height must at least be 1")
        return [width, height]
    value = string(value)
    match = re.match(r"\s*([0-9]+)\s*[xX]\s*([0-9]+)\s*", value)
    if not match:
        raise Invalid(
            "Invalid value '{}' for dimensions. Only WIDTHxHEIGHT is allowed."
        )
    return dimensions([match.group(1), match.group(2)])


def directory(value):
    import json

    value = string(value)
    path = CORE.relative_config_path(value)

    if CORE.vscode and (
        not CORE.ace or os.path.abspath(path) == os.path.abspath(CORE.config_path)
    ):
        print(
            json.dumps(
                {
                    "type": "check_directory_exists",
                    "path": path,
                }
            )
        )
        data = json.loads(input())
        assert data["type"] == "directory_exists_response"
        if data["content"]:
            return value
        raise Invalid(
            f"Could not find directory '{path}'. Please make sure it exists (full path: {os.path.abspath(path)})."
        )

    if not os.path.exists(path):
        raise Invalid(
            f"Could not find directory '{path}'. Please make sure it exists (full path: {os.path.abspath(path)})."
        )
    if not os.path.isdir(path):
        raise Invalid(
            f"Path '{path}' is not a directory (full path: {os.path.abspath(path)})."
        )
    return value


def file_(value):
    import json

    value = string(value)
    path = CORE.relative_config_path(value)

    if CORE.vscode and (
        not CORE.ace or os.path.abspath(path) == os.path.abspath(CORE.config_path)
    ):
        print(
            json.dumps(
                {
                    "type": "check_file_exists",
                    "path": path,
                }
            )
        )
        data = json.loads(input())
        assert data["type"] == "file_exists_response"
        if data["content"]:
            return value
        raise Invalid(
            f"Could not find file '{path}'. Please make sure it exists (full path: {os.path.abspath(path)})."
        )

    if not os.path.exists(path):
        raise Invalid(
            f"Could not find file '{path}'. Please make sure it exists (full path: {os.path.abspath(path)})."
        )
    if not os.path.isfile(path):
        raise Invalid(
            f"Path '{path}' is not a file (full path: {os.path.abspath(path)})."
        )
    return value


ENTITY_ID_CHARACTERS = "abcdefghijklmnopqrstuvwxyz0123456789_"


def entity_id(value):
    """Validate that this option represents a valid Home Assistant entity id.

    Should only be used for 'homeassistant' platforms.
    """
    value = string_strict(value).lower()
    if value.count(".") != 1:
        raise Invalid("Entity ID must have exactly one dot in it")
    for x in value.split("."):
        for c in x:
            if c not in ENTITY_ID_CHARACTERS:
                raise Invalid(f"Invalid character for entity ID: {c}")
    return value


def extract_keys(schema):
    """Extract the names of the keys from the given schema."""
    if isinstance(schema, Schema):
        schema = schema.schema
    assert isinstance(schema, dict)
    keys = []
    for skey in list(schema.keys()):
        if isinstance(skey, str):
            keys.append(skey)
        elif isinstance(skey, vol.Marker) and isinstance(skey.schema, str):
            keys.append(skey.schema)
        else:
            raise ValueError()
    keys.sort()
    return keys


@schema_extractor_typed
def typed_schema(schemas, **kwargs):
    """Create a schema that has a key to distinguish between schemas"""
    key = kwargs.pop("key", CONF_TYPE)
    default_schema_option = kwargs.pop("default_type", None)
    enum_mapping = kwargs.pop("enum", None)
    if enum_mapping is not None:
        assert isinstance(enum_mapping, dict)
        assert set(enum_mapping.keys()) == set(schemas.keys())
    key_validator = one_of(*schemas, **kwargs)

    def validator(value):
        if not isinstance(value, dict):
            raise Invalid("Value must be dict")
        value = value.copy()
        schema_option = value.pop(key, default_schema_option)
        if schema_option is None:
            raise Invalid(f"{key} not specified!")
        key_v = key_validator(schema_option)
        if enum_mapping is not None:
            key_v = add_class_to_obj(key_v, core.EnumValue)
            key_v.enum_value = enum_mapping[key_v]
        value = Schema(schemas[key_v])(value)
        value[key] = key_v
        return value

    return validator


class GenerateID(Optional):
    """Mark this key as being an auto-generated ID key."""

    def __init__(self, key=CONF_ID):
        super().__init__(key, default=lambda: None)


def _get_priority_default(*args):
    for arg in args:
        if arg is not vol.UNDEFINED:
            return arg
    return vol.UNDEFINED


class SplitDefault(Optional):
    """Mark this key to have a split default for ESP8266/ESP32."""

    def __init__(
        self,
        key,
        esp8266=vol.UNDEFINED,
        esp32=vol.UNDEFINED,
        esp32_arduino=vol.UNDEFINED,
        esp32_idf=vol.UNDEFINED,
        esp32_s2=vol.UNDEFINED,
        esp32_s2_arduino=vol.UNDEFINED,
        esp32_s2_idf=vol.UNDEFINED,
        esp32_s3=vol.UNDEFINED,
        esp32_s3_arduino=vol.UNDEFINED,
        esp32_s3_idf=vol.UNDEFINED,
        esp32_c3=vol.UNDEFINED,
        esp32_c3_arduino=vol.UNDEFINED,
        esp32_c3_idf=vol.UNDEFINED,
        rp2040=vol.UNDEFINED,
        bk72xx=vol.UNDEFINED,
        rtl87xx=vol.UNDEFINED,
        host=vol.UNDEFINED,
    ):
        super().__init__(key)
        self._esp8266_default = vol.default_factory(esp8266)
        self._esp32_arduino_default = vol.default_factory(
            _get_priority_default(esp32_arduino, esp32)
        )
        self._esp32_idf_default = vol.default_factory(
            _get_priority_default(esp32_idf, esp32)
        )
        self._esp32_s2_arduino_default = vol.default_factory(
            _get_priority_default(esp32_s2_arduino, esp32_s2, esp32_arduino, esp32)
        )
        self._esp32_s2_idf_default = vol.default_factory(
            _get_priority_default(esp32_s2_idf, esp32_s2, esp32_idf, esp32)
        )
        self._esp32_s3_arduino_default = vol.default_factory(
            _get_priority_default(esp32_s3_arduino, esp32_s3, esp32_arduino, esp32)
        )
        self._esp32_s3_idf_default = vol.default_factory(
            _get_priority_default(esp32_s3_idf, esp32_s3, esp32_idf, esp32)
        )
        self._esp32_c3_arduino_default = vol.default_factory(
            _get_priority_default(esp32_c3_arduino, esp32_c3, esp32_arduino, esp32)
        )
        self._esp32_c3_idf_default = vol.default_factory(
            _get_priority_default(esp32_c3_idf, esp32_c3, esp32_idf, esp32)
        )
        self._rp2040_default = vol.default_factory(rp2040)
        self._bk72xx_default = vol.default_factory(bk72xx)
        self._rtl87xx_default = vol.default_factory(rtl87xx)
        self._host_default = vol.default_factory(host)

    @property
    def default(self):
        if CORE.is_esp8266:
            return self._esp8266_default
        if CORE.is_esp32:
            from esphome.components.esp32 import get_esp32_variant
            from esphome.components.esp32.const import (
                VARIANT_ESP32S2,
                VARIANT_ESP32S3,
                VARIANT_ESP32C3,
            )

            variant = get_esp32_variant()
            if variant == VARIANT_ESP32S2:
                if CORE.using_arduino:
                    return self._esp32_s2_arduino_default
                if CORE.using_esp_idf:
                    return self._esp32_s2_idf_default
            elif variant == VARIANT_ESP32S3:
                if CORE.using_arduino:
                    return self._esp32_s3_arduino_default
                if CORE.using_esp_idf:
                    return self._esp32_s3_idf_default
            elif variant == VARIANT_ESP32C3:
                if CORE.using_arduino:
                    return self._esp32_c3_arduino_default
                if CORE.using_esp_idf:
                    return self._esp32_c3_idf_default
            else:
                if CORE.using_arduino:
                    return self._esp32_arduino_default
                if CORE.using_esp_idf:
                    return self._esp32_idf_default
        if CORE.is_rp2040:
            return self._rp2040_default
        if CORE.is_bk72xx:
            return self._bk72xx_default
        if CORE.is_rtl87xx:
            return self._rtl87xx_default
        if CORE.is_host:
            return self._host_default
        raise NotImplementedError

    @default.setter
    def default(self, value):
        # Ignore default set from vol.Optional
        pass


class OnlyWith(Optional):
    """Set the default value only if the given component is loaded."""

    def __init__(self, key, component, default=None):
        super().__init__(key)
        self._component = component
        self._default = vol.default_factory(default)

    @property
    def default(self):
        if self._component in CORE.loaded_integrations:
            return self._default
        return vol.UNDEFINED

    @default.setter
    def default(self, value):
        # Ignore default set from vol.Optional
        pass


def _entity_base_validator(config):
    if CONF_NAME not in config and CONF_ID not in config:
        raise Invalid("At least one of 'id:' or 'name:' is required!")
    if CONF_NAME not in config:
        id = config[CONF_ID]
        if not id.is_manual:
            raise Invalid("At least one of 'id:' or 'name:' is required!")
        config[CONF_NAME] = id.id
        config[CONF_INTERNAL] = True
        return config
    if config[CONF_NAME] is None:
        config[CONF_NAME] = ""
    return config


def ensure_schema(schema):
    if not isinstance(schema, vol.Schema):
        return Schema(schema)
    return schema


def validate_registry_entry(name, registry):
    base_schema = ensure_schema(registry.base_schema).extend(
        {
            Optional(CONF_TYPE_ID): valid,
        },
        extra=ALLOW_EXTRA,
    )
    ignore_keys = extract_keys(base_schema)

    @schema_extractor_registry(registry)
    def validator(value):
        if isinstance(value, str):
            value = {value: {}}
        if not isinstance(value, dict):
            raise Invalid(
                f"{name.title()} must consist of key-value mapping! Got {value}"
            )
        key = next((x for x in value if x not in ignore_keys), None)
        if key is None:
            raise Invalid(f"Key missing from {name}! Got {value}")
        if key not in registry:
            raise Invalid(f"Unable to find {name} with the name '{key}'", [key])
        key2 = next((x for x in value if x != key and x not in ignore_keys), None)
        if key2 is not None:
            raise Invalid(
                f"Cannot have two {name}s in one item. Key '{key}' overrides '{key2}'! "
                f"Did you forget to indent the block inside the {key}?"
            )

        if value[key] is None:
            value[key] = {}

        registry_entry = registry[key]

        value = value.copy()

        with prepend_path([key]):
            value[key] = registry_entry.schema(value[key])

        if registry_entry.type_id is not None:
            my_base_schema = base_schema.extend(
                {GenerateID(CONF_TYPE_ID): declare_id(registry_entry.type_id)}
            )
            value = my_base_schema(value)

        return value

    return validator


def none(value):
    if value in ("none", "None"):
        return None
    if boolean(value) is False:
        return None
    raise Invalid("Must be none")


def requires_friendly_name(message):
    def validate(value):
        if CORE.friendly_name is None:
            raise Invalid(message)
        return value

    return validate


def validate_registry(name, registry):
    return ensure_list(validate_registry_entry(name, registry))


def maybe_simple_value(*validators, **kwargs):
    key = kwargs.pop("key", CONF_VALUE)
    validator = All(*validators)

    @schema_extractor("maybe")
    def validate(value):
        if value == SCHEMA_EXTRACT:
            return (validator, key)

        if isinstance(value, dict) and key in value:
            return validator(value)
        return validator({key: value})

    return validate


ENTITY_CATEGORIES = {
    ENTITY_CATEGORY_NONE: cg.EntityCategory.ENTITY_CATEGORY_NONE,
    ENTITY_CATEGORY_CONFIG: cg.EntityCategory.ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC: cg.EntityCategory.ENTITY_CATEGORY_DIAGNOSTIC,
}


def entity_category(value):
    return enum(ENTITY_CATEGORIES, lower=True)(value)


MQTT_COMPONENT_AVAILABILITY_SCHEMA = Schema(
    {
        Required(CONF_TOPIC): subscribe_topic,
        Optional(CONF_PAYLOAD_AVAILABLE, default="online"): mqtt_payload,
        Optional(CONF_PAYLOAD_NOT_AVAILABLE, default="offline"): mqtt_payload,
    }
)

MQTT_COMPONENT_SCHEMA = Schema(
    {
        Optional(CONF_QOS): All(requires_component("mqtt"), int_range(min=0, max=2)),
        Optional(CONF_RETAIN): All(requires_component("mqtt"), boolean),
        Optional(CONF_DISCOVERY): All(requires_component("mqtt"), boolean),
        Optional(CONF_STATE_TOPIC): All(requires_component("mqtt"), publish_topic),
        Optional(CONF_AVAILABILITY): All(
            requires_component("mqtt"), Any(None, MQTT_COMPONENT_AVAILABILITY_SCHEMA)
        ),
    }
)

MQTT_COMMAND_COMPONENT_SCHEMA = MQTT_COMPONENT_SCHEMA.extend(
    {
        Optional(CONF_COMMAND_TOPIC): All(requires_component("mqtt"), subscribe_topic),
        Optional(CONF_COMMAND_RETAIN): All(requires_component("mqtt"), boolean),
    }
)

ENTITY_BASE_SCHEMA = Schema(
    {
        Optional(CONF_NAME): Any(
            All(
                none,
                requires_friendly_name(
                    "Name cannot be None when esphome->friendly_name is not set!"
                ),
            ),
            string,
        ),
        Optional(CONF_INTERNAL): boolean,
        Optional(CONF_DISABLED_BY_DEFAULT, default=False): boolean,
        Optional(CONF_ICON): icon,
        Optional(CONF_ENTITY_CATEGORY): entity_category,
    }
)

ENTITY_BASE_SCHEMA.add_extra(_entity_base_validator)

COMPONENT_SCHEMA = Schema({Optional(CONF_SETUP_PRIORITY): float_})


def polling_component_schema(default_update_interval):
    """Validate that this component represents a PollingComponent with a configurable
    update_interval.

    :param default_update_interval: The default update interval to set for the integration.
    """
    if default_update_interval is None:
        return COMPONENT_SCHEMA.extend(
            {
                Required(CONF_UPDATE_INTERVAL): default_update_interval,
            }
        )
    assert isinstance(default_update_interval, str)
    return COMPONENT_SCHEMA.extend(
        {
            Optional(
                CONF_UPDATE_INTERVAL, default=default_update_interval
            ): update_interval,
        }
    )


def url(value):
    import urllib.parse

    value = string_strict(value)
    try:
        parsed = urllib.parse.urlparse(value)
    except ValueError as e:
        raise Invalid("Not a valid URL") from e

    if not parsed.scheme or not parsed.netloc:
        raise Invalid("Expected a URL scheme and host")
    return parsed.geturl()


def git_ref(value):
    if re.match(r"[a-zA-Z0-9\-_.\./]+", value) is None:
        raise Invalid("Not a valid git ref")
    return value


def source_refresh(value: str):
    if value.lower() == "always":
        return source_refresh("0s")
    if value.lower() == "never":
        return source_refresh("365250d")
    return positive_time_period_seconds(value)


@dataclass(frozen=True, order=True)
class Version:
    major: int
    minor: int
    patch: int

    def __str__(self):
        return f"{self.major}.{self.minor}.{self.patch}"

    @classmethod
    def parse(cls, value: str) -> "Version":
        match = re.match(r"^(\d+).(\d+).(\d+)-?\w*$", value)
        if match is None:
            raise ValueError(f"Not a valid version number {value}")
        major = int(match[1])
        minor = int(match[2])
        patch = int(match[3])
        return Version(major=major, minor=minor, patch=patch)


def version_number(value):
    value = string_strict(value)
    try:
        return str(Version.parse(value))
    except ValueError as e:
        raise Invalid("Not a valid version number") from e


def validate_esphome_version(value: str):
    min_version = Version.parse(value)
    current_version = Version.parse(ESPHOME_VERSION)
    if current_version < min_version:
        raise Invalid(
            f"Your ESPHome version is too old. Please update to at least {min_version}"
        )
    return value


def platformio_version_constraint(value):
    # for documentation on valid version constraints:
    # https://docs.platformio.org/en/latest/core/userguide/platforms/cmd_install.html#cmd-platform-install

    value = string_strict(value)
    constraints = []
    for item in value.split(","):
        # find and strip prefix operator
        op = None
        for test_op in ("^", "~", ">=", ">", "<=", "<", "!="):
            if item.startswith(test_op):
                op = test_op
                item = item[len(test_op) :]
                break

        constraints.append((op, version_number(item)))
    return constraints


def require_framework_version(
    *,
    esp_idf=None,
    esp32_arduino=None,
    esp8266_arduino=None,
    rp2040_arduino=None,
    max_version=False,
    extra_message=None,
):
    def validator(value):
        core_data = CORE.data[KEY_CORE]
        framework = core_data[KEY_TARGET_FRAMEWORK]
        if framework == "esp-idf":
            if esp_idf is None:
                msg = "This feature is incompatible with esp-idf"
                if extra_message:
                    msg += f". {extra_message}"
                raise Invalid(msg)
            required = esp_idf
        elif CORE.is_esp32 and framework == "arduino":
            if esp32_arduino is None:
                msg = "This feature is incompatible with ESP32 using arduino framework"
                if extra_message:
                    msg += f". {extra_message}"
                raise Invalid(msg)
            required = esp32_arduino
        elif CORE.is_esp8266 and framework == "arduino":
            if esp8266_arduino is None:
                msg = "This feature is incompatible with ESP8266"
                if extra_message:
                    msg += f". {extra_message}"
                raise Invalid(msg)
            required = esp8266_arduino
        elif CORE.is_rp2040 and framework == "arduino":
            if rp2040_arduino is None:
                msg = "This feature is incompatible with RP2040"
                if extra_message:
                    msg += f". {extra_message}"
                raise Invalid(msg)
            required = rp2040_arduino
        else:
            raise Invalid(
                f"""
            Internal Error: require_framework_version does not support this platform configuration
                platform: {core_data[KEY_TARGET_PLATFORM]}
                framework: {framework}

            Please report this issue on GitHub -> https://github.com/esphome/issues/issues/new?template=bug_report.yml.
            """
            )

        if max_version:
            if core_data[KEY_FRAMEWORK_VERSION] > required:
                msg = f"This feature requires framework version {required} or lower"
                if extra_message:
                    msg += f". {extra_message}"
                raise Invalid(msg)
            return value

        if core_data[KEY_FRAMEWORK_VERSION] < required:
            msg = f"This feature requires at least framework version {required}"
            if extra_message:
                msg += f". {extra_message}"
            raise Invalid(msg)
        return value

    return validator


def require_esphome_version(year, month, patch):
    def validator(value):
        esphome_version = parse_esphome_version()
        if esphome_version < (year, month, patch):
            requires_version = f"{year}.{month}.{patch}"
            raise Invalid(
                f"This component requires at least ESPHome version {requires_version}"
            )
        return value

    return validator


@contextmanager
def suppress_invalid():
    try:
        yield
    except vol.Invalid:
        pass


GIT_SCHEMA = Schema(
    {
        Required(CONF_URL): url,
        Optional(CONF_REF): git_ref,
        Optional(CONF_USERNAME): string,
        Optional(CONF_PASSWORD): string,
        Optional(CONF_PATH): string,
    }
)
LOCAL_SCHEMA = Schema(
    {
        Required(CONF_PATH): directory,
    }
)


def validate_source_shorthand(value):
    if not isinstance(value, str):
        raise Invalid("Shorthand only for strings")
    try:
        return SOURCE_SCHEMA({CONF_TYPE: TYPE_LOCAL, CONF_PATH: value})
    except Invalid:
        pass
    # Regex for GitHub repo name with optional branch/tag
    # Note: git allows other branch/tag names as well, but never seen them used before
    m = re.match(
        r"github://(?:([a-zA-Z0-9\-]+)/([a-zA-Z0-9\-\._]+)(?:@([a-zA-Z0-9\-_.\./]+))?|pr#([0-9]+))",
        value,
    )
    if m is None:
        raise Invalid(
            "Source is not a file system path, in expected github://username/name[@branch-or-tag] or github://pr#1234 format!"
        )
    if m.group(4):
        conf = {
            CONF_TYPE: TYPE_GIT,
            CONF_URL: "https://github.com/esphome/esphome.git",
            CONF_REF: f"pull/{m.group(4)}/head",
        }
    else:
        conf = {
            CONF_TYPE: TYPE_GIT,
            CONF_URL: f"https://github.com/{m.group(1)}/{m.group(2)}.git",
        }
        if m.group(3):
            conf[CONF_REF] = m.group(3)

    return SOURCE_SCHEMA(conf)


SOURCE_SCHEMA = Any(
    validate_source_shorthand,
    typed_schema(
        {
            TYPE_GIT: GIT_SCHEMA,
            TYPE_LOCAL: LOCAL_SCHEMA,
        }
    ),
)
