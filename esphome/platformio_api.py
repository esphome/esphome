from __future__ import print_function

import json
import logging
import os
import re
import subprocess

from esphome.core import CORE
from esphome.py_compat import decode_text
from esphome.util import run_external_command, run_external_process

_LOGGER = logging.getLogger(__name__)


def patch_structhash():
    # Patch platformio's structhash to not recompile the entire project when files are
    # removed/added. This might have unintended consequences, but this improves compile
    # times greatly when adding/removing components and a simple clean build solves
    # all issues
    from platformio.commands.run import helpers, command
    from os.path import join, isdir, getmtime
    from os import makedirs

    def patched_clean_build_dir(build_dir, *args):
        from platformio import util
        from platformio.project.helpers import get_project_dir
        platformio_ini = join(get_project_dir(), "platformio.ini")

        # if project's config is modified
        if isdir(build_dir) and getmtime(platformio_ini) > getmtime(build_dir):
            util.rmtree_(build_dir)

        if not isdir(build_dir):
            makedirs(build_dir)

    # pylint: disable=protected-access
    helpers.clean_build_dir = patched_clean_build_dir
    command.clean_build_dir = patched_clean_build_dir


IGNORE_LIB_WARNINGS = r'(?:' + '|'.join(['Hash', 'Update']) + r')'
FILTER_PLATFORMIO_LINES = [
    r'Verbose mode can be enabled via `-v, --verbose` option.*',
    r'CONFIGURATION: https://docs.platformio.org/.*',
    r'PLATFORM: .*',
    r'DEBUG: Current.*',
    r'PACKAGES: .*',
    r'LDF: Library Dependency Finder -> http://bit.ly/configure-pio-ldf.*',
    r'LDF Modes: Finder ~ chain, Compatibility ~ soft.*',
    r'Looking for ' + IGNORE_LIB_WARNINGS + r' library in registry',
    r"Warning! Library `.*'" + IGNORE_LIB_WARNINGS +
    r".*` has not been found in PlatformIO Registry.",
    r"You can ignore this message, if `.*" + IGNORE_LIB_WARNINGS + r".*` is a built-in library.*",
    r'Scanning dependencies...',
    r"Found \d+ compatible libraries",
    r'Memory Usage -> http://bit.ly/pio-memory-usage',
    r'esptool.py v.*',
    r"Found: https://platformio.org/lib/show/.*",
    r"Using cache: .*",
    r'Installing dependencies',
    r'.* @ .* is already installed',
]


def run_platformio_cli(*args, **kwargs):
    os.environ["PLATFORMIO_FORCE_COLOR"] = "true"
    os.environ["PLATFORMIO_BUILD_DIR"] = os.path.abspath(CORE.relative_pioenvs_path())
    os.environ["PLATFORMIO_LIBDEPS_DIR"] = os.path.abspath(CORE.relative_piolibdeps_path())
    cmd = ['platformio'] + list(args)

    if not CORE.verbose:
        kwargs['filter_lines'] = FILTER_PLATFORMIO_LINES

    if os.environ.get('ESPHOME_USE_SUBPROCESS') is not None:
        return run_external_process(*cmd, **kwargs)

    import platformio.__main__
    patch_structhash()
    return run_external_command(platformio.__main__.main,
                                *cmd, **kwargs)


def run_platformio_cli_run(config, verbose, *args, **kwargs):
    command = ['run', '-d', CORE.build_path]
    if verbose:
        command += ['-v']
    command += list(args)
    return run_platformio_cli(*command, **kwargs)


def run_compile(config, verbose):
    return run_platformio_cli_run(config, verbose)


def run_upload(config, verbose, port):
    return run_platformio_cli_run(config, verbose, '-t', 'upload', '--upload-port', port)


def run_idedata(config):
    args = ['-t', 'idedata']
    stdout = run_platformio_cli_run(config, False, *args, capture_stdout=True)
    stdout = decode_text(stdout)
    match = re.search(r'{.*}', stdout)
    if match is None:
        return IDEData(None)
    try:
        return IDEData(json.loads(match.group()))
    except ValueError:
        return IDEData(None)


IDE_DATA = None


def get_idedata(config):
    global IDE_DATA

    if IDE_DATA is None:
        _LOGGER.info("Need to fetch platformio IDE-data, please stand by")
        IDE_DATA = run_idedata(config)
    return IDE_DATA


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
        return
    command = [idedata.addr2line_path, '-pfiaC', '-e', idedata.firmware_elf_path, addr]
    try:
        translation = subprocess.check_output(command).strip()
    except Exception:  # pylint: disable=broad-except
        return

    if "?? ??:0" in translation:
        # Nothing useful
        return
    translation = translation.replace(' at ??:?', '').replace(':?', '')
    _LOGGER.warning("Decoded %s", translation)


def _parse_register(config, regex, line):
    match = regex.match(line)
    if match is not None:
        _decode_pc(config, match.group(1))


STACKTRACE_ESP8266_EXCEPTION_TYPE_RE = re.compile(r'[eE]xception \((\d+)\):')
STACKTRACE_ESP8266_PC_RE = re.compile(r'epc1=0x(4[0-9a-fA-F]{7})')
STACKTRACE_ESP8266_EXCVADDR_RE = re.compile(r'excvaddr=0x(4[0-9a-fA-F]{7})')
STACKTRACE_ESP32_PC_RE = re.compile(r'PC\s*:\s*(?:0x)?(4[0-9a-fA-F]{7})')
STACKTRACE_ESP32_EXCVADDR_RE = re.compile(r'EXCVADDR\s*:\s*(?:0x)?(4[0-9a-fA-F]{7})')
STACKTRACE_BAD_ALLOC_RE = re.compile(r'^last failed alloc call: (4[0-9a-fA-F]{7})\((\d+)\)$')
STACKTRACE_ESP32_BACKTRACE_RE = re.compile(r'Backtrace:(?:\s+0x[0-9a-fA-F]{8}:0x[0-9a-fA-F]{8})+')
STACKTRACE_ESP32_BACKTRACE_PC_RE = re.compile(r'4[0-9a-f]{7}')
STACKTRACE_ESP8266_BACKTRACE_PC_RE = re.compile(r'4[0-9a-f]{7}')


def process_stacktrace(config, line, backtrace_state):
    line = line.strip()
    # ESP8266 Exception type
    match = re.match(STACKTRACE_ESP8266_EXCEPTION_TYPE_RE, line)
    if match is not None:
        code = match.group(1)
        _LOGGER.warning("Exception type: %s", ESP8266_EXCEPTION_CODES.get(code, 'unknown'))

    # ESP8266 PC/EXCVADDR
    _parse_register(config, STACKTRACE_ESP8266_PC_RE, line)
    _parse_register(config, STACKTRACE_ESP8266_EXCVADDR_RE, line)
    # ESP32 PC/EXCVADDR
    _parse_register(config, STACKTRACE_ESP32_PC_RE, line)
    _parse_register(config, STACKTRACE_ESP32_EXCVADDR_RE, line)

    # bad alloc
    match = re.match(STACKTRACE_BAD_ALLOC_RE, line)
    if match is not None:
        _LOGGER.warning("Memory allocation of %s bytes failed at %s",
                        match.group(2), match.group(1))
        _decode_pc(config, match.group(1))

    # ESP32 single-line backtrace
    match = re.match(STACKTRACE_ESP32_BACKTRACE_RE, line)
    if match is not None:
        _LOGGER.warning("Found stack trace! Trying to decode it")
        for addr in re.finditer(STACKTRACE_ESP32_BACKTRACE_PC_RE, line):
            _decode_pc(config, addr.group())

    # ESP8266 multi-line backtrace
    if '>>>stack>>>' in line:
        # Start of backtrace
        backtrace_state = True
        _LOGGER.warning("Found stack trace! Trying to decode it")
    elif '<<<stack<<<' in line:
        # End of backtrace
        backtrace_state = False

    if backtrace_state:
        for addr in re.finditer(STACKTRACE_ESP8266_BACKTRACE_PC_RE, line):
            _decode_pc(config, addr.group())

    return backtrace_state


class IDEData(object):
    def __init__(self, raw):
        if not isinstance(raw, dict):
            self.raw = {}
        else:
            self.raw = raw

    @property
    def firmware_elf_path(self):
        return self.raw.get("prog_path")

    @property
    def flash_extra_images(self):
        return [
            (x['path'], x['offset']) for x in self.raw.get("flash_extra_images", [])
        ]

    @property
    def cc_path(self):
        # For example /Users/<USER>/.platformio/packages/toolchain-xtensa32/bin/xtensa-esp32-elf-gcc
        return self.raw.get("cc_path")

    @property
    def addr2line_path(self):
        cc_path = self.cc_path
        if cc_path is None:
            return None
        # replace gcc at end with addr2line
        return cc_path[:-3] + 'addr2line'
