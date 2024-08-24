from dataclasses import dataclass
import json
import logging
import os
from pathlib import Path
import re
import subprocess
from typing import Union

from esphome.const import CONF_COMPILE_PROCESS_LIMIT, CONF_ESPHOME, KEY_CORE
from esphome.core import CORE, EsphomeError
from esphome.util import run_external_command, run_external_process

_LOGGER = logging.getLogger(__name__)


def patch_structhash():
    # Patch platformio's structhash to not recompile the entire project when files are
    # removed/added. This might have unintended consequences, but this improves compile
    # times greatly when adding/removing components and a simple clean build solves
    # all issues
    from os import makedirs
    from os.path import getmtime, isdir, join

    from platformio.run import cli, helpers

    def patched_clean_build_dir(build_dir, *args):
        from platformio import fs
        from platformio.project.helpers import get_project_dir

        platformio_ini = join(get_project_dir(), "platformio.ini")

        # if project's config is modified
        if isdir(build_dir) and getmtime(platformio_ini) > getmtime(build_dir):
            fs.rmtree(build_dir)

        if not isdir(build_dir):
            makedirs(build_dir)

    helpers.clean_build_dir = patched_clean_build_dir
    cli.clean_build_dir = patched_clean_build_dir


IGNORE_LIB_WARNINGS = f"(?:{'|'.join(['Hash', 'Update'])})"
FILTER_PLATFORMIO_LINES = [
    r"Verbose mode can be enabled via `-v, --verbose` option.*",
    r"CONFIGURATION: https://docs.platformio.org/.*",
    r"DEBUG: Current.*",
    r"LDF Modes:.*",
    r"LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf.*",
    f"Looking for {IGNORE_LIB_WARNINGS} library in registry",
    f"Warning! Library `.*'{IGNORE_LIB_WARNINGS}.*` has not been found in PlatformIO Registry.",
    f"You can ignore this message, if `.*{IGNORE_LIB_WARNINGS}.*` is a built-in library.*",
    r"Scanning dependencies...",
    r"Found \d+ compatible libraries",
    r"Memory Usage -> http://bit.ly/pio-memory-usage",
    r"Found: https://platformio.org/lib/show/.*",
    r"Using cache: .*",
    r"Installing dependencies",
    r"Library Manager: Already installed, built-in library",
    r"Building in .* mode",
    r"Advanced Memory Usage is available via .*",
    r"Merged .* ELF section",
    r"esptool.py v.*",
    r"Checking size .*",
    r"Retrieving maximum program size .*",
    r"PLATFORM: .*",
    r"PACKAGES:.*",
    r" - framework-arduinoespressif.* \(.*\)",
    r" - tool-esptool.* \(.*\)",
    r" - toolchain-.* \(.*\)",
    r"Creating BIN file .*",
]


def run_platformio_cli(*args, **kwargs) -> Union[str, int]:
    os.environ["PLATFORMIO_FORCE_COLOR"] = "true"
    os.environ["PLATFORMIO_BUILD_DIR"] = os.path.abspath(CORE.relative_pioenvs_path())
    os.environ.setdefault(
        "PLATFORMIO_LIBDEPS_DIR", os.path.abspath(CORE.relative_piolibdeps_path())
    )
    cmd = ["platformio"] + list(args)

    if not CORE.verbose:
        kwargs["filter_lines"] = FILTER_PLATFORMIO_LINES

    if os.environ.get("ESPHOME_USE_SUBPROCESS") is not None:
        return run_external_process(*cmd, **kwargs)

    import platformio.__main__

    patch_structhash()
    return run_external_command(platformio.__main__.main, *cmd, **kwargs)


def run_platformio_cli_run(config, verbose, *args, **kwargs) -> Union[str, int]:
    command = ["run", "-d", CORE.build_path]
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
    platformio_ini = Path(CORE.relative_build_path("platformio.ini"))
    temp_idedata = Path(CORE.relative_internal_path("idedata", f"{CORE.name}.json"))

    changed = False
    if not platformio_ini.is_file() or not temp_idedata.is_file():
        changed = True
    elif platformio_ini.stat().st_mtime >= temp_idedata.stat().st_mtime:
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
        translation = subprocess.check_output(command).decode().strip()
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
    path: str
    offset: str


class IDEData:
    def __init__(self, raw):
        self.raw = raw

    @property
    def firmware_elf_path(self):
        return self.raw["prog_path"]

    @property
    def firmware_bin_path(self) -> str:
        return str(Path(self.firmware_elf_path).with_suffix(".bin"))

    @property
    def extra_flash_images(self) -> list[FlashImage]:
        return [
            FlashImage(path=entry["path"], offset=entry["offset"])
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
