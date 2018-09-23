import math
import re
from collections import OrderedDict


class ESPHomeYAMLError(Exception):
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
        import esphomeyaml.helpers

        num = ''.join('{:02X}'.format(part) for part in self.parts)
        return esphomeyaml.helpers.RawExpression('0x{}ULL'.format(num))


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


def ensure_unique_string(preferred_string, current_strings):
    test_string = preferred_string
    current_strings_set = set(current_strings)

    tries = 1

    while test_string in current_strings_set:
        tries += 1
        test_string = u"{}_{}".format(preferred_string, tries)

    return test_string


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


CONFIG_PATH = None
ESP_PLATFORM = ''
BOARD = ''
RAW_CONFIG = None
