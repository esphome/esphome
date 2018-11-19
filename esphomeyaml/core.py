import collections
from collections import OrderedDict
import inspect
import logging
import math
import os
import re

from typing import Any, Dict, List

from esphomeyaml.const import CONF_ARDUINO_VERSION, CONF_DOMAIN, CONF_ESPHOMELIB_VERSION, \
    CONF_ESPHOMEYAML, CONF_HOSTNAME, CONF_LOCAL, CONF_MANUAL_IP, CONF_STATIC_IP, CONF_WIFI, \
    ESP_PLATFORM_ESP32, ESP_PLATFORM_ESP8266
from esphomeyaml.helpers import ensure_unique_string

_LOGGER = logging.getLogger(__name__)


class EsphomeyamlError(Exception):
    """General esphomeyaml exception occurred."""
    pass


class HexInt(long):
    def __str__(self):
        if 0 <= self <= 255:
            return "0x{:02X}".format(self)
        return "0x{:X}".format(self)


class IPAddress(object):
    def __init__(self, *args):
        if len(args) != 4:
            raise ValueError(u"IPAddress must consist up 4 items")
        self.args = args

    def __str__(self):
        return '.'.join(str(x) for x in self.args)


class MACAddress(object):
    def __init__(self, *parts):
        if len(parts) != 6:
            raise ValueError(u"MAC Address must consist of 6 items")
        self.parts = parts

    def __str__(self):
        return ':'.join('{:02X}'.format(part) for part in self.parts)

    def as_hex(self):
        from esphomeyaml.cpp_generator import RawExpression

        num = ''.join('{:02X}'.format(part) for part in self.parts)
        return RawExpression('0x{}ULL'.format(num))


def is_approximately_integer(value):
    if isinstance(value, (int, long)):
        return True
    return abs(value - round(value)) < 0.001


class TimePeriod(object):
    def __init__(self, microseconds=None, milliseconds=None, seconds=None,
                 minutes=None, hours=None, days=None):
        if days is not None:
            if not is_approximately_integer(days):
                frac_days, days = math.modf(days)
                hours = (hours or 0) + frac_days * 24
            self.days = int(round(days))
        else:
            self.days = None

        if hours is not None:
            if not is_approximately_integer(hours):
                frac_hours, hours = math.modf(hours)
                minutes = (minutes or 0) + frac_hours * 60
            self.hours = int(round(hours))
        else:
            self.hours = None

        if minutes is not None:
            if not is_approximately_integer(minutes):
                frac_minutes, minutes = math.modf(minutes)
                seconds = (seconds or 0) + frac_minutes * 60
            self.minutes = int(round(minutes))
        else:
            self.minutes = None

        if seconds is not None:
            if not is_approximately_integer(seconds):
                frac_seconds, seconds = math.modf(seconds)
                milliseconds = (milliseconds or 0) + frac_seconds * 1000
            self.seconds = int(round(seconds))
        else:
            self.seconds = None

        if milliseconds is not None:
            if not is_approximately_integer(milliseconds):
                frac_milliseconds, milliseconds = math.modf(milliseconds)
                microseconds = (microseconds or 0) + frac_milliseconds * 1000
            self.milliseconds = int(round(milliseconds))
        else:
            self.milliseconds = None

        if microseconds is not None:
            if not is_approximately_integer(microseconds):
                raise ValueError("Maximum precision is microseconds")
            self.microseconds = int(round(microseconds))
        else:
            self.microseconds = None

    def as_dict(self):
        out = OrderedDict()
        if self.microseconds is not None:
            out['microseconds'] = self.microseconds
        if self.milliseconds is not None:
            out['milliseconds'] = self.milliseconds
        if self.seconds is not None:
            out['seconds'] = self.seconds
        if self.minutes is not None:
            out['minutes'] = self.minutes
        if self.hours is not None:
            out['hours'] = self.hours
        if self.days is not None:
            out['days'] = self.days
        return out

    def __str__(self):
        if self.microseconds is not None:
            return '{} us'.format(self.total_microseconds)
        if self.milliseconds is not None:
            return '{} ms'.format(self.total_milliseconds)
        if self.seconds is not None:
            return '{} s'.format(self.total_seconds)
        if self.minutes is not None:
            return '{} min'.format(self.total_minutes)
        if self.hours is not None:
            return '{} h'.format(self.total_hours)
        if self.days is not None:
            return '{} d'.format(self.total_days)
        return '0'

    @property
    def total_microseconds(self):
        return self.total_milliseconds * 1000 + (self.microseconds or 0)

    @property
    def total_milliseconds(self):
        return self.total_seconds * 1000 + (self.milliseconds or 0)

    @property
    def total_seconds(self):
        return self.total_minutes * 60 + (self.seconds or 0)

    @property
    def total_minutes(self):
        return self.total_hours * 60 + (self.minutes or 0)

    @property
    def total_hours(self):
        return self.total_days * 24 + (self.hours or 0)

    @property
    def total_days(self):
        return self.days or 0

    def __eq__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds == other.total_microseconds

    def __ne__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds != other.total_microseconds

    def __lt__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds < other.total_microseconds

    def __gt__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds > other.total_microseconds

    def __le__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds <= other.total_microseconds

    def __ge__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds >= other.total_microseconds


class TimePeriodMicroseconds(TimePeriod):
    pass


class TimePeriodMilliseconds(TimePeriod):
    pass


class TimePeriodSeconds(TimePeriod):
    pass


class Lambda(object):
    def __init__(self, value):
        self.value = value
        self.parts = re.split(r'id\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*\)(\.?)', value)
        self.requires_ids = [ID(self.parts[i]) for i in range(1, len(self.parts), 3)]

    def __str__(self):
        return self.value

    def __repr__(self):
        return u'Lambda<{}>'.format(self.value)


class ID(object):
    def __init__(self, id, is_declaration=False, type=None):
        self.id = id
        self.is_manual = id is not None
        self.is_declaration = is_declaration
        self.type = type

    def resolve(self, registered_ids):
        if self.id is None:
            base = str(self.type).replace('::', '_').lower()
            name = ''.join(c for c in base if c.isalnum() or c == '_')
            self.id = ensure_unique_string(name, registered_ids)
        return self.id

    def __str__(self):
        if self.id is None:
            return ''
        return self.id

    def __repr__(self):
        return u'ID<{} declaration={}, type={}, manual={}>'.format(
            self.id, self.is_declaration, self.type, self.is_manual)

    def __eq__(self, other):
        if not isinstance(other, ID):
            raise ValueError("other must be ID")
        return self.id == other.id

    def __hash__(self):
        return hash(self.id)


class EsphomeyamlCore(object):
    def __init__(self):
        # The name of the node
        self.name = None  # type: str
        # The relative path to the configuration YAML
        self.config_path = None  # type: str
        # The relative path to where all build files are stored
        self.build_path = None  # type: str
        # The platform (ESP8266, ESP32) of this device
        self.esp_platform = None  # type: str
        # The board that's used (for example nodemcuv2)
        self.board = None  # type: str
        # The full raw configuration
        self.raw_config = None  # type: ConfigType
        # The validated configuration, this is None until the config has been validated
        self.config = None  # type: ConfigType
        # The pending tasks in the task queue (mostly for C++ generation)
        self.pending_tasks = collections.deque()
        # The variable cache, for each ID this holds a MockObj of the variable obj
        self.variables = {}  # type: Dict[str, MockObj]
        # The list of expressions for the C++ generation
        self.expressions = []  # type: List[Expression]

    @property
    def address(self):  # type: () -> str
        if CONF_MANUAL_IP in self.config[CONF_WIFI]:
            return str(self.config[CONF_WIFI][CONF_MANUAL_IP][CONF_STATIC_IP])
        elif CONF_HOSTNAME in self.config[CONF_WIFI]:
            hostname = self.config[CONF_WIFI][CONF_HOSTNAME]
        else:
            hostname = self.name
        return hostname + self.config[CONF_WIFI][CONF_DOMAIN]

    @property
    def esphomelib_version(self):  # type: () -> Dict[str, str]
        return self.config[CONF_ESPHOMEYAML][CONF_ESPHOMELIB_VERSION]

    @property
    def is_local_esphomelib_copy(self):
        return CONF_LOCAL in self.esphomelib_version

    @property
    def arduino_version(self):  # type: () -> str
        return self.config[CONF_ESPHOMEYAML][CONF_ARDUINO_VERSION]

    @property
    def config_dir(self):
        return os.path.dirname(self.config_path)

    @property
    def config_filename(self):
        return os.path.basename(self.config_path)

    def relative_path(self, *path):
        path_ = os.path.expanduser(os.path.join(*path))
        return os.path.join(self.config_dir, path_)

    def relative_build_path(self, *path):
        path_ = os.path.expanduser(os.path.join(*path))
        return os.path.join(self.build_path, path_)

    @property
    def firmware_bin(self):
        return self.relative_build_path('.pioenvs', self.name, 'firmware.bin')

    @property
    def is_esp8266(self):
        if self.esp_platform is None:
            raise ValueError
        return self.esp_platform == ESP_PLATFORM_ESP8266

    @property
    def is_esp32(self):
        if self.esp_platform is None:
            raise ValueError
        return self.esp_platform == ESP_PLATFORM_ESP32

    def add_job(self, func, *args, **kwargs):
        domain = kwargs.get('domain')
        if inspect.isgeneratorfunction(func):
            def func_():
                yield
                for _ in func(*args):
                    yield
        else:
            def func_():
                yield
                func(*args)
        gen = func_()
        self.pending_tasks.append((gen, domain))
        return gen

    def flush_tasks(self):
        i = 0
        while self.pending_tasks:
            i += 1
            if i > 1000000:
                raise EsphomeyamlError("Circular dependency detected!")

            task, domain = self.pending_tasks.popleft()
            _LOGGER.debug("Executing task for domain=%s", domain)
            try:
                task.next()
                self.pending_tasks.append((task, domain))
            except StopIteration:
                _LOGGER.debug(" -> %s finished", domain)

    def add(self, expression, require=True):
        from esphomeyaml.cpp_generator import Expression

        if require and isinstance(expression, Expression):
            expression.require()
        self.expressions.append(expression)
        _LOGGER.debug("Adding: %s", expression)
        return expression

    def get_variable(self, id):
        while True:
            if id in self.variables:
                yield self.variables[id]
                return
            _LOGGER.debug("Waiting for variable %s", id)
            yield None

    def get_variable_with_full_id(self, id):
        while True:
            if id in self.variables:
                for k, v in self.variables.iteritems():
                    if k == id:
                        yield (k, v)
                        return
            _LOGGER.debug("Waiting for variable %s", id)
            yield None, None

    def register_variable(self, id, obj):
        _LOGGER.debug("Registered variable %s of type %s", id.id, id.type)
        self.variables[id] = obj


CORE = EsphomeyamlCore()

ConfigType = Dict[str, Any]
CoreType = EsphomeyamlCore
