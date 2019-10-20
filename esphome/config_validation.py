# coding=utf-8
"""Helpers for config validation using voluptuous."""
from __future__ import print_function

import logging
import os
import re
from contextlib import contextmanager
import uuid as uuid_
from datetime import datetime
from string import ascii_letters, digits

import voluptuous as vol

from esphome import core
from esphome.const import CONF_AVAILABILITY, CONF_COMMAND_TOPIC, CONF_DISCOVERY, CONF_ID, \
    CONF_INTERNAL, CONF_NAME, CONF_PAYLOAD_AVAILABLE, CONF_PAYLOAD_NOT_AVAILABLE, \
    CONF_RETAIN, CONF_SETUP_PRIORITY, CONF_STATE_TOPIC, CONF_TOPIC, \
    CONF_HOUR, CONF_MINUTE, CONF_SECOND, CONF_VALUE, CONF_UPDATE_INTERVAL, CONF_TYPE_ID, CONF_TYPE
from esphome.core import CORE, HexInt, IPAddress, Lambda, TimePeriod, TimePeriodMicroseconds, \
    TimePeriodMilliseconds, TimePeriodSeconds, TimePeriodMinutes
from esphome.helpers import list_starts_with
from esphome.py_compat import integer_types, string_types, text_type, IS_PY2, decode_text
from esphome.voluptuous_schema import _Schema

_LOGGER = logging.getLogger(__name__)

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

ALLOWED_NAME_CHARS = u'abcdefghijklmnopqrstuvwxyz0123456789_'

RESERVED_IDS = [
    # C++ keywords http://en.cppreference.com/w/cpp/keyword
    'alignas', 'alignof', 'and', 'and_eq', 'asm', 'auto', 'bitand', 'bitor', 'bool', 'break',
    'case', 'catch', 'char', 'char16_t', 'char32_t', 'class', 'compl', 'concept', 'const',
    'constexpr', 'const_cast', 'continue', 'decltype', 'default', 'delete', 'do', 'double',
    'dynamic_cast', 'else', 'enum', 'explicit', 'export', 'export', 'extern', 'false', 'float',
    'for', 'friend', 'goto', 'if', 'inline', 'int', 'long', 'mutable', 'namespace', 'new',
    'noexcept', 'not', 'not_eq', 'nullptr', 'operator', 'or', 'or_eq', 'private', 'protected',
    'public', 'register', 'reinterpret_cast', 'requires', 'return', 'short', 'signed', 'sizeof',
    'static', 'static_assert', 'static_cast', 'struct', 'switch', 'template', 'this',
    'thread_local', 'throw', 'true', 'try', 'typedef', 'typeid', 'typename', 'union', 'unsigned',
    'using', 'virtual', 'void', 'volatile', 'wchar_t', 'while', 'xor', 'xor_eq',

    'App', 'pinMode', 'delay', 'delayMicroseconds', 'digitalRead', 'digitalWrite', 'INPUT',
    'OUTPUT',
    'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t', 'int8_t', 'int16_t', 'int32_t', 'int64_t',
    'close', 'pause', 'sleep', 'open', 'setup', 'loop',
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
        super(Optional, self).__init__(key, default=default)


class Required(vol.Required):
    """Define a field to be required to be set. The validated configuration is guaranteed
    to contain this key.

    All required values should be acceessed with the `config[CONF_<KEY>]` syntax in code
    - *not* the `config.get(CONF_<KEY>)` syntax.
    """

    def __init__(self, key):
        super(Required, self).__init__(key)


def check_not_templatable(value):
    if isinstance(value, Lambda):
        raise Invalid("This option is not templatable!")


def alphanumeric(value):
    if value is None:
        raise Invalid("string value is None")
    value = text_type(value)
    if not value.isalnum():
        raise Invalid("string value is not alphanumeric")
    return value


def valid_name(value):
    value = string_strict(value)
    for c in value:
        if c not in ALLOWED_NAME_CHARS:
            raise Invalid(u"'{}' is an invalid character for names. Valid characters are: {}"
                          u" (lowercase, no spaces)".format(c, ALLOWED_NAME_CHARS))
    return value


def string(value):
    """Validate that a configuration value is a string. If not, automatically converts to a string.

    Note that this can be lossy, for example the input value 60.00 (float) will be turned into
    "60.0" (string). For values where this could be a problem `string_string` has to be used.
    """
    check_not_templatable(value)
    if isinstance(value, (dict, list)):
        raise Invalid("string value cannot be dictionary or list.")
    if isinstance(value, bool):
        raise Invalid("Auto-converted this value to boolean, please wrap the value in quotes.")
    if isinstance(value, text_type):
        return value
    if value is not None:
        return text_type(value)
    raise Invalid("string value is None")


def string_strict(value):
    """Like string, but only allows strings, and does not automatically convert other types to
    strings."""
    check_not_templatable(value)
    if isinstance(value, text_type):
        return value
    if isinstance(value, string_types):
        return text_type(value)
    raise Invalid("Must be string, got {}. did you forget putting quotes "
                  "around the value?".format(type(value)))


def icon(value):
    """Validate that a given config value is a valid icon."""
    value = string_strict(value)
    if not value:
        return value
    if value.startswith('mdi:'):
        return value
    raise Invalid('Icons should start with prefix "mdi:"')


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
    if isinstance(value, string_types):
        value = value.lower()
        if value in ('true', 'yes', 'on', 'enable'):
            return True
        if value in ('false', 'no', 'off', 'disable'):
            return False
    raise Invalid(u"Expected boolean value, but cannot convert {} to a boolean. "
                  u"Please use 'true' or 'false'".format(value))


def ensure_list(*validators):
    """Validate this configuration option to be a list.

    If the config value is not a list, it is automatically converted to a
    single-item list.

    None and empty dictionaries are converted to empty lists.
    """
    user = All(*validators)

    def validator(value):
        check_not_templatable(value)
        if value is None or (isinstance(value, dict) and not value):
            return []
        if not isinstance(value, list):
            return [user(value)]
        ret = []
        errs = []
        for i, val in enumerate(value):
            try:
                with prepend_path([i]):
                    ret.append(user(val))
            except MultipleInvalid as err:
                errs.extend(err.errors)
            except Invalid as err:
                errs.append(err)
        if errs:
            raise MultipleInvalid(errs)
        return ret

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
    if isinstance(value, integer_types):
        return value
    if isinstance(value, float):
        if int(value) == value:
            return int(value)
        raise Invalid("This option only accepts integers with no fractional part. Please remove "
                      "the fractional part from {}".format(value))
    value = string_strict(value).lower()
    base = 10
    if value.startswith('0x'):
        base = 16
    try:
        return int(value, base)
    except ValueError:
        raise Invalid(u"Expected integer, but cannot parse {} as an integer".format(value))


def int_range(min=None, max=None, min_included=True, max_included=True):
    """Validate that the config option is an integer in the given range."""
    if min is not None:
        assert isinstance(min, integer_types)
    if max is not None:
        assert isinstance(max, integer_types)
    return All(int_, Range(min=min, max=max, min_included=min_included, max_included=max_included))


def hex_int_range(min=None, max=None, min_included=True, max_included=True):
    """Validate that the config option is an integer in the given range."""
    return All(hex_int,
               Range(min=min, max=max, min_included=min_included, max_included=max_included))


def float_range(min=None, max=None, min_included=True, max_included=True):
    """Validate that the config option is a floating point number in the given range."""
    if min is not None:
        assert isinstance(min, (int, float))
    if max is not None:
        assert isinstance(max, (int, float))
    return All(float_, Range(min=min, max=max, min_included=min_included,
                             max_included=max_included))


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
    if '-' in value:
        raise Invalid("Dashes are not supported in IDs, please use underscores instead.")
    valid_chars = ascii_letters + digits + '_'
    for char in value:
        if char not in valid_chars:
            raise Invalid(u"IDs must only consist of upper/lowercase characters, the underscore"
                          u"character and numbers. The character '{}' cannot be used"
                          u"".format(char))
    if value in RESERVED_IDS:
        raise Invalid(u"ID '{}' is reserved internally and cannot be used".format(value))
    if value in CORE.loaded_integrations:
        raise Invalid(u"ID '{}' conflicts with the name of an esphome integration, please use "
                      u"another ID name.".format(value))
    return value


def use_id(type):
    """Declare that this configuration option should point to an ID with the given type."""
    def validator(value):
        check_not_templatable(value)
        if value is None:
            return core.ID(None, is_declaration=False, type=type)
        if isinstance(value, core.ID) and value.is_declaration is False and value.type is type:
            return value

        return core.ID(validate_id_name(value), is_declaration=False, type=type)

    return validator


def declare_id(type):
    """Declare that this configuration option should be used to declare a variable ID
    with the given type.

    If two IDs with the same name exist, a validation error is thrown.
    """
    def validator(value):
        check_not_templatable(value)
        if value is None:
            return core.ID(None, is_declaration=True, type=type)

        return core.ID(validate_id_name(value), is_declaration=True, type=type)

    return validator


def templatable(other_validators):
    """Validate that the configuration option can (optionally) be templated.

    The user can declare a value as template by using the '!lambda' tag. In that case,
    validation is skipped. Otherwise (if the value is not templated) the validator given
    as the first argument to this method is called.
    """
    schema = Schema(other_validators)

    def validator(value):
        if isinstance(value, Lambda):
            return returning_lambda(value)
        if isinstance(other_validators, dict):
            return schema(value)
        return schema(value)

    return validator


def only_on(platforms):
    """Validate that this option can only be specified on the given ESP platforms."""
    if not isinstance(platforms, list):
        platforms = [platforms]

    def validator_(obj):
        if CORE.esp_platform not in platforms:
            raise Invalid(u"This feature is only available on {}".format(platforms))
        return obj

    return validator_


only_on_esp32 = only_on('ESP32')
only_on_esp8266 = only_on('ESP8266')


# Adapted from:
# https://github.com/alecthomas/voluptuous/issues/115#issuecomment-144464666
def has_at_least_one_key(*keys):
    """Validate that at least one of the given keys exist in the config."""

    def validate(obj):
        """Test keys exist in dict."""
        if not isinstance(obj, dict):
            raise Invalid('expected dictionary')

        if not any(k in keys for k in obj):
            raise Invalid('Must contain at least one of {}.'.format(', '.join(keys)))
        return obj

    return validate


def has_exactly_one_key(*keys):
    """Validate that exactly one of the given keys exist in the config."""
    def validate(obj):
        if not isinstance(obj, dict):
            raise Invalid('expected dictionary')

        number = sum(k in keys for k in obj)
        if number > 1:
            raise Invalid("Cannot specify more than one of {}.".format(', '.join(keys)))
        if number < 1:
            raise Invalid('Must contain exactly one of {}.'.format(', '.join(keys)))
        return obj

    return validate


def has_at_most_one_key(*keys):
    """Validate that at most one of the given keys exist in the config."""
    def validate(obj):
        if not isinstance(obj, dict):
            raise Invalid('expected dictionary')

        number = sum(k in keys for k in obj)
        if number > 1:
            raise Invalid("Cannot specify more than one of {}.".format(', '.join(keys)))
        return obj

    return validate


TIME_PERIOD_ERROR = "Time period {} should be format number + unit, for example 5ms, 5s, 5min, 5h"

time_period_dict = All(
    Schema({
        Optional('days'): float_,
        Optional('hours'): float_,
        Optional('minutes'): float_,
        Optional('seconds'): float_,
        Optional('milliseconds'): float_,
        Optional('microseconds'): float_,
    }),
    has_at_least_one_key('days', 'hours', 'minutes', 'seconds', 'milliseconds', 'microseconds'),
    lambda value: TimePeriod(**value)
)


def time_period_str_colon(value):
    """Validate and transform time offset with format HH:MM[:SS]."""
    if isinstance(value, int):
        raise Invalid('Make sure you wrap time values in quotes')
    if not isinstance(value, str):
        raise Invalid(TIME_PERIOD_ERROR.format(value))

    try:
        parsed = [int(x) for x in value.split(':')]
    except ValueError:
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
        raise Invalid("Don't know what '{0}' means as it has no time *unit*! Did you mean "
                      "'{0}s'?".format(value))
    if not isinstance(value, string_types):
        raise Invalid("Expected string for time period with unit.")

    unit_to_kwarg = {
        'us': 'microseconds',
        'microseconds': 'microseconds',
        'ms': 'milliseconds',
        'milliseconds': 'milliseconds',
        's': 'seconds',
        'sec': 'seconds',
        'seconds': 'seconds',
        'min': 'minutes',
        'minutes': 'minutes',
        'h': 'hours',
        'hours': 'hours',
        'd': 'days',
        'days': 'days',
    }

    match = re.match(r"^([-+]?[0-9]*\.?[0-9]*)\s*(\w*)$", value)

    if match is None:
        raise Invalid(u"Expected time period with unit, "
                      u"got {}".format(value))
    kwarg = unit_to_kwarg[one_of(*unit_to_kwarg)(match.group(2))]

    return TimePeriod(**{kwarg: float(match.group(1))})


def time_period_in_milliseconds_(value):
    if value.microseconds is not None and value.microseconds != 0:
        raise Invalid("Maximum precision is milliseconds")
    return TimePeriodMilliseconds(**value.as_dict())


def time_period_in_microseconds_(value):
    return TimePeriodMicroseconds(**value.as_dict())


def time_period_in_seconds_(value):
    if value.microseconds is not None and value.microseconds != 0:
        raise Invalid("Maximum precision is seconds")
    if value.milliseconds is not None and value.milliseconds != 0:
        raise Invalid("Maximum precision is seconds")
    return TimePeriodSeconds(**value.as_dict())


def time_period_in_minutes_(value):
    if value.microseconds is not None and value.microseconds != 0:
        raise Invalid("Maximum precision is minutes")
    if value.milliseconds is not None and value.milliseconds != 0:
        raise Invalid("Maximum precision is minutes")
    if value.seconds is not None and value.seconds != 0:
        raise Invalid("Maximum precision is minutes")
    return TimePeriodMinutes(**value.as_dict())


def update_interval(value):
    if value == 'never':
        return 4294967295  # uint32_t max
    return positive_time_period_milliseconds(value)


time_period = Any(time_period_str_unit, time_period_str_colon, time_period_dict)
positive_time_period = All(time_period, Range(min=TimePeriod()))
positive_time_period_milliseconds = All(positive_time_period, time_period_in_milliseconds_)
positive_time_period_seconds = All(positive_time_period, time_period_in_seconds_)
positive_time_period_minutes = All(positive_time_period, time_period_in_minutes_)
time_period_microseconds = All(time_period, time_period_in_microseconds_)
positive_time_period_microseconds = All(positive_time_period, time_period_in_microseconds_)
positive_not_null_time_period = All(time_period,
                                    Range(min=TimePeriod(), min_included=False))


def time_of_day(value):
    value = string(value)
    try:
        date = datetime.strptime(value, '%H:%M:%S')
    except ValueError as err:
        try:
            date = datetime.strptime(value, '%H:%M:%S %p')
        except ValueError:
            raise Invalid("Invalid time of day: {}".format(err))

    return {
        CONF_HOUR: date.hour,
        CONF_MINUTE: date.minute,
        CONF_SECOND: date.second,
    }


def mac_address(value):
    value = string_strict(value)
    parts = value.split(':')
    if len(parts) != 6:
        raise Invalid("MAC Address must consist of 6 : (colon) separated parts")
    parts_int = []
    if any(len(part) != 2 for part in parts):
        raise Invalid("MAC Address must be format XX:XX:XX:XX:XX:XX")
    for part in parts:
        try:
            parts_int.append(int(part, 16))
        except ValueError:
            raise Invalid("MAC Address parts must be hexadecimal values from 00 to FF")

    return core.MACAddress(*parts_int)


def uuid(value):
    return Coerce(uuid_.UUID)(value)


METRIC_SUFFIXES = {
    'E': 1e18, 'P': 1e15, 'T': 1e12, 'G': 1e9, 'M': 1e6, 'k': 1e3, 'da': 10, 'd': 1e-1,
    'c': 1e-2, 'm': 0.001, u'µ': 1e-6, 'u': 1e-6, 'n': 1e-9, 'p': 1e-12, 'f': 1e-15, 'a': 1e-18,
    '': 1
}


def float_with_unit(quantity, regex_suffix, optional_unit=False):
    pattern = re.compile(r"^([-+]?[0-9]*\.?[0-9]*)\s*(\w*?)" + regex_suffix + r"$", re.UNICODE)

    def validator(value):
        if optional_unit:
            try:
                return float_(value)
            except Invalid:
                pass
        match = pattern.match(string(value))

        if match is None:
            raise Invalid(u"Expected {} with unit, got {}".format(quantity, value))

        mantissa = float(match.group(1))
        if match.group(2) not in METRIC_SUFFIXES:
            raise Invalid(u"Invalid {} suffix {}".format(quantity, match.group(2)))

        multiplier = METRIC_SUFFIXES[match.group(2)]
        return mantissa * multiplier

    return validator


frequency = float_with_unit("frequency", u"(Hz|HZ|hz)?")
resistance = float_with_unit("resistance", u"(Ω|Ω|ohm|Ohm|OHM)?")
current = float_with_unit("current", u"(a|A|amp|Amp|amps|Amps|ampere|Ampere)?")
voltage = float_with_unit("voltage", u"(v|V|volt|Volts)?")
distance = float_with_unit("distance", u"(m)")
framerate = float_with_unit("framerate", u"(FPS|fps|Fps|FpS|Hz)")
angle = float_with_unit("angle", u"(°|deg)", optional_unit=True)
_temperature_c = float_with_unit("temperature", u"(°C|° C|°|C)?")
_temperature_k = float_with_unit("temperature", u"(° K|° K|K)?")
_temperature_f = float_with_unit("temperature", u"(°F|° F|F)?")

if IS_PY2:
    # Override voluptuous invalid to unicode for py2
    def _vol_invalid_unicode(self):
        path = u' @ data[%s]' % u']['.join(map(repr, self.path)) \
            if self.path else u''
        # pylint: disable=no-member
        output = decode_text(self.message)
        if self.error_type:
            output += u' for ' + self.error_type
        return output + path

    Invalid.__unicode__ = _vol_invalid_unicode


def temperature(value):
    try:
        return _temperature_c(value)
    except Invalid as orig_err:  # noqa
        pass

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

    raise orig_err  # noqa


_color_temperature_mireds = float_with_unit('Color Temperature', r'(mireds|Mireds)')
_color_temperature_kelvin = float_with_unit('Color Temperature', r'(K|Kelvin)')


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
        raise Invalid(u"Expected number of bytes with unit, got {}".format(value))

    mantissa = int(match.group(1))
    if match.group(2) not in METRIC_SUFFIXES:
        raise Invalid(u"Invalid metric suffix {}".format(match.group(2)))
    multiplier = METRIC_SUFFIXES[match.group(2)]
    if multiplier < 1:
        raise Invalid(u"Only suffixes with positive exponents are supported. "
                      u"Got {}".format(match.group(2)))
    return int(mantissa * multiplier)


def hostname(value):
    value = string(value)
    if len(value) > 63:
        raise Invalid("Hostnames can only be 63 characters long")
    for c in value:
        if not (c.isalnum() or c in '_-'):
            raise Invalid("Hostname can only have alphanumeric characters and _ or -")
    return value


def domain(value):
    value = string(value)
    if re.match(vol.DOMAIN_REGEX, value) is not None:
        return value
    try:
        return str(ipv4(value))
    except Invalid:
        raise Invalid("Invalid domain: {}".format(value))


def domain_name(value):
    value = string_strict(value)
    if not value:
        return value
    if not value.startswith('.'):
        raise Invalid("Domain name must start with .")
    if value.startswith('..'):
        raise Invalid("Domain name must start with single .")
    for c in value:
        if not (c.isalnum() or c in '._-'):
            raise Invalid("Domain name can only have alphanumeric characters and _ or -")
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
    elif isinstance(value, string_types):
        parts = value.split('.')
    elif isinstance(value, IPAddress):
        return value
    else:
        raise Invalid("IPv4 address must consist of either string or "
                      "integer list")
    if len(parts) != 4:
        raise Invalid("IPv4 address must consist of four point-separated "
                      "integers")
    parts_ = list(map(int, parts))
    if not all(0 <= x < 256 for x in parts_):
        raise Invalid("IPv4 address parts must be in range from 0 to 255")
    return IPAddress(*parts_)


def _valid_topic(value):
    """Validate that this is a valid topic name/filter."""
    if isinstance(value, dict):
        raise Invalid("Can't use dictionary with topic")
    value = string(value)
    try:
        raw_value = value.encode('utf-8')
    except UnicodeError:
        raise Invalid("MQTT topic name/filter must be valid UTF-8 string.")
    if not raw_value:
        raise Invalid("MQTT topic name/filter must not be empty.")
    if len(raw_value) > 65535:
        raise Invalid("MQTT topic name/filter must not be longer than "
                      "65535 encoded bytes.")
    if '\0' in value:
        raise Invalid("MQTT topic name/filter must not contain null "
                      "character.")
    return value


def subscribe_topic(value):
    """Validate that we can subscribe using this MQTT topic."""
    value = _valid_topic(value)
    for i in (i for i, c in enumerate(value) if c == '+'):
        if (i > 0 and value[i - 1] != '/') or \
                (i < len(value) - 1 and value[i + 1] != '/'):
            raise Invalid("Single-level wildcard must occupy an entire "
                          "level of the filter")

    index = value.find('#')
    if index != -1:
        if index != len(value) - 1:
            # If there are multiple wildcards, this will also trigger
            raise Invalid("Multi-level wildcard must be the last "
                          "character in the topic filter.")
        if len(value) > 1 and value[index - 1] != '/':
            raise Invalid("Multi-level wildcard must be after a topic "
                          "level separator.")

    return value


def publish_topic(value):
    """Validate that we can publish using this MQTT topic."""
    value = _valid_topic(value)
    if '+' in value or '#' in value:
        raise Invalid("Wildcards can not be used in topic names")
    return value


def mqtt_payload(value):
    if value is None:
        return ''
    return string(value)


def mqtt_qos(value):
    try:
        value = int(value)
    except (TypeError, ValueError):
        raise Invalid(u"MQTT Quality of Service must be integer, got {}".format(value))
    return one_of(0, 1, 2)(value)


def requires_component(comp):
    """Validate that this option can only be specified when the component `comp` is loaded."""
    def validator(value):
        if comp not in CORE.raw_config:
            raise Invalid("This option requires component {}".format(comp))
        return value

    return validator


uint8_t = int_range(min=0, max=255)
uint16_t = int_range(min=0, max=65535)
uint32_t = int_range(min=0, max=4294967295)
hex_uint8_t = hex_int_range(min=0, max=255)
hex_uint16_t = hex_int_range(min=0, max=65535)
hex_uint32_t = hex_int_range(min=0, max=4294967295)
i2c_address = hex_uint8_t


def percentage(value):
    """Validate that the value is a percentage.

    The resulting value is an integer in the range 0.0 to 1.0.
    """
    value = possibly_negative_percentage(value)
    return zero_to_one_float(value)


def possibly_negative_percentage(value):
    has_percent_sign = isinstance(value, string_types) and value.endswith('%')
    if has_percent_sign:
        value = float(value[:-1].rstrip()) / 100.0
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
    return negative_one_to_one_float(value)


def percentage_int(value):
    if isinstance(value, string_types) and value.endswith('%'):
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
    options = u', '.join(u"'{}'".format(x) for x in values)
    lower = kwargs.pop('lower', False)
    upper = kwargs.pop('upper', False)
    string_ = kwargs.pop('string', False) or lower or upper
    to_int = kwargs.pop('int', False)
    to_float = kwargs.pop('float', False)
    space = kwargs.pop('space', ' ')
    if kwargs:
        raise ValueError

    def validator(value):
        if string_:
            value = string(value)
            value = value.replace(' ', space)
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
            options_ = [text_type(x) for x in values]
            option = text_type(value)
            matches = difflib.get_close_matches(option, options_)
            if matches:
                raise Invalid(u"Unknown value '{}', did you mean {}?"
                              u"".format(value, u", ".join(u"'{}'".format(x) for x in matches)))
            raise Invalid(u"Unknown value '{}', valid options are {}.".format(value, options))
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

    def validator(value):
        from esphome.yaml_util import make_data_base

        value = make_data_base(one_of_validator(value))
        cls = value.__class__
        value.__class__ = cls.__class__(cls.__name__ + "Enum", (cls, core.EnumValue), {})
        value.enum_value = mapping[value]
        return value

    return validator


LAMBDA_ENTITY_ID_PROG = re.compile(r'id\(\s*([a-zA-Z0-9_]+\.[.a-zA-Z0-9_]+)\s*\)')


def lambda_(value):
    """Coerce this configuration option to a lambda."""
    if not isinstance(value, Lambda):
        value = Lambda(string_strict(value))
    entity_id_parts = re.split(LAMBDA_ENTITY_ID_PROG, value.value)
    if len(entity_id_parts) != 1:
        entity_ids = ' '.join("'{}'".format(entity_id_parts[i])
                              for i in range(1, len(entity_id_parts), 2))
        raise Invalid("Lambda contains reference to entity-id-style ID {}. "
                      "The id() wrapper only works for ESPHome-internal types. For importing "
                      "states from Home Assistant use the 'homeassistant' sensor platforms."
                      "".format(entity_ids))
    return value


def returning_lambda(value):
    """Coerce this configuration option to a lambda.

    Additionally, make sure the lambda returns something.
    """
    value = lambda_(value)
    if u'return' not in value.value:
        raise Invalid("Lambda doesn't contain a 'return' statement, but the lambda "
                      "is expected to return a value. \n"
                      "Please make sure the lambda contains at least one "
                      "return statement.")
    return value


def dimensions(value):
    if isinstance(value, list):
        if len(value) != 2:
            raise Invalid(u"Dimensions must have a length of two, not {}".format(len(value)))
        try:
            width, height = int(value[0]), int(value[1])
        except ValueError:
            raise Invalid(u"Width and height dimensions must be integers")
        if width <= 0 or height <= 0:
            raise Invalid(u"Width and height must at least be 1")
        return [width, height]
    value = string(value)
    match = re.match(r"\s*([0-9]+)\s*[xX]\s*([0-9]+)\s*", value)
    if not match:
        raise Invalid(u"Invalid value '{}' for dimensions. Only WIDTHxHEIGHT is allowed.")
    return dimensions([match.group(1), match.group(2)])


def directory(value):
    import json
    from esphome.py_compat import safe_input
    value = string(value)
    path = CORE.relative_config_path(value)

    if CORE.vscode and (not CORE.ace or
                        os.path.abspath(path) == os.path.abspath(CORE.config_path)):
        print(json.dumps({
            'type': 'check_directory_exists',
            'path': path,
        }))
        data = json.loads(safe_input())
        assert data['type'] == 'directory_exists_response'
        if data['content']:
            return value
        raise Invalid(u"Could not find directory '{}'. Please make sure it exists (full path: {})."
                      u"".format(path, os.path.abspath(path)))

    if not os.path.exists(path):
        raise Invalid(u"Could not find directory '{}'. Please make sure it exists (full path: {})."
                      u"".format(path, os.path.abspath(path)))
    if not os.path.isdir(path):
        raise Invalid(u"Path '{}' is not a directory (full path: {})."
                      u"".format(path, os.path.abspath(path)))
    return value


def file_(value):
    import json
    from esphome.py_compat import safe_input
    value = string(value)
    path = CORE.relative_config_path(value)

    if CORE.vscode and (not CORE.ace or
                        os.path.abspath(path) == os.path.abspath(CORE.config_path)):
        print(json.dumps({
            'type': 'check_file_exists',
            'path': path,
        }))
        data = json.loads(safe_input())
        assert data['type'] == 'file_exists_response'
        if data['content']:
            return value
        raise Invalid(u"Could not find file '{}'. Please make sure it exists (full path: {})."
                      u"".format(path, os.path.abspath(path)))

    if not os.path.exists(path):
        raise Invalid(u"Could not find file '{}'. Please make sure it exists (full path: {})."
                      u"".format(path, os.path.abspath(path)))
    if not os.path.isfile(path):
        raise Invalid(u"Path '{}' is not a file (full path: {})."
                      u"".format(path, os.path.abspath(path)))
    return value


ENTITY_ID_CHARACTERS = 'abcdefghijklmnopqrstuvwxyz0123456789_'


def entity_id(value):
    """Validate that this option represents a valid Home Assistant entity id.

    Should only be used for 'homeassistant' platforms.
    """
    value = string_strict(value).lower()
    if value.count('.') != 1:
        raise Invalid("Entity ID must have exactly one dot in it")
    for x in value.split('.'):
        for c in x:
            if c not in ENTITY_ID_CHARACTERS:
                raise Invalid("Invalid character for entity ID: {}".format(c))
    return value


def extract_keys(schema):
    """Extract the names of the keys from the given schema."""
    if isinstance(schema, Schema):
        schema = schema.schema
    assert isinstance(schema, dict)
    keys = []
    for skey in list(schema.keys()):
        if isinstance(skey, string_types):
            keys.append(skey)
        elif isinstance(skey, vol.Marker) and isinstance(skey.schema, string_types):
            keys.append(skey.schema)
        else:
            raise ValueError()
    keys.sort()
    return keys


def typed_schema(schemas, **kwargs):
    """Create a schema that has a key to distinguish between schemas"""
    key = kwargs.pop('key', CONF_TYPE)
    key_validator = one_of(*schemas, **kwargs)

    def validator(value):
        if not isinstance(value, dict):
            raise Invalid("Value must be dict")
        if CONF_TYPE not in value:
            raise Invalid("type not specified!")
        value = value.copy()
        key_v = key_validator(value.pop(key))
        value = schemas[key_v](value)
        value[key] = key_v
        return value

    return validator


class GenerateID(Optional):
    """Mark this key as being an auto-generated ID key."""

    def __init__(self, key=CONF_ID):
        super(GenerateID, self).__init__(key, default=lambda: None)


class SplitDefault(Optional):
    """Mark this key to have a split default for ESP8266/ESP32."""

    def __init__(self, key, esp8266=vol.UNDEFINED, esp32=vol.UNDEFINED):
        super(SplitDefault, self).__init__(key)
        self._esp8266_default = vol.default_factory(esp8266)
        self._esp32_default = vol.default_factory(esp32)

    @property
    def default(self):
        if CORE.is_esp8266:
            return self._esp8266_default
        if CORE.is_esp32:
            return self._esp32_default
        raise ValueError

    @default.setter
    def default(self, value):
        # Ignore default set from vol.Optional
        pass


class OnlyWith(Optional):
    """Set the default value only if the given component is loaded."""

    def __init__(self, key, component, default=None):
        super(OnlyWith, self).__init__(key)
        self._component = component
        self._default = vol.default_factory(default)

    @property
    def default(self):
        if self._component not in CORE.raw_config:
            return vol.UNDEFINED
        return self._default

    @default.setter
    def default(self, value):
        # Ignore default set from vol.Optional
        pass


def _nameable_validator(config):
    if CONF_NAME not in config and CONF_ID not in config:
        raise Invalid("At least one of 'id:' or 'name:' is required!")
    if CONF_NAME not in config:
        id = config[CONF_ID]
        if not id.is_manual:
            raise Invalid("At least one of 'id:' or 'name:' is required!")
        config[CONF_NAME] = id.id
        config[CONF_INTERNAL] = True
        return config
    return config


def ensure_schema(schema):
    if not isinstance(schema, vol.Schema):
        return Schema(schema)
    return schema


def validate_registry_entry(name, registry):
    base_schema = ensure_schema(registry.base_schema).extend({
        Optional(CONF_TYPE_ID): valid,
    }, extra=ALLOW_EXTRA)
    ignore_keys = extract_keys(base_schema)

    def validator(value):
        if isinstance(value, string_types):
            value = {value: {}}
        if not isinstance(value, dict):
            raise Invalid(u"{} must consist of key-value mapping! Got {}"
                          u"".format(name.title(), value))
        key = next((x for x in value if x not in ignore_keys), None)
        if key is None:
            raise Invalid(u"Key missing from {}! Got {}".format(name, value))
        if key not in registry:
            raise Invalid(u"Unable to find {} with the name '{}'".format(name, key), [key])
        key2 = next((x for x in value if x != key and x not in ignore_keys), None)
        if key2 is not None:
            raise Invalid(u"Cannot have two {0}s in one item. Key '{1}' overrides '{2}'! "
                          u"Did you forget to indent the block inside the {0}?"
                          u"".format(name, key, key2))

        if value[key] is None:
            value[key] = {}

        registry_entry = registry[key]

        value = value.copy()

        with prepend_path([key]):
            value[key] = registry_entry.schema(value[key])

        if registry_entry.type_id is not None:
            my_base_schema = base_schema.extend({
                GenerateID(CONF_TYPE_ID): declare_id(registry_entry.type_id)
            })
            value = my_base_schema(value)

        return value

    return validator


def validate_registry(name, registry):
    return ensure_list(validate_registry_entry(name, registry))


def maybe_simple_value(*validators, **kwargs):
    key = kwargs.pop('key', CONF_VALUE)
    validator = All(*validators)

    def validate(value):
        if isinstance(value, dict) and key in value:
            return validator(value)
        return validator({key: value})

    return validate


MQTT_COMPONENT_AVAILABILITY_SCHEMA = Schema({
    Required(CONF_TOPIC): subscribe_topic,
    Optional(CONF_PAYLOAD_AVAILABLE, default='online'): mqtt_payload,
    Optional(CONF_PAYLOAD_NOT_AVAILABLE, default='offline'): mqtt_payload,
})

MQTT_COMPONENT_SCHEMA = Schema({
    Optional(CONF_NAME): string,
    Optional(CONF_RETAIN): All(requires_component('mqtt'), boolean),
    Optional(CONF_DISCOVERY): All(requires_component('mqtt'), boolean),
    Optional(CONF_STATE_TOPIC): All(requires_component('mqtt'), publish_topic),
    Optional(CONF_AVAILABILITY): All(requires_component('mqtt'),
                                     Any(None, MQTT_COMPONENT_AVAILABILITY_SCHEMA)),
    Optional(CONF_INTERNAL): boolean,
})
MQTT_COMPONENT_SCHEMA.add_extra(_nameable_validator)

MQTT_COMMAND_COMPONENT_SCHEMA = MQTT_COMPONENT_SCHEMA.extend({
    Optional(CONF_COMMAND_TOPIC): All(requires_component('mqtt'), subscribe_topic),
})

COMPONENT_SCHEMA = Schema({
    Optional(CONF_SETUP_PRIORITY): float_
})


def polling_component_schema(default_update_interval):
    """Validate that this component represents a PollingComponent with a configurable
    update_interval.

    :param default_update_interval: The default update interval to set for the integration.
    """
    if default_update_interval is None:
        return COMPONENT_SCHEMA.extend({
            Required(CONF_UPDATE_INTERVAL): default_update_interval,
        })
    assert isinstance(default_update_interval, string_types)
    return COMPONENT_SCHEMA.extend({
        Optional(CONF_UPDATE_INTERVAL, default=default_update_interval): update_interval,
    })
