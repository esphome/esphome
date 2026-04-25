from dataclasses import dataclass
import hashlib
import json
import logging
import os
from pathlib import Path
import re
import subprocess
import sys

from esphome.const import (
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_ESPHOME,
    ENV_NO_PLATFORMIO_BUILD_CACHE,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORMIO_ENV_NAME,
)
from esphome.core import CORE, EsphomeError
from esphome.helpers import get_bool_env, rmtree
from esphome.util import run_external_process

_LOGGER = logging.getLogger(__name__)

_PLATFORMIO_ENV_DEFAULTS: dict[str, str] = {}
# Bump when ESPHome changes what may be stored in the managed build cache.
_PLATFORMIO_BUILD_CACHE_VERSION = "3"


def _set_platformio_env_default(name: str, value: str) -> None:
    """Set a PlatformIO env default while preserving user-provided overrides."""
    current = os.environ.get(name)
    if current is not None and _PLATFORMIO_ENV_DEFAULTS.get(name) != current:
        return

    os.environ[name] = value
    _PLATFORMIO_ENV_DEFAULTS[name] = value


def _unset_platformio_env_default(name: str) -> None:
    """Remove a PlatformIO env value only when ESPHome set that default."""
    current = os.environ.get(name)
    if current is not None and _PLATFORMIO_ENV_DEFAULTS.get(name) == current:
        os.environ.pop(name, None)

    _PLATFORMIO_ENV_DEFAULTS.pop(name, None)


def _platformio_env_default_is_managed(name: str) -> bool:
    """Return true when ESPHome can update the variable as its own default."""
    current = os.environ.get(name)
    return current is None or _PLATFORMIO_ENV_DEFAULTS.get(name) == current


def _clean_legacy_build_cache_dir() -> None:
    """Remove pre-scoped PlatformIO build-cache shards after upgrading ESPHome."""
    build_cache_root = CORE.relative_internal_path("platformio", "build-cache")
    if not build_cache_root.is_dir():
        return

    legacy_shards = [
        child
        for child in build_cache_root.iterdir()
        if child.is_dir() and re.fullmatch(r"[0-9A-F]{2}", child.name)
    ]
    if legacy_shards:
        _LOGGER.info(
            "Deleting %d legacy PlatformIO build cache shards from %s",
            len(legacy_shards),
            build_cache_root,
        )
    for child in legacy_shards:
        _LOGGER.debug("Deleting legacy PlatformIO build cache shard %s", child)
        rmtree(child)


def _ensure_managed_build_cache_version(cache_dir: Path) -> None:
    """Drop stale managed cache scopes when ESPHome changes cache semantics."""
    marker = cache_dir.parent / f".{cache_dir.name}.version"
    current_version = None
    if marker.is_file():
        current_version = marker.read_text(encoding="utf-8").strip()

    if cache_dir.exists() and current_version != _PLATFORMIO_BUILD_CACHE_VERSION:
        _LOGGER.info("Deleting stale PlatformIO build cache scope %s", cache_dir)
        rmtree(cache_dir)

    marker.parent.mkdir(parents=True, exist_ok=True)
    cache_dir.mkdir(parents=True, exist_ok=True)
    marker.write_text(_PLATFORMIO_BUILD_CACHE_VERSION + "\n", encoding="utf-8")


def _platformio_build_cache_dir() -> Path:
    """Return a cache path shared by equivalent PlatformIO toolchains only."""
    return CORE.relative_internal_path(
        "platformio", "build-cache", _platformio_toolchain_cache_key()
    )


def _platformio_toolchain_cache_key() -> str:
    """Return the stable key used for PlatformIO cache scopes."""
    core_data = CORE.data.get(KEY_CORE, {})
    platform = core_data.get(KEY_TARGET_PLATFORM) or "unknown"
    framework = core_data.get(KEY_TARGET_FRAMEWORK) or "unknown"
    return re.sub(r"[^a-zA-Z0-9_.-]", "_", f"{platform}-{framework}")


def _normalize_platformio_values(value: str | list[str] | None) -> list[str]:
    """Return sorted PlatformIO values for stable cache fingerprints."""
    if value is None:
        return []
    values = [value] if isinstance(value, str) else value
    return sorted({x for x in values if x and x != "${common.lib_deps}"})


def _platformio_libdeps_dir() -> Path:
    """Return the default PlatformIO library dependency path.

    Embedded projects with the stable ``esphome`` environment can safely share
    downloaded library sources when their dependency list is identical. Host and
    Zephyr keep the historical per-build path because their env layout remains
    device-scoped in ESPHome.
    """
    if CORE.pioenv_name != PLATFORMIO_ENV_NAME:
        return CORE.relative_piolibdeps_path()

    lib_deps = _normalize_platformio_values(CORE.platformio_options.get("lib_deps"))
    if not lib_deps:
        lib_deps = sorted(x.as_lib_dep for x in CORE.platformio_libraries.values())

    fingerprint = hashlib.sha256(json.dumps(lib_deps).encode()).hexdigest()[:16]
    return CORE.relative_internal_path(
        "platformio", "libdeps", _platformio_toolchain_cache_key(), fingerprint
    )


def run_platformio_cli(*args, **kwargs) -> str | int:
    os.environ["PLATFORMIO_FORCE_COLOR"] = "true"
    os.environ["PLATFORMIO_BUILD_DIR"] = str(CORE.relative_pioenvs_path().absolute())
    _set_platformio_env_default(
        "PLATFORMIO_LIBDEPS_DIR", str(_platformio_libdeps_dir().absolute())
    )
    if not get_bool_env(ENV_NO_PLATFORMIO_BUILD_CACHE):
        if _platformio_env_default_is_managed("PLATFORMIO_BUILD_CACHE_DIR"):
            _clean_legacy_build_cache_dir()
            build_cache_dir = _platformio_build_cache_dir()
            _ensure_managed_build_cache_version(build_cache_dir)
        _set_platformio_env_default(
            "PLATFORMIO_BUILD_CACHE_DIR",
            str(_platformio_build_cache_dir().absolute()),
        )
    else:
        _unset_platformio_env_default("PLATFORMIO_BUILD_CACHE_DIR")
    # Suppress Python syntax warnings from third-party scripts during compilation
    os.environ.setdefault("PYTHONWARNINGS", "ignore::SyntaxWarning")
    # Increase uv retry count to handle transient network errors (default is 3)
    os.environ.setdefault("UV_HTTP_RETRIES", "10")
    cmd = [sys.executable, "-m", "esphome.platformio_runner"] + list(args)

    return run_external_process(*cmd, **kwargs)


def run_platformio_cli_run(config, verbose, *args, **kwargs) -> str | int:
    command = ["run", "-d", str(CORE.build_path)]
    if verbose:
        command += ["-v"]
    command += list(args)
    return run_platformio_cli(*command, **kwargs)


def run_compile(config, verbose):
    args = []
    if CONF_COMPILE_PROCESS_LIMIT in config[CONF_ESPHOME]:
        args += [f"-j{config[CONF_ESPHOME][CONF_COMPILE_PROCESS_LIMIT]}"]
    return run_platformio_cli_run(config, verbose, *args)


def _run_idedata(config):
    args = ["-t", "idedata"]
    stdout = run_platformio_cli_run(config, False, *args, capture_stdout=True)
    match = re.search(r'{\s*".*}', stdout)
    if match is None:
        _LOGGER.error("Could not match idedata, please report this error")
        _LOGGER.error("Stdout: %s", stdout)
        raise EsphomeError

    try:
        return json.loads(match.group())
    except ValueError:
        _LOGGER.error("Could not parse idedata", exc_info=True)
        _LOGGER.error("Stdout: %s", stdout)
        raise


def _load_idedata(config):
    platformio_ini = CORE.relative_build_path("platformio.ini")
    temp_idedata = CORE.relative_internal_path("idedata", f"{CORE.name}.json")

    changed = False
    if (
        not platformio_ini.is_file()
        or not temp_idedata.is_file()
        or platformio_ini.stat().st_mtime >= temp_idedata.stat().st_mtime
    ):
        changed = True

    if not changed:
        try:
            return json.loads(temp_idedata.read_text(encoding="utf-8"))
        except ValueError:
            pass

    temp_idedata.parent.mkdir(exist_ok=True, parents=True)

    data = _run_idedata(config)

    temp_idedata.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return data


KEY_IDEDATA = "idedata"


def get_idedata(config) -> "IDEData":
    if KEY_IDEDATA in CORE.data[KEY_CORE]:
        return CORE.data[KEY_CORE][KEY_IDEDATA]
    idedata = IDEData(_load_idedata(config))
    CORE.data[KEY_CORE][KEY_IDEDATA] = idedata
    return idedata


# ESP logs stack trace decoder, based on https://github.com/me-no-dev/EspExceptionDecoder
ESP8266_EXCEPTION_CODES = {
    0: "Illegal instruction (Is the flash damaged?)",
    1: "SYSCALL instruction",
    2: "InstructionFetchError: Processor internal physical address or data error during "
    "instruction fetch",
    3: "LoadStoreError: Processor internal physical address or data error during load or store",
    4: "Level1Interrupt: Level-1 interrupt as indicated by set level-1 bits in the INTERRUPT "
    "register",
    5: "Alloca: MOVSP instruction, if caller's registers are not in the register file",
    6: "Integer Divide By Zero",
    7: "reserved",
    8: "Privileged: Attempt to execute a privileged operation when CRING ? 0",
    9: "LoadStoreAlignmentCause: Load or store to an unaligned address",
    10: "reserved",
    11: "reserved",
    12: "InstrPIFDataError: PIF data error during instruction fetch",
    13: "LoadStorePIFDataError: Synchronous PIF data error during LoadStore access",
    14: "InstrPIFAddrError: PIF address error during instruction fetch",
    15: "LoadStorePIFAddrError: Synchronous PIF address error during LoadStore access",
    16: "InstTLBMiss: Error during Instruction TLB refill",
    17: "InstTLBMultiHit: Multiple instruction TLB entries matched",
    18: "InstFetchPrivilege: An instruction fetch referenced a virtual address at a ring level "
    "less than CRING",
    19: "reserved",
    20: "InstFetchProhibited: An instruction fetch referenced a page mapped with an attribute "
    "that does not permit instruction fetch",
    21: "reserved",
    22: "reserved",
    23: "reserved",
    24: "LoadStoreTLBMiss: Error during TLB refill for a load or store",
    25: "LoadStoreTLBMultiHit: Multiple TLB entries matched for a load or store",
    26: "LoadStorePrivilege: A load or store referenced a virtual address at a ring level less "
    "than ",
    27: "reserved",
    28: "Access to invalid address: LOAD (wild pointer?)",
    29: "Access to invalid address: STORE (wild pointer?)",
}


def _decode_pc(config, addr):
    idedata = get_idedata(config)
    if not idedata.addr2line_path or not idedata.firmware_elf_path:
        _LOGGER.debug("decode_pc no addr2line")
        return
    command = [idedata.addr2line_path, "-pfiaC", "-e", idedata.firmware_elf_path, addr]
    try:
        translation = subprocess.check_output(command, close_fds=False).decode().strip()
    except Exception:  # pylint: disable=broad-except
        _LOGGER.debug("Caught exception for command %s", command, exc_info=1)
        return

    if "?? ??:0" in translation:
        # Nothing useful
        return
    translation = translation.replace(" at ??:?", "").replace(":?", "")
    _LOGGER.warning("Decoded %s", translation)


def _parse_register(config, regex, line):
    match = regex.match(line)
    if match is not None:
        _decode_pc(config, match.group(1))


STACKTRACE_ESP8266_EXCEPTION_TYPE_RE = re.compile(r"[eE]xception \((\d+)\):")
STACKTRACE_ESP8266_PC_RE = re.compile(r"epc1=0x(4[0-9a-fA-F]{7})")
STACKTRACE_ESP8266_EXCVADDR_RE = re.compile(r"excvaddr=0x(4[0-9a-fA-F]{7})")
STACKTRACE_ESP32_PC_RE = re.compile(r".*PC\s*:\s*(?:0x)?(4[0-9a-fA-F]{7}).*")
STACKTRACE_ESP32_EXCVADDR_RE = re.compile(r"EXCVADDR\s*:\s*(?:0x)?(4[0-9a-fA-F]{7})")
STACKTRACE_ESP32_C3_PC_RE = re.compile(r"MEPC\s*:\s*(?:0x)?(4[0-9a-fA-F]{7})")
STACKTRACE_ESP32_C3_RA_RE = re.compile(r"RA\s*:\s*(?:0x)?(4[0-9a-fA-F]{7})")
STACKTRACE_BAD_ALLOC_RE = re.compile(
    r"^last failed alloc call: (4[0-9a-fA-F]{7})\((\d+)\)$"
)
STACKTRACE_ESP32_BACKTRACE_RE = re.compile(
    r"Backtrace:(?:\s*0x[0-9a-fA-F]{8}:0x[0-9a-fA-F]{8})+"
)
STACKTRACE_ESP32_BACKTRACE_PC_RE = re.compile(r"4[0-9a-f]{7}")
# ESP32 crash handler (stored backtrace from previous boot)
STACKTRACE_ESP32_CRASH_BT_RE = re.compile(r"BT\d+:\s*0x([0-9a-fA-F]{8})")
STACKTRACE_ESP8266_BACKTRACE_PC_RE = re.compile(r"4[0-9a-f]{7}")


def process_stacktrace(config, line, backtrace_state):
    line = line.strip()
    # ESP8266 Exception type
    match = re.match(STACKTRACE_ESP8266_EXCEPTION_TYPE_RE, line)
    if match is not None:
        code = int(match.group(1))
        _LOGGER.warning(
            "Exception type: %s", ESP8266_EXCEPTION_CODES.get(code, "unknown")
        )

    # ESP8266 PC/EXCVADDR
    _parse_register(config, STACKTRACE_ESP8266_PC_RE, line)
    _parse_register(config, STACKTRACE_ESP8266_EXCVADDR_RE, line)
    # ESP32 PC/EXCVADDR
    _parse_register(config, STACKTRACE_ESP32_PC_RE, line)
    _parse_register(config, STACKTRACE_ESP32_EXCVADDR_RE, line)
    # ESP32-C3 PC/RA
    _parse_register(config, STACKTRACE_ESP32_C3_PC_RE, line)
    _parse_register(config, STACKTRACE_ESP32_C3_RA_RE, line)

    # bad alloc
    match = re.match(STACKTRACE_BAD_ALLOC_RE, line)
    if match is not None:
        _LOGGER.warning(
            "Memory allocation of %s bytes failed at %s", match.group(2), match.group(1)
        )
        _decode_pc(config, match.group(1))

    # ESP32 crash handler backtrace (from previous boot)
    match = re.search(STACKTRACE_ESP32_CRASH_BT_RE, line)
    if match is not None:
        _decode_pc(config, match.group(1))

    # ESP32 single-line backtrace
    match = re.match(STACKTRACE_ESP32_BACKTRACE_RE, line)
    if match is not None:
        _LOGGER.warning("Found stack trace! Trying to decode it")
        for addr in re.finditer(STACKTRACE_ESP32_BACKTRACE_PC_RE, line):
            _decode_pc(config, addr.group())

    # ESP8266 multi-line backtrace
    if ">>>stack>>>" in line:
        # Start of backtrace
        backtrace_state = True
        _LOGGER.warning("Found stack trace! Trying to decode it")
    elif "<<<stack<<<" in line:
        # End of backtrace
        backtrace_state = False

    if backtrace_state:
        for addr in re.finditer(STACKTRACE_ESP8266_BACKTRACE_PC_RE, line):
            _decode_pc(config, addr.group())

    return backtrace_state


@dataclass
class FlashImage:
    path: Path
    offset: str


class IDEData:
    def __init__(self, raw):
        self.raw = raw

    @property
    def firmware_elf_path(self) -> Path:
        return Path(self.raw["prog_path"])

    @property
    def firmware_bin_path(self) -> Path:
        return self.firmware_elf_path.with_suffix(".bin")

    @property
    def extra_flash_images(self) -> list[FlashImage]:
        return [
            FlashImage(path=Path(entry["path"]), offset=entry["offset"])
            for entry in self.raw["extra"]["flash_images"]
        ]

    @property
    def cc_path(self) -> str:
        # For example /Users/<USER>/.platformio/packages/toolchain-xtensa32/bin/xtensa-esp32-elf-gcc
        return self.raw["cc_path"]

    @property
    def addr2line_path(self) -> str:
        # replace gcc at end with addr2line

        # Windows
        if self.cc_path.endswith(".exe"):
            return f"{self.cc_path[:-7]}addr2line.exe"

        return f"{self.cc_path[:-3]}addr2line"

    @property
    def objdump_path(self) -> str:
        # replace gcc at end with objdump
        path = self.cc_path
        return (
            f"{path[:-7]}objdump.exe"
            if path.endswith(".exe")
            else f"{path[:-3]}objdump"
        )

    @property
    def readelf_path(self) -> str:
        # replace gcc at end with readelf
        path = self.cc_path
        return (
            f"{path[:-7]}readelf.exe"
            if path.endswith(".exe")
            else f"{path[:-3]}readelf"
        )

    @property
    def defines(self) -> list[str]:
        """Return the list of preprocessor defines from idedata."""
        return self.raw.get("defines", [])
