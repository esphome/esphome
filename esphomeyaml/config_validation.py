# coding=utf-8
"""Helpers for config validation using voluptuous."""
from __future__ import print_function

import logging
import os
import re
import uuid as uuid_

import voluptuous as vol

from esphomeyaml import core, helpers
from esphomeyaml.const import CONF_AVAILABILITY, CONF_COMMAND_TOPIC, CONF_DISCOVERY, CONF_ID, \
    CONF_NAME, CONF_PAYLOAD_AVAILABLE, \
    CONF_PAYLOAD_NOT_AVAILABLE, CONF_PLATFORM, CONF_RETAIN, CONF_STATE_TOPIC, CONF_TOPIC, \
    ESP_PLATFORM_ESP32, ESP_PLATFORM_ESP8266, CONF_INTERNAL
from esphomeyaml.core import HexInt, IPAddress, Lambda, TimePeriod, TimePeriodMicroseconds, \
    TimePeriodMilliseconds, TimePeriodSeconds

_LOGGER = logging.getLogger(__name__)

# pylint: disable=invalid-name

port = vol.All(vol.Coerce(int), vol.Range(min=1, max=65535))
positive_float = vol.All(vol.Coerce(float), vol.Range(min=0))
zero_to_one_float = vol.All(vol.Coerce(float), vol.Range(min=0, max=1))
positive_int = vol.All(vol.Coerce(int), vol.Range(min=0))
positive_not_null_int = vol.All(vol.Coerce(int), vol.Range(min=0, min_included=False))

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
]


def alphanumeric(value):
    if value is None:
        raise vol.Invalid("string value is None")
    value = unicode(value)
    if not value.isalnum():
        raise vol.Invalid("string value is not alphanumeric")
    return value


def valid_name(value):
    value = string_strict(value)
    for c in value:
        if c not in ALLOWED_NAME_CHARS:
            raise vol.Invalid(u"'{}' is an invalid character for names. Valid characters are: {}"
                              u"".format(c, ALLOWED_NAME_CHARS))
    return value


def string(value):
    if isinstance(value, (dict, list)):
        raise vol.Invalid("string value cannot be dictionary or list.")
    if value is not None:
        return unicode(value)
    raise vol.Invalid("string value is None")


def string_strict(value):
    """Strictly only allow strings."""
    if isinstance(value, (str, unicode)):
        return value
    raise vol.Invalid("Must be string, did you forget putting quotes "
                      "around the value?")


def icon(value):
    """Validate icon."""
    value = string_strict(value)
    if value.startswith('mdi:'):
        return value
    raise vol.Invalid('Icons should start with prefix "mdi:"')


def boolean(value):
    """Validate and coerce a boolean value."""
    if isinstance(value, str):
        value = value.lower()
        if value in ('1', 'true', 'yes', 'on', 'enable'):
            return True
        if value in ('0', 'false', 'no', 'off', 'disable'):
            return False
        raise vol.Invalid('invalid boolean value {}'.format(value))
    return bool(value)


def ensure_list(value):
    """Wrap value in list if it is not one."""
    if value is None or (isinstance(value, dict) and not value):
        return []
    if isinstance(value, list):
        return value
    return [value]


def ensure_list_not_empty(value):
    if isinstance(value, list):
        return value
    return [value]


def ensure_dict(value):
    if value is None:
        return {}
    if not isinstance(value, dict):
        raise vol.Invalid("Expected a dictionary")
    return value


def hex_int_(value):
    if isinstance(value, (int, long)):
        return HexInt(value)
    value = string_strict(value).lower()
    if value.startswith('0x'):
        return HexInt(int(value, 16))
    return HexInt(int(value))


def int_(value):
    if isinstance(value, (int, long)):
        return value
    value = string_strict(value).lower()
    if value.startswith('0x'):
        return int(value, 16)
    return int(value)


hex_int = vol.Coerce(hex_int_)


def variable_id_str_(value):
    value = string(value)
    if not value:
        raise vol.Invalid("ID must not be empty")
    if value[0].isdigit():
        raise vol.Invalid("First character in ID cannot be a digit.")
    if '-' in value:
        raise vol.Invalid("Dashes are not supported in IDs, please use underscores instead.")
    for char in value:
        if char != '_' and not char.isalnum():
            raise vol.Invalid(u"IDs must only consist of upper/lowercase characters and numbers."
                              u"The character '{}' cannot be used".format(char))
    if value in RESERVED_IDS:
        raise vol.Invalid(u"ID {} is reserved internally and cannot be used".format(value))
    return value


def use_variable_id(type):
    def validator(value):
        if value is None:
            return core.ID(None, is_declaration=False, type=type)

        return core.ID(variable_id_str_(value), is_declaration=False, type=type)

    return validator


def declare_variable_id(type):
    def validator(value):
        if value is None:
            return core.ID(None, is_declaration=True, type=type)

        return core.ID(variable_id_str_(value), is_declaration=True, type=type)

    return validator


def templatable(other_validators):
    def validator(value):
        if isinstance(value, Lambda):
            return value
        if isinstance(other_validators, dict):
            return vol.Schema(other_validators)(value)
        return other_validators(value)

    return validator


def only_on(platforms):
    if not isinstance(platforms, list):
        platforms = [platforms]

    def validator_(obj):
        if core.ESP_PLATFORM not in platforms:
            raise vol.Invalid(u"This feature is only available on {}".format(platforms))
        return obj

    return validator_


only_on_esp32 = only_on(ESP_PLATFORM_ESP32)
only_on_esp8266 = only_on(ESP_PLATFORM_ESP8266)


# Adapted from:
# https://github.com/alecthomas/voluptuous/issues/115#issuecomment-144464666
def has_at_least_one_key(*keys):
    """Validate that at least one key exists."""

    def validate(obj):
        """Test keys exist in dict."""
        if not isinstance(obj, dict):
            raise vol.Invalid('expected dictionary')

        if not any(k in keys for k in obj):
            raise vol.Invalid('Must contain at least one of {}.'.format(', '.join(keys)))
        return obj

    return validate


def has_exactly_one_key(*keys):
    def validate(obj):
        if not isinstance(obj, dict):
            raise vol.Invalid('expected dictionary')

        number = sum(k in keys for k in obj)
        if number > 1:
            raise vol.Invalid("Cannot specify more than one of {}.".format(', '.join(keys)))
        if number < 1:
            raise vol.Invalid('Must contain exactly one of {}.'.format(', '.join(keys)))
        return obj

    return validate


def has_at_most_one_key(*keys):
    def validate(obj):
        if not isinstance(obj, dict):
            raise vol.Invalid('expected dictionary')

        number = sum(k in keys for k in obj)
        if number > 1:
            raise vol.Invalid("Cannot specify more than one of {}.".format(', '.join(keys)))
        return obj

    return validate


TIME_PERIOD_ERROR = "Time period {} should be format number + unit, for example 5ms, 5s, 5min, 5h"

time_period_dict = vol.All(
    dict, vol.Schema({
        'days': vol.Coerce(float),
        'hours': vol.Coerce(float),
        'minutes': vol.Coerce(float),
        'seconds': vol.Coerce(float),
        'milliseconds': vol.Coerce(float),
        'microseconds': vol.Coerce(float),
    }),
    has_at_least_one_key('days', 'hours', 'minutes',
                         'seconds', 'milliseconds', 'microseconds'),
    lambda value: TimePeriod(**value))


def time_period_str_colon(value):
    """Validate and transform time offset with format HH:MM[:SS]."""
    if isinstance(value, int):
        raise vol.Invalid('Make sure you wrap time values in quotes')
    elif not isinstance(value, str):
        raise vol.Invalid(TIME_PERIOD_ERROR.format(value))

    try:
        parsed = [int(x) for x in value.split(':')]
    except ValueError:
        raise vol.Invalid(TIME_PERIOD_ERROR.format(value))

    if len(parsed) == 2:
        hour, minute = parsed
        second = 0
    elif len(parsed) == 3:
        hour, minute, second = parsed
    else:
        raise vol.Invalid(TIME_PERIOD_ERROR.format(value))

    return TimePeriod(hours=hour, minutes=minute, seconds=second)


def time_period_str_unit(value):
    """Validate and transform time period with time unit and integer value."""
    if isinstance(value, int):
        value = str(value)
    elif not isinstance(value, (str, unicode)):
        raise vol.Invalid("Expected string for time period with unit.")

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

    if match is None or match.group(2) not in unit_to_kwarg:
        raise vol.Invalid(u"Expected time period with unit, "
                          u"got {}".format(value))

    kwarg = unit_to_kwarg[match.group(2)]
    return TimePeriod(**{kwarg: float(match.group(1))})


def time_period_in_milliseconds_(value):
    if value.microseconds is not None and value.microseconds != 0:
        raise vol.Invalid("Maximum precision is milliseconds")
    return TimePeriodMilliseconds(**value.as_dict())


def time_period_in_microseconds_(value):
    return TimePeriodMicroseconds(**value.as_dict())


def time_period_in_seconds_(value):
    if value.microseconds is not None and value.microseconds != 0:
        raise vol.Invalid("Maximum precision is seconds")
    if value.milliseconds is not None and value.milliseconds != 0:
        raise vol.Invalid("Maximum precision is seconds")
    return TimePeriodSeconds(**value.as_dict())


def update_interval(value):
    if value == 'never':
        return 4294967295  # uint32_t max
    return positive_time_period_milliseconds(value)


time_period = vol.Any(time_period_str_unit, time_period_str_colon, time_period_dict)
positive_time_period = vol.All(time_period, vol.Range(min=TimePeriod()))
positive_time_period_milliseconds = vol.All(positive_time_period, time_period_in_milliseconds_)
positive_time_period_seconds = vol.All(positive_time_period, time_period_in_seconds_)
time_period_microseconds = vol.All(time_period, time_period_in_microseconds_)
positive_time_period_microseconds = vol.All(positive_time_period, time_period_in_microseconds_)
positive_not_null_time_period = vol.All(time_period,
                                        vol.Range(min=TimePeriod(), min_included=False))


def mac_address(value):
    value = string_strict(value)
    parts = value.split(':')
    if len(parts) != 6:
        raise vol.Invalid("MAC Address must consist of 6 : (colon) separated parts")
    parts_int = []
    if any(len(part) != 2 for part in parts):
        raise vol.Invalid("MAC Address must be format XX:XX:XX:XX:XX:XX")
    for part in parts:
        try:
            parts_int.append(int(part, 16))
        except ValueError:
            raise vol.Invalid("MAC Address parts must be hexadecimal values from 00 to FF")

    return core.MACAddress(*parts_int)


def uuid(value):
    return vol.Coerce(uuid_.UUID)(value)


METRIC_SUFFIXES = {
    'E': 1e18, 'P': 1e15, 'T': 1e12, 'G': 1e9, 'M': 1e6, 'k': 1e3, 'da': 10, 'd': 1e-1,
    'c': 1e-2, 'm': 0.001, u'µ': 1e-6, 'u': 1e-6, 'n': 1e-9, 'p': 1e-12, 'f': 1e-15, 'a': 1e-18,
    '': 1
}


def float_with_unit(quantity, regex_suffix):
    pattern = re.compile(r"^([-+]?[0-9]*\.?[0-9]*)\s*(\w*?)" + regex_suffix + "$")

    def validator(value):
        match = pattern.match(string(value))

        if match is None:
            raise vol.Invalid(u"Expected {} with unit, got {}".format(quantity, value))

        mantissa = float(match.group(1))
        if match.group(2) not in METRIC_SUFFIXES:
            raise vol.Invalid(u"Invalid {} suffix {}".format(quantity, match.group(2)))

        multiplier = METRIC_SUFFIXES[match.group(2)]
        return mantissa * multiplier

    return validator


frequency = float_with_unit("frequency", r"(Hz|HZ|hz)?")
resistance = float_with_unit("resistance", r"(Ω|Ω|ohm|Ohm|OHM)?")
current = float_with_unit("current", r"(a|A|amp|Amp|amps|Amps|ampere|Ampere)?")
voltage = float_with_unit("voltage", r"(v|V|volt|Volts)?")


def validate_bytes(value):
    value = string(value)
    match = re.match(r"^([0-9]+)\s*(\w*?)(?:byte|B|b)?s?$", value)

    if match is None:
        raise vol.Invalid(u"Expected number of bytes with unit, got {}".format(value))

    mantissa = int(match.group(1))
    if match.group(2) not in METRIC_SUFFIXES:
        raise vol.Invalid(u"Invalid metric suffix {}".format(match.group(2)))
    multiplier = METRIC_SUFFIXES[match.group(2)]
    if multiplier < 1:
        raise vol.Invalid(u"Only suffixes with positive exponents are supported. "
                          u"Got {}".format(match.group(2)))
    return int(mantissa * multiplier)


def hostname(value):
    value = string(value)
    if len(value) > 63:
        raise vol.Invalid("Hostnames can only be 63 characters long")
    for c in value:
        if not (c.isalnum() or c in '_-'):
            raise vol.Invalid("Hostname can only have alphanumeric characters and _ or -")
    return value


def domain_name(value):
    value = string(value)
    if not value.startswith('.'):
        raise vol.Invalid("Domain name must start with .")
    if value.startswith('..'):
        raise vol.Invalid("Domain name must start with single .")
    for c in value:
        if not (c.isalnum() or c in '._-'):
            raise vol.Invalid("Domain name can only have alphanumeric characters and _ or -")
    return value


def ssid(value):
    if value is None:
        raise vol.Invalid("SSID can not be None")
    if not isinstance(value, str):
        raise vol.Invalid("SSID must be a string. Did you wrap it in quotes?")
    if not value:
        raise vol.Invalid("SSID can't be empty.")
    if len(value) > 31:
        raise vol.Invalid("SSID can't be longer than 31 characters")
    return value


def ipv4(value):
    if isinstance(value, list):
        parts = value
    elif isinstance(value, str):
        parts = value.split('.')
    else:
        raise vol.Invalid("IPv4 address must consist of either string or "
                          "integer list")
    if len(parts) != 4:
        raise vol.Invalid("IPv4 address must consist of four point-separated "
                          "integers")
    parts_ = list(map(int, parts))
    if not all(0 <= x < 256 for x in parts_):
        raise vol.Invalid("IPv4 address parts must be in range from 0 to 255")
    return IPAddress(*parts_)


def _valid_topic(value):
    """Validate that this is a valid topic name/filter."""
    if isinstance(value, dict):
        raise vol.Invalid("Can't use dictionary with topic")
    value = string(value)
    try:
        raw_value = value.encode('utf-8')
    except UnicodeError:
        raise vol.Invalid("MQTT topic name/filter must be valid UTF-8 string.")
    if not raw_value:
        raise vol.Invalid("MQTT topic name/filter must not be empty.")
    if len(raw_value) > 65535:
        raise vol.Invalid("MQTT topic name/filter must not be longer than "
                          "65535 encoded bytes.")
    if '\0' in value:
        raise vol.Invalid("MQTT topic name/filter must not contain null "
                          "character.")
    return value


def subscribe_topic(value):
    """Validate that we can subscribe using this MQTT topic."""
    value = _valid_topic(value)
    for i in (i for i, c in enumerate(value) if c == '+'):
        if (i > 0 and value[i - 1] != '/') or \
                (i < len(value) - 1 and value[i + 1] != '/'):
            raise vol.Invalid("Single-level wildcard must occupy an entire "
                              "level of the filter")

    index = value.find('#')
    if index != -1:
        if index != len(value) - 1:
            # If there are multiple wildcards, this will also trigger
            raise vol.Invalid("Multi-level wildcard must be the last "
                              "character in the topic filter.")
        if len(value) > 1 and value[index - 1] != '/':
            raise vol.Invalid("Multi-level wildcard must be after a topic "
                              "level separator.")

    return value


def publish_topic(value):
    """Validate that we can publish using this MQTT topic."""
    value = _valid_topic(value)
    if '+' in value or '#' in value:
        raise vol.Invalid("Wildcards can not be used in topic names")
    return value


def mqtt_payload(value):
    if value is None:
        return ''
    return string(value)


def mqtt_qos(value):
    try:
        value = int(value)
    except (TypeError, ValueError):
        raise vol.Invalid(u"MQTT Quality of Service must be integer, got {}".format(value))
    return one_of(0, 1, 2)(value)


uint8_t = vol.All(int_, vol.Range(min=0, max=255))
uint16_t = vol.All(int_, vol.Range(min=0, max=65535))
uint32_t = vol.All(int_, vol.Range(min=0, max=4294967295))
hex_uint8_t = vol.All(hex_int, vol.Range(min=0, max=255))
hex_uint16_t = vol.All(hex_int, vol.Range(min=0, max=65535))
hex_uint32_t = vol.All(hex_int, vol.Range(min=0, max=4294967295))
i2c_address = hex_uint8_t


def percentage(value):
    if isinstance(value, (str, unicode)) and value.endswith('%'):
        value = float(value[:-1].rstrip()) / 100.0
    return zero_to_one_float(value)


def percentage_int(value):
    if isinstance(value, (str, unicode)) and value.endswith('%'):
        value = int(value[:-1].rstrip())
    return value


def invalid(message):
    def validator(value):
        raise vol.Invalid(message)

    return validator


def valid(value):
    return value


def one_of(*values):
    options = u', '.join(u"'{}'".format(x) for x in values)

    def validator(value):
        if value not in values:
            raise vol.Invalid(u"Unknown value '{}', must be one of {}".format(value, options))
        return value

    return validator


def lambda_(value):
    if isinstance(value, Lambda):
        return value
    return Lambda(string_strict(value))


def dimensions(value):
    if isinstance(value, list):
        if len(value) != 2:
            raise vol.Invalid(u"Dimensions must have a length of two, not {}".format(len(value)))
        try:
            width, height = int(value[0]), int(value[1])
        except ValueError:
            raise vol.Invalid(u"Width and height dimensions must be integers")
        if width <= 0 or height <= 0:
            raise vol.Invalid(u"Width and height must at least be 1")
        return [width, height]
    value = string(value)
    match = re.match(r"\s*([0-9]+)\s*[xX]\s*([0-9]+)\s*", value)
    if not match:
        raise vol.Invalid(u"Invalid value '{}' for dimensions. Only WIDTHxHEIGHT is allowed.")
    return dimensions([match.group(1), match.group(2)])


def directory(value):
    value = string(value)
    path = helpers.relative_path(value)
    if not os.path.exists(path):
        raise vol.Invalid(u"Could not find directory '{}'. Please make sure it exists.".format(
            path))
    if not os.path.isdir(path):
        raise vol.Invalid(u"Path '{}' is not a directory.".format(path))
    return value


def file_(value):
    value = string(value)
    path = helpers.relative_path(value)
    if not os.path.exists(path):
        raise vol.Invalid(u"Could not find file '{}'. Please make sure it exists.".format(
            path))
    if not os.path.isfile(path):
        raise vol.Invalid(u"Path '{}' is not a file.".format(path))
    return value


REGISTERED_IDS = set()


class GenerateID(vol.Optional):
    def __init__(self, key=CONF_ID):
        super(GenerateID, self).__init__(key, default=lambda: None)


def nameable(*schemas):
    def validator(config):
        config = vol.All(*schemas)(config)
        if CONF_NAME not in config and CONF_ID not in config:
            raise vol.Invalid("At least one of 'id:' or 'name:' is required!")
        if CONF_NAME not in config:
            id = config[CONF_ID]
            if not id.is_manual:
                raise vol.Invalid("At least one of 'id:' or 'name:' is required!")
            config[CONF_NAME] = id.id
            config[CONF_INTERNAL] = True
            return config
        return config

    return validator


PLATFORM_SCHEMA = vol.Schema({
    vol.Required(CONF_PLATFORM): valid,
})

MQTT_COMPONENT_AVAILABILITY_SCHEMA = vol.Schema({
    vol.Required(CONF_TOPIC): subscribe_topic,
    vol.Optional(CONF_PAYLOAD_AVAILABLE, default='online'): mqtt_payload,
    vol.Optional(CONF_PAYLOAD_NOT_AVAILABLE, default='offline'): mqtt_payload,
})

MQTT_COMPONENT_SCHEMA = vol.Schema({
    vol.Optional(CONF_NAME): string,
    vol.Optional(CONF_RETAIN): boolean,
    vol.Optional(CONF_DISCOVERY): boolean,
    vol.Optional(CONF_STATE_TOPIC): publish_topic,
    vol.Optional(CONF_AVAILABILITY): vol.Any(None, MQTT_COMPONENT_AVAILABILITY_SCHEMA),
    vol.Optional(CONF_INTERNAL): boolean,
})

MQTT_COMMAND_COMPONENT_SCHEMA = MQTT_COMPONENT_SCHEMA.extend({
    vol.Optional(CONF_COMMAND_TOPIC): subscribe_topic,
})
