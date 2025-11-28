from collections import defaultdict
from contextlib import contextmanager
import logging
import math
import os
from pathlib import Path
import re
from typing import TYPE_CHECKING

from esphome.const import (
    CONF_COMMENT,
    CONF_ESPHOME,
    CONF_ETHERNET,
    CONF_OPENTHREAD,
    CONF_PORT,
    CONF_USE_ADDRESS,
    CONF_WEB_SERVER,
    CONF_WIFI,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_HOST,
    PLATFORM_LN882X,
    PLATFORM_NRF52,
    PLATFORM_RP2040,
    PLATFORM_RTL87XX,
)

# pylint: disable=unused-import
from esphome.coroutine import (  # noqa: F401
    CoroPriority,
    FakeAwaitable as _FakeAwaitable,
    FakeEventLoop as _FakeEventLoop,
    coroutine,
    coroutine_with_priority,
)
from esphome.helpers import ensure_unique_string, get_str_env, is_ha_addon
from esphome.util import OrderedDict

if TYPE_CHECKING:
    from esphome.address_cache import AddressCache

    from ..cpp_generator import MockObj, MockObjClass, Statement
    from ..types import ConfigType, EntityMetadata

_LOGGER = logging.getLogger(__name__)

# Key for tracking controller count in CORE.data for ControllerRegistry StaticVector sizing
KEY_CONTROLLER_REGISTRY_COUNT = "controller_registry_count"


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
        nanoseconds=None,
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
                frac_microseconds, microseconds = math.modf(microseconds)
                nanoseconds = (nanoseconds or 0) + frac_microseconds * 1000
            self.microseconds = int(round(microseconds))
        else:
            self.microseconds = None

        if nanoseconds is not None:
            if not is_approximately_integer(nanoseconds):
                raise ValueError("Maximum precision is nanoseconds")
            self.nanoseconds = int(round(nanoseconds))
        else:
            self.nanoseconds = None

    def as_dict(self):
        out = OrderedDict()
        if self.nanoseconds is not None:
            out["nanoseconds"] = self.nanoseconds
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
        if self.nanoseconds is not None:
            return f"{self.total_nanoseconds}ns"
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
        return f"TimePeriod<{self.total_nanoseconds}ns>"

    @property
    def total_nanoseconds(self):
        return self.total_microseconds * 1000 + (self.nanoseconds or 0)

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
            return self.total_nanoseconds == other.total_nanoseconds
        return NotImplemented

    def __ne__(self, other):
        if isinstance(other, TimePeriod):
            return self.total_nanoseconds != other.total_nanoseconds
        return NotImplemented

    def __lt__(self, other):
        if isinstance(other, TimePeriod):
            return self.total_nanoseconds < other.total_nanoseconds
        return NotImplemented

    def __gt__(self, other):
        if isinstance(other, TimePeriod):
            return self.total_nanoseconds > other.total_nanoseconds
        return NotImplemented

    def __le__(self, other):
        if isinstance(other, TimePeriod):
            return self.total_nanoseconds <= other.total_nanoseconds
        return NotImplemented

    def __ge__(self, other):
        if isinstance(other, TimePeriod):
            return self.total_nanoseconds >= other.total_nanoseconds
        return NotImplemented


class TimePeriodNanoseconds(TimePeriod):
    pass


class TimePeriodMicroseconds(TimePeriod):
    pass


class TimePeriodMilliseconds(TimePeriod):
    pass


class TimePeriodSeconds(TimePeriod):
    pass


class TimePeriodMinutes(TimePeriod):
    pass


LAMBDA_PROG = re.compile(r"\bid\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*\)(\.?)")


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
        self.type: MockObjClass | None = type

    def resolve(self, registered_ids):
        from esphome.config_validation import RESERVED_IDS

        if self.id is None:
            base = str(self.type).replace("::", "_").lower()
            if base == self.type:
                base = base + "_id"
            name = "".join(c for c in base if c.isalnum() or c == "_")
            used = set(registered_ids) | set(RESERVED_IDS) | CORE.loaded_integrations
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
        return cls(str(mark.name), mark.line, mark.column)

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

    def __str__(self):
        return f"{self.name}={self.value}"


class Library:
    def __init__(self, name, version, repository=None):
        self.name = name
        self.version = version
        self.repository = repository

    def __str__(self):
        return self.as_lib_dep

    @property
    def as_lib_dep(self):
        if self.repository is not None:
            if self.name is not None:
                return f"{self.name}={self.repository}"
            return self.repository

        if self.version is None:
            return self.name
        return f"{self.name}@{self.version}"

    @property
    def as_tuple(self):
        return self.name, self.version, self.repository

    def __hash__(self):
        return hash(self.as_tuple)

    def __eq__(self, other):
        if isinstance(other, Library):
            return self.as_tuple == other.as_tuple
        return NotImplemented

    def reconcile_with(self, other):
        """Merge two libraries, reconciling any conflicts."""

        if self.name != other.name:
            # Different libraries, no reconciliation possible
            raise ValueError(
                f"Cannot reconcile libraries with different names: {self.name} and {other.name}"
            )

        # repository specificity takes precedence over version specificity
        if self.repository is None and other.repository is None:
            pass  # No repositories, no conflict, continue on

        elif self.repository is None:
            # incoming library has a repository, use it
            self.repository = other.repository
            self.version = other.version
            return self

        elif other.repository is None:
            return self  # use the repository/version already present

        elif self.repository != other.repository:
            raise ValueError(
                f"Reconciliation failed! Libraries {self} and {other} requested with conflicting repositories!"
            )

        if self.version is None and other.version is None:
            return self  # Arduino library reconciled against another Arduino library, current is acceptable

        if self.version is None:
            # incoming library has a version, use it
            self.version = other.version
            return self

        if other.version is None:
            return self  # incoming library has no version, current is acceptable

            # Same versions, current library is acceptable
        if self.version != other.version:
            raise ValueError(
                f"Version pinning failed! Libraries {other} and {self} "
                "requested with conflicting versions!"
            )
        return self


# pylint: disable=too-many-public-methods
class EsphomeCore:
    def __init__(self):
        # True if command is run from dashboard
        self.dashboard = False
        # True if command is run from vscode api
        self.vscode = False
        # True if running in testing mode (disables validation checks for grouped testing)
        self.testing_mode = False
        # The name of the node
        self.name: str | None = None
        # The friendly name of the node
        self.friendly_name: str | None = None
        # The area / zone of the node
        self.area: str | None = None
        # Additional data components can store temporary data in.
        # This dict is cleared between compilation runs.
        #
        # Usage pattern (use @dataclass for type safety):
        #   DOMAIN = "my_component"
        #
        #   @dataclass
        #   class MyComponentData:
        #       feature_enabled: bool = False
        #
        #   def _get_data() -> MyComponentData:
        #       if DOMAIN not in CORE.data:
        #           CORE.data[DOMAIN] = MyComponentData()
        #       return CORE.data[DOMAIN]
        #
        # The first key should always be the component domain name (DOMAIN constant).
        self.data = {}
        # The relative path to the configuration YAML
        self.config_path: Path | None = None
        # The relative path to where all build files are stored
        self.build_path: Path | None = None
        # The validated configuration, this is None until the config has been validated
        self.config: ConfigType | None = None
        # The pending tasks in the task queue (mostly for C++ generation)
        # This is a priority queue (with heapq)
        # Each item is a tuple of form: (-priority, unique number, task)
        self.event_loop = _FakeEventLoop()
        # Task counter for pending tasks
        self.task_counter = 0
        # The variable cache, for each ID this holds a MockObj of the variable obj
        self.variables: dict[str, MockObj] = {}
        # A list of statements that go in the main setup() block
        self.main_statements: list[Statement] = []
        # A list of statements to insert in the global block (includes and global variables)
        self.global_statements: list[Statement] = []
        # A map of platformio libraries to add to the project (shortname: (name, version, repository))
        self.platformio_libraries: dict[str, Library] = {}
        # A set of build flags to set in the platformio project
        self.build_flags: set[str] = set()
        # A set of build unflags to set in the platformio project
        self.build_unflags: set[str] = set()
        # A set of defines to set for the compile process in esphome/core/defines.h
        self.defines: set[Define] = set()
        # A map of all platformio options to apply
        self.platformio_options: dict[str, str | list[str]] = {}
        # A set of strings of names of loaded integrations, used to find namespace ID conflicts
        self.loaded_integrations = set()
        # A set of strings for platform/integration combos
        self.loaded_platforms: set[str] = set()
        # A set of component IDs to track what Component subclasses are declared
        self.component_ids = set()
        # Dict to track platform entity counts for pre-allocation
        # Key: platform name (e.g. "sensor", "binary_sensor"), Value: count
        self.platform_counts: defaultdict[str, int] = defaultdict(int)
        # Track entity unique IDs to handle duplicates
        # Dict mapping (device_id, platform, sanitized_name) -> entity metadata
        self.unique_ids: dict[tuple[str, str, str], EntityMetadata] = {}
        # Whether ESPHome was started in verbose mode
        self.verbose = False
        # Whether ESPHome was started in quiet mode
        self.quiet = False
        # A list of all known ID classes
        self.id_classes = {}
        # The current component being processed during validation
        self.current_component: str | None = None
        # Address cache for DNS and mDNS lookups from command line arguments
        self.address_cache: AddressCache | None = None

    def reset(self):
        from esphome.pins import PIN_SCHEMA_REGISTRY

        self.dashboard = False
        self.name = None
        self.friendly_name = None
        self.area = None
        self.data = {}
        self.config_path = None
        self.build_path = None
        self.config = None
        self.event_loop = _FakeEventLoop()
        self.task_counter = 0
        self.variables = {}
        self.main_statements = []
        self.global_statements = []
        self.platformio_libraries = {}
        self.build_flags = set()
        self.build_unflags = set()
        self.defines = set()
        self.platformio_options = {}
        self.loaded_integrations = set()
        self.component_ids = set()
        self.platform_counts = defaultdict(int)
        self.unique_ids = {}
        self.current_component = None
        self.address_cache = None
        PIN_SCHEMA_REGISTRY.reset()

    @contextmanager
    def component_context(self, component: str):
        """Context manager to set the current component being processed."""
        old_component = self.current_component
        self.current_component = component
        try:
            yield
        finally:
            self.current_component = old_component

    @property
    def address(self) -> str | None:
        if self.config is None:
            raise ValueError("Config has not been loaded yet")

        for network_type in (CONF_WIFI, CONF_ETHERNET, CONF_OPENTHREAD):
            if network_type in self.config:
                return self.config[network_type][CONF_USE_ADDRESS]

        if CONF_OPENTHREAD in self.config:
            return f"{self.name}.local"

        return None

    @property
    def web_port(self) -> int | None:
        if self.config is None:
            raise ValueError("Config has not been loaded yet")

        if CONF_WEB_SERVER in self.config:
            try:
                return self.config[CONF_WEB_SERVER][CONF_PORT]
            except KeyError:
                return 80

        return None

    @property
    def comment(self) -> str | None:
        if self.config is None:
            raise ValueError("Config has not been loaded yet")

        if CONF_COMMENT in self.config[CONF_ESPHOME]:
            return self.config[CONF_ESPHOME][CONF_COMMENT]

        return None

    @property
    def config_dir(self) -> Path:
        if self.config_path.is_dir():
            return self.config_path.absolute()
        return self.config_path.absolute().parent

    @property
    def data_dir(self) -> Path:
        if is_ha_addon():
            return Path("/data")
        if "ESPHOME_DATA_DIR" in os.environ:
            return Path(get_str_env("ESPHOME_DATA_DIR", None))
        return self.relative_config_path(".esphome")

    @property
    def config_filename(self) -> str:
        return self.config_path.name

    def relative_config_path(self, *path: str | Path) -> Path:
        path_ = Path(*path).expanduser()
        return self.config_dir / path_

    def relative_internal_path(self, *path: str | Path) -> Path:
        path_ = Path(*path).expanduser()
        return self.data_dir / path_

    def relative_build_path(self, *path: str | Path) -> Path:
        path_ = Path(*path).expanduser()
        return self.build_path / path_

    def relative_src_path(self, *path: str | Path) -> Path:
        return self.relative_build_path("src", *path)

    def relative_pioenvs_path(self, *path: str | Path) -> Path:
        return self.relative_build_path(".pioenvs", *path)

    def relative_piolibdeps_path(self, *path: str | Path) -> Path:
        return self.relative_build_path(".piolibdeps", *path)

    @property
    def firmware_bin(self) -> Path:
        if self.is_libretiny:
            return self.relative_pioenvs_path(self.name, "firmware.uf2")
        return self.relative_pioenvs_path(self.name, "firmware.bin")

    @property
    def target_platform(self):
        return self.data[KEY_CORE][KEY_TARGET_PLATFORM]

    @property
    def is_esp8266(self):
        return self.target_platform == PLATFORM_ESP8266

    @property
    def is_esp32(self):
        return self.target_platform == PLATFORM_ESP32

    @property
    def is_rp2040(self):
        return self.target_platform == PLATFORM_RP2040

    @property
    def is_bk72xx(self):
        return self.target_platform == PLATFORM_BK72XX

    @property
    def is_rtl87xx(self):
        return self.target_platform == PLATFORM_RTL87XX

    @property
    def is_ln882x(self):
        return self.target_platform == PLATFORM_LN882X

    @property
    def is_libretiny(self):
        return self.is_bk72xx or self.is_rtl87xx or self.is_ln882x

    @property
    def is_nrf52(self):
        return self.target_platform == PLATFORM_NRF52

    @property
    def is_host(self):
        return self.target_platform == PLATFORM_HOST

    @property
    def target_framework(self):
        return self.data[KEY_CORE][KEY_TARGET_FRAMEWORK]

    @property
    def using_arduino(self):
        return self.target_framework == "arduino"

    @property
    def using_esp_idf(self):
        return self.target_framework == "esp-idf"

    @property
    def using_zephyr(self):
        return self.target_framework == "zephyr"

    def add_job(self, func, *args, **kwargs) -> None:
        self.event_loop.add_job(func, *args, **kwargs)

    def flush_tasks(self) -> None:
        try:
            self.event_loop.flush_tasks()
        except RuntimeError as e:
            raise EsphomeError(str(e)) from e

    def add(self, expression, prepend=False) -> "Statement":
        """Add an expression or statement to the main setup() block."""
        from esphome.cpp_generator import Expression, Statement, statement

        if isinstance(expression, Expression):
            expression = statement(expression)
        if not isinstance(expression, Statement):
            raise ValueError(
                f"Add '{expression}' must be expression or statement, not {type(expression)}"
            )

        if prepend:
            self.main_statements.insert(0, expression)
        else:
            self.main_statements.append(expression)
        _LOGGER.debug("Adding: %s", expression)
        return expression

    def add_global(self, expression, prepend=False) -> "Statement":
        from esphome.cpp_generator import Expression, Statement, statement

        if isinstance(expression, Expression):
            expression = statement(expression)
        if not isinstance(expression, Statement):
            raise ValueError(
                f"Add '{expression}' must be expression or statement, not {type(expression)}"
            )
        if prepend:
            self.global_statements.insert(0, expression)
        else:
            self.global_statements.append(expression)
        _LOGGER.debug("Adding global: %s", expression)
        return expression

    def add_library(self, library: Library):
        if not isinstance(library, Library):
            raise TypeError(
                f"Library {library} must be instance of Library, not {type(library)}"
            )

        if not library.name:
            raise ValueError(f"The library for {library.repository} must have a name")

        short_name = (
            library.name if "/" not in library.name else library.name.split("/")[-1]
        )

        if short_name not in self.platformio_libraries:
            _LOGGER.debug("Adding library: %s", library)
            self.platformio_libraries[short_name] = library
            return library

        self.platformio_libraries[short_name].reconcile_with(library)
        return self.platformio_libraries[short_name]

    def add_build_flag(self, build_flag: str) -> str:
        self.build_flags.add(build_flag)
        _LOGGER.debug("Adding build flag: %s", build_flag)
        return build_flag

    def add_build_unflag(self, build_unflag: str) -> None:
        self.build_unflags.add(build_unflag)
        _LOGGER.debug("Adding build unflag: %s", build_unflag)

    def add_define(self, define):
        if isinstance(define, str):
            define = Define(define)
        elif isinstance(define, Define):
            pass
        else:
            raise ValueError(
                f"Define {define} must be string or Define, not {type(define)}"
            )
        self.defines.add(define)
        _LOGGER.debug("Adding define: %s", define)
        return define

    def add_platformio_option(self, key: str, value: str | list[str]) -> None:
        new_val = value
        old_val = self.platformio_options.get(key)
        if isinstance(old_val, list):
            assert isinstance(value, list)
            new_val = old_val + value
        self.platformio_options[key] = new_val

    def _get_variable_generator(self, id):
        while True:
            try:
                return self.variables[id]
            except KeyError:
                _LOGGER.debug("Waiting for variable %s (%r)", id, id)
                yield

    async def get_variable(self, id) -> "MockObj":
        if not isinstance(id, ID):
            raise ValueError(f"ID {id!r} must be of type ID!")
        # Fast path, check if already registered without awaiting
        if id in self.variables:
            return self.variables[id]
        return await _FakeAwaitable(self._get_variable_generator(id))

    def _get_variable_with_full_id_generator(self, id):
        while True:
            if id in self.variables:
                for k, v in self.variables.items():
                    if k == id:
                        return (k, v)
            _LOGGER.debug("Waiting for variable %s", id)
            yield

    async def get_variable_with_full_id(self, id: ID) -> tuple[ID, "MockObj"]:
        if not isinstance(id, ID):
            raise ValueError(f"ID {id!r} must be of type ID!")
        return await _FakeAwaitable(self._get_variable_with_full_id_generator(id))

    def register_variable(self, id, obj):
        if id in self.variables:
            raise EsphomeError(f"ID {id} is already registered")
        _LOGGER.debug("Registered variable %s of type %s", id.id, id.type)
        self.variables[id] = obj

    def has_id(self, id):
        return id in self.variables

    def register_platform_component(self, platform_name: str, var) -> None:
        """Register a component for a platform and track its count.

        :param platform_name: The name of the platform (e.g., 'sensor', 'binary_sensor')
        :param var: The variable (component) being registered (currently unused but kept for future use)
        """
        self.platform_counts[platform_name] += 1

    def register_controller(self) -> None:
        """Track registration of a Controller for ControllerRegistry StaticVector sizing."""
        controller_count = self.data.setdefault(KEY_CONTROLLER_REGISTRY_COUNT, 0)
        self.data[KEY_CONTROLLER_REGISTRY_COUNT] = controller_count + 1

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
