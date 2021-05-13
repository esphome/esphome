import functools
import heapq
import inspect
import logging

import math
import os
import re

# pylint: disable=unused-import, wrong-import-order
from typing import Any, Dict, List, Optional, Set, TYPE_CHECKING  # noqa

from esphome.const import (
    CONF_ARDUINO_VERSION,
    SOURCE_FILE_EXTENSIONS,
    CONF_COMMENT,
    CONF_ESPHOME,
    CONF_USE_ADDRESS,
    CONF_ETHERNET,
    CONF_WIFI,
)
from esphome.helpers import ensure_unique_string, is_hassio
from esphome.util import OrderedDict

if TYPE_CHECKING:
    from ..cpp_generator import MockObj, MockObjClass, Statement

_LOGGER = logging.getLogger(__name__)


class EsphomeError(Exception):
    """General ESPHome exception occurred."""


class HexInt(int):
    def __str__(self):
        value = self
        sign = "-" if value < 0 else ""
        value = abs(value)
        if 0 <= value <= 255:
            return f"{sign}0x{value:02X}"
        return f"{sign}0x{value:X}"


class IPAddress:
    def __init__(self, *args):
        if len(args) != 4:
            raise ValueError("IPAddress must consist of 4 items")
        self.args = args

    def __str__(self):
        return ".".join(str(x) for x in self.args)


class MACAddress:
    def __init__(self, *parts):
        if len(parts) != 6:
            raise ValueError("MAC Address must consist of 6 items")
        self.parts = parts

    def __str__(self):
        return ":".join(f"{part:02X}" for part in self.parts)

    @property
    def as_hex(self):
        from esphome.cpp_generator import RawExpression

        num = "".join(f"{part:02X}" for part in self.parts)
        return RawExpression(f"0x{num}ULL")


def is_approximately_integer(value):
    if isinstance(value, int):
        return True
    return abs(value - round(value)) < 0.001


class TimePeriod:
    def __init__(
        self,
        microseconds=None,
        milliseconds=None,
        seconds=None,
        minutes=None,
        hours=None,
        days=None,
    ):
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
            out["microseconds"] = self.microseconds
        if self.milliseconds is not None:
            out["milliseconds"] = self.milliseconds
        if self.seconds is not None:
            out["seconds"] = self.seconds
        if self.minutes is not None:
            out["minutes"] = self.minutes
        if self.hours is not None:
            out["hours"] = self.hours
        if self.days is not None:
            out["days"] = self.days
        return out

    def __str__(self):
        if self.microseconds is not None:
            return f"{self.total_microseconds}us"
        if self.milliseconds is not None:
            return f"{self.total_milliseconds}ms"
        if self.seconds is not None:
            return f"{self.total_seconds}s"
        if self.minutes is not None:
            return f"{self.total_minutes}min"
        if self.hours is not None:
            return f"{self.total_hours}h"
        if self.days is not None:
            return f"{self.total_days}d"
        return "0s"

    def __repr__(self):
        return f"TimePeriod<{self.total_microseconds}>"

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
        if isinstance(other, TimePeriod):
            return self.total_microseconds == other.total_microseconds
        return NotImplemented

    def __ne__(self, other):
        if isinstance(other, TimePeriod):
            return self.total_microseconds != other.total_microseconds
        return NotImplemented

    def __lt__(self, other):
        if isinstance(other, TimePeriod):
            return self.total_microseconds < other.total_microseconds
        return NotImplemented

    def __gt__(self, other):
        if isinstance(other, TimePeriod):
            return self.total_microseconds > other.total_microseconds
        return NotImplemented

    def __le__(self, other):
        if isinstance(other, TimePeriod):
            return self.total_microseconds <= other.total_microseconds
        return NotImplemented

    def __ge__(self, other):
        if isinstance(other, TimePeriod):
            return self.total_microseconds >= other.total_microseconds
        return NotImplemented


class TimePeriodMicroseconds(TimePeriod):
    pass


class TimePeriodMilliseconds(TimePeriod):
    pass


class TimePeriodSeconds(TimePeriod):
    pass


class TimePeriodMinutes(TimePeriod):
    pass


LAMBDA_PROG = re.compile(r"id\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*\)(\.?)")


class Lambda:
    def __init__(self, value):
        # pylint: disable=protected-access
        if isinstance(value, Lambda):
            self._value = value._value
        else:
            self._value = value
        self._parts = None
        self._requires_ids = None

    # https://stackoverflow.com/a/241506/229052
    def comment_remover(self, text):
        def replacer(match):
            s = match.group(0)
            if s.startswith("/"):
                return " "  # note: a space and not an empty string
            return s

        pattern = re.compile(
            r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
            re.DOTALL | re.MULTILINE,
        )
        return re.sub(pattern, replacer, text)

    @property
    def parts(self):
        if self._parts is None:
            self._parts = re.split(LAMBDA_PROG, self.comment_remover(self._value))
        return self._parts

    @property
    def requires_ids(self):
        if self._requires_ids is None:
            self._requires_ids = [
                ID(self.parts[i]) for i in range(1, len(self.parts), 3)
            ]
        return self._requires_ids

    @property
    def value(self):
        return self._value

    @value.setter
    def value(self, value):
        self._value = value
        self._parts = None
        self._requires_ids = None

    def __str__(self):
        return self.value

    def __repr__(self):
        return f"Lambda<{self.value}>"


class ID:
    def __init__(self, id, is_declaration=False, type=None, is_manual=None):
        self.id = id
        if is_manual is None:
            self.is_manual = id is not None
        else:
            self.is_manual = is_manual
        self.is_declaration = is_declaration
        self.type: Optional["MockObjClass"] = type

    def resolve(self, registered_ids):
        from esphome.config_validation import RESERVED_IDS

        if self.id is None:
            base = str(self.type).replace("::", "_").lower()
            name = "".join(c for c in base if c.isalnum() or c == "_")
            used = set(registered_ids) | set(RESERVED_IDS)
            self.id = ensure_unique_string(name, used)
        return self.id

    def __str__(self):
        if self.id is None:
            return ""
        return self.id

    def __repr__(self):
        return (
            f"ID<{self.id} declaration={self.is_declaration}, "
            f"type={self.type}, manual={self.is_manual}>"
        )

    def __eq__(self, other):
        if isinstance(other, ID):
            return self.id == other.id
        return NotImplemented

    def __hash__(self):
        return hash(self.id)

    def copy(self):
        return ID(
            self.id,
            is_declaration=self.is_declaration,
            type=self.type,
            is_manual=self.is_manual,
        )


class DocumentLocation:
    def __init__(self, document: str, line: int, column: int):
        self.document: str = document
        self.line: int = line
        self.column: int = column

    @classmethod
    def from_mark(cls, mark):
        return cls(mark.name, mark.line, mark.column)

    def __str__(self):
        return f"{self.document} {self.line}:{self.column}"

    @property
    def as_line_directive(self):
        document_path = str(self.document).replace("\\", "\\\\")
        return f'#line {self.line + 1} "{document_path}"'


class DocumentRange:
    def __init__(self, start_mark: DocumentLocation, end_mark: DocumentLocation):
        self.start_mark: DocumentLocation = start_mark
        self.end_mark: DocumentLocation = end_mark

    @classmethod
    def from_marks(cls, start_mark, end_mark):
        return cls(
            DocumentLocation.from_mark(start_mark), DocumentLocation.from_mark(end_mark)
        )

    def __str__(self):
        return f"[{self.start_mark} - {self.end_mark}]"


class Define:
    def __init__(self, name, value=None):
        self.name = name
        self.value = value

    @property
    def as_build_flag(self):
        if self.value is None:
            return f"-D{self.name}"
        return f"-D{self.name}={self.value}"

    @property
    def as_macro(self):
        if self.value is None:
            return f"#define {self.name}"
        return f"#define {self.name} {self.value}"

    @property
    def as_tuple(self):
        return self.name, self.value

    def __hash__(self):
        return hash(self.as_tuple)

    def __eq__(self, other):
        if isinstance(other, Define):
            return self.as_tuple == other.as_tuple
        return NotImplemented


class Library:
    def __init__(self, name, version):
        self.name = name
        self.version = version

    @property
    def as_lib_dep(self):
        if self.version is None:
            return self.name
        return f"{self.name}@{self.version}"

    @property
    def as_tuple(self):
        return self.name, self.version

    def __hash__(self):
        return hash(self.as_tuple)

    def __eq__(self, other):
        if isinstance(other, Library):
            return self.as_tuple == other.as_tuple
        return NotImplemented


def coroutine(func):
    return coroutine_with_priority(0.0)(func)


def coroutine_with_priority(priority):
    def decorator(func):
        if getattr(func, "_esphome_coroutine", False):
            # If func is already a coroutine, do not re-wrap it (performance)
            return func

        @functools.wraps(func)
        def _wrapper_generator(*args, **kwargs):
            instance_id = kwargs.pop("__esphome_coroutine_instance__")
            if not inspect.isgeneratorfunction(func):
                # If func is not a generator, return result immediately
                yield func(*args, **kwargs)
                # pylint: disable=protected-access
                CORE._remove_coroutine(instance_id)
                return
            gen = func(*args, **kwargs)
            var = None
            try:
                while True:
                    var = gen.send(var)
                    if inspect.isgenerator(var):
                        # Yielded generator, equivalent to 'yield from'
                        x = None
                        for x in var:
                            yield None
                        # Last yield value is the result
                        var = x
                    else:
                        yield var
            except StopIteration:
                # Stopping iteration
                yield var
            # pylint: disable=protected-access
            CORE._remove_coroutine(instance_id)

        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            import random

            instance_id = random.randint(0, 2 ** 32)
            kwargs["__esphome_coroutine_instance__"] = instance_id
            gen = _wrapper_generator(*args, **kwargs)
            # pylint: disable=protected-access
            CORE._add_active_coroutine(instance_id, gen)
            return gen

        # pylint: disable=protected-access
        wrapper._esphome_coroutine = True
        wrapper.priority = priority
        return wrapper

    return decorator


def find_source_files(file):
    files = set()
    directory = os.path.abspath(os.path.dirname(file))
    for f in os.listdir(directory):
        if not os.path.isfile(os.path.join(directory, f)):
            continue
        _, ext = os.path.splitext(f)
        if ext.lower() not in SOURCE_FILE_EXTENSIONS:
            continue
        files.add(f)
    return files


# pylint: disable=too-many-instance-attributes,too-many-public-methods
class EsphomeCore:
    def __init__(self):
        # True if command is run from dashboard
        self.dashboard = False
        # True if command is run from vscode api
        self.vscode = False
        self.ace = False
        # The name of the node
        self.name: Optional[str] = None
        # The relative path to the configuration YAML
        self.config_path: Optional[str] = None
        # The relative path to where all build files are stored
        self.build_path: Optional[str] = None
        # The platform (ESP8266, ESP32) of this device
        self.esp_platform: Optional[str] = None
        # The board that's used (for example nodemcuv2)
        self.board: Optional[str] = None
        # The full raw configuration
        self.raw_config: Optional[ConfigType] = None
        # The validated configuration, this is None until the config has been validated
        self.config: Optional[ConfigType] = None
        # The pending tasks in the task queue (mostly for C++ generation)
        # This is a priority queue (with heapq)
        # Each item is a tuple of form: (-priority, unique number, task)
        self.pending_tasks = []
        # Task counter for pending tasks
        self.task_counter = 0
        # The variable cache, for each ID this holds a MockObj of the variable obj
        self.variables: Dict[str, "MockObj"] = {}
        # A list of statements that go in the main setup() block
        self.main_statements: List["Statement"] = []
        # A list of statements to insert in the global block (includes and global variables)
        self.global_statements: List["Statement"] = []
        # A set of platformio libraries to add to the project
        self.libraries: List[Library] = []
        # A set of build flags to set in the platformio project
        self.build_flags: Set[str] = set()
        # A set of defines to set for the compile process in esphome/core/defines.h
        self.defines: Set["Define"] = set()
        # A dictionary of started coroutines, used to warn when a coroutine was not
        # awaited.
        self.active_coroutines: Dict[int, Any] = {}
        # A set of strings of names of loaded integrations, used to find namespace ID conflicts
        self.loaded_integrations = set()
        # A set of component IDs to track what Component subclasses are declared
        self.component_ids = set()
        # Whether ESPHome was started in verbose mode
        self.verbose = False

    def reset(self):
        self.dashboard = False
        self.name = None
        self.config_path = None
        self.build_path = None
        self.esp_platform = None
        self.board = None
        self.raw_config = None
        self.config = None
        self.pending_tasks = []
        self.task_counter = 0
        self.variables = {}
        self.main_statements = []
        self.global_statements = []
        self.libraries = []
        self.build_flags = set()
        self.defines = set()
        self.active_coroutines = {}
        self.loaded_integrations = set()
        self.component_ids = set()

    @property
    def address(self) -> Optional[str]:
        if self.config is None:
            raise ValueError("Config has not been loaded yet")

        if "wifi" in self.config:
            return self.config[CONF_WIFI][CONF_USE_ADDRESS]

        if CONF_ETHERNET in self.config:
            return self.config[CONF_ETHERNET][CONF_USE_ADDRESS]

        return None

    @property
    def comment(self) -> Optional[str]:
        if self.config is None:
            raise ValueError("Config has not been loaded yet")

        if CONF_COMMENT in self.config[CONF_ESPHOME]:
            return self.config[CONF_ESPHOME][CONF_COMMENT]

        return None

    def _add_active_coroutine(self, instance_id, obj):
        self.active_coroutines[instance_id] = obj

    def _remove_coroutine(self, instance_id):
        self.active_coroutines.pop(instance_id)

    @property
    def arduino_version(self) -> str:
        if self.config is None:
            raise ValueError("Config has not been loaded yet")

        return self.config[CONF_ESPHOME][CONF_ARDUINO_VERSION]

    @property
    def config_dir(self):
        return os.path.dirname(self.config_path)

    @property
    def config_filename(self):
        return os.path.basename(self.config_path)

    def relative_config_path(self, *path):
        # pylint: disable=no-value-for-parameter
        path_ = os.path.expanduser(os.path.join(*path))
        return os.path.join(self.config_dir, path_)

    def relative_build_path(self, *path):
        # pylint: disable=no-value-for-parameter
        path_ = os.path.expanduser(os.path.join(*path))
        return os.path.join(self.build_path, path_)

    def relative_src_path(self, *path):
        return self.relative_build_path("src", *path)

    def relative_pioenvs_path(self, *path):
        if is_hassio():
            return os.path.join("/data", self.name, ".pioenvs", *path)
        return self.relative_build_path(".pioenvs", *path)

    def relative_piolibdeps_path(self, *path):
        if is_hassio():
            return os.path.join("/data", self.name, ".piolibdeps", *path)
        return self.relative_build_path(".piolibdeps", *path)

    @property
    def firmware_bin(self):
        return self.relative_pioenvs_path(self.name, "firmware.bin")

    @property
    def is_esp8266(self):
        if self.esp_platform is None:
            raise ValueError("No platform specified")
        return self.esp_platform == "ESP8266"

    @property
    def is_esp32(self):
        if self.esp_platform is None:
            raise ValueError("No platform specified")
        return self.esp_platform == "ESP32"

    def add_job(self, func, *args, **kwargs):
        coro = coroutine(func)
        task = coro(*args, **kwargs)
        item = (-coro.priority, self.task_counter, task)
        self.task_counter += 1
        heapq.heappush(self.pending_tasks, item)
        return task

    def flush_tasks(self):
        i = 0
        while self.pending_tasks:
            i += 1
            if i > 1000000:
                raise EsphomeError("Circular dependency detected!")

            inv_priority, num, task = heapq.heappop(self.pending_tasks)
            priority = -inv_priority
            _LOGGER.debug("Running %s (num %s)", task, num)
            try:
                next(task)
                # Decrease priority over time, so that if this task is blocked
                # due to a dependency others will clear the dependency
                # This could be improved with a less naive approach
                priority -= 1
                item = (-priority, num, task)
                heapq.heappush(self.pending_tasks, item)
            except StopIteration:
                _LOGGER.debug(" -> finished")

        # Print not-awaited coroutines
        for obj in self.active_coroutines.values():
            _LOGGER.warning(
                "Coroutine '%s' %s was never awaited with 'yield'.", obj.__name__, obj
            )
            _LOGGER.warning("Please file a bug report with your configuration.")
        if self.active_coroutines:
            raise EsphomeError()
        if self.component_ids:
            comps = ", ".join(f"'{x}'" for x in self.component_ids)
            _LOGGER.warning(
                "Components %s were never registered. Please create a bug report", comps
            )
            _LOGGER.warning("with your configuration.")
            raise EsphomeError()
        self.active_coroutines.clear()

    def add(self, expression):
        from esphome.cpp_generator import Expression, Statement, statement

        if isinstance(expression, Expression):
            expression = statement(expression)
        if not isinstance(expression, Statement):
            raise ValueError(
                "Add '{}' must be expression or statement, not {}"
                "".format(expression, type(expression))
            )

        self.main_statements.append(expression)
        _LOGGER.debug("Adding: %s", expression)
        return expression

    def add_global(self, expression):
        from esphome.cpp_generator import Expression, Statement, statement

        if isinstance(expression, Expression):
            expression = statement(expression)
        if not isinstance(expression, Statement):
            raise ValueError(
                "Add '{}' must be expression or statement, not {}"
                "".format(expression, type(expression))
            )
        self.global_statements.append(expression)
        _LOGGER.debug("Adding global: %s", expression)
        return expression

    def add_library(self, library):
        if not isinstance(library, Library):
            raise ValueError(
                "Library {} must be instance of Library, not {}"
                "".format(library, type(library))
            )
        _LOGGER.debug("Adding library: %s", library)
        for other in self.libraries[:]:
            if other.name != library.name:
                continue
            if library.version is None:
                # Other requirement is more specific
                break
            if other.version is None:
                # Found more specific version requirement
                self.libraries.remove(other)
                continue
            if other.version == library.version:
                break

            raise ValueError(
                "Version pinning failed! Libraries {} and {} "
                "requested with conflicting versions!"
                "".format(library, other)
            )
        else:
            self.libraries.append(library)
        return library

    def add_build_flag(self, build_flag):
        self.build_flags.add(build_flag)
        _LOGGER.debug("Adding build flag: %s", build_flag)
        return build_flag

    def add_define(self, define):
        if isinstance(define, str):
            define = Define(define)
        elif isinstance(define, Define):
            pass
        else:
            raise ValueError(
                "Define {} must be string or Define, not {}"
                "".format(define, type(define))
            )
        self.defines.add(define)
        _LOGGER.debug("Adding define: %s", define)
        return define

    def get_variable(self, id):
        if not isinstance(id, ID):
            raise ValueError(f"ID {id!r} must be of type ID!")
        while True:
            if id in self.variables:
                yield self.variables[id]
                return
            _LOGGER.debug("Waiting for variable %s (%r)", id, id)
            yield None

    def get_variable_with_full_id(self, id):
        while True:
            if id in self.variables:
                for k, v in self.variables.items():
                    if k == id:
                        yield (k, v)
                        return
            _LOGGER.debug("Waiting for variable %s", id)
            yield None, None

    def register_variable(self, id, obj):
        if id in self.variables:
            raise EsphomeError(f"ID {id} is already registered")
        _LOGGER.debug("Registered variable %s of type %s", id.id, id.type)
        self.variables[id] = obj

    def has_id(self, id):
        return id in self.variables

    @property
    def cpp_main_section(self):
        from esphome.cpp_generator import statement

        main_code = []
        for exp in self.main_statements:
            text = str(statement(exp))
            text = text.rstrip()
            main_code.append(text)
        return "\n".join(main_code) + "\n\n"

    @property
    def cpp_global_section(self):
        from esphome.cpp_generator import statement

        global_code = []
        for exp in self.global_statements:
            text = str(statement(exp))
            text = text.rstrip()
            global_code.append(text)
        return "\n".join(global_code) + "\n"


class AutoLoad(OrderedDict):
    pass


class EnumValue:
    """Special type used by ESPHome to mark enum values for cv.enum."""

    @property
    def enum_value(self):
        return getattr(self, "_enum_value", None)

    @enum_value.setter
    def enum_value(self, value):
        setattr(self, "_enum_value", value)


CORE = EsphomeCore()

ConfigType = Dict[str, Any]
CoreType = EsphomeCore
