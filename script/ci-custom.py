#!/usr/bin/env python3

import argparse
import codecs
import collections
import fnmatch
import functools
import os.path
import re
import sys
import time

import colorama
from helpers import filter_changed, git_ls_files, print_error_for_file, styled

sys.path.append(os.path.dirname(__file__))


def find_all(a_str, sub):
    if not a_str.find(sub):
        # Optimization: If str is not in whole text, then do not try
        # on each line
        return
    for i, line in enumerate(a_str.split("\n")):
        column = 0
        while True:
            column = line.find(sub, column)
            if column == -1:
                break
            yield i, column
            column += len(sub)


file_types = (
    ".h",
    ".c",
    ".cpp",
    ".tcc",
    ".yaml",
    ".yml",
    ".ini",
    ".txt",
    ".ico",
    ".svg",
    ".png",
    ".py",
    ".html",
    ".js",
    ".md",
    ".sh",
    ".css",
    ".proto",
    ".conf",
    ".cfg",
    ".woff",
    ".woff2",
    "",
)
cpp_include = ("*.h", "*.c", "*.cpp", "*.tcc")
py_include = ("*.py",)
ignore_types = (".ico", ".png", ".woff", ".woff2", "", ".ttf", ".otf")

LINT_FILE_CHECKS = []
LINT_CONTENT_CHECKS = []
LINT_POST_CHECKS = []
EXECUTABLE_BIT = {}

errors = collections.defaultdict(list)


def add_errors(fname, errs):
    if not isinstance(errs, list):
        errs = [errs]
    for err in errs:
        if err is None:
            continue
        try:
            lineno, col, msg = err
        except ValueError:
            lineno = 1
            col = 1
            msg = err
        if not isinstance(msg, str):
            raise ValueError("Error is not instance of string!")
        if not isinstance(lineno, int):
            raise ValueError("Line number is not an int!")
        if not isinstance(col, int):
            raise ValueError("Column number is not an int!")
        errors[fname].append((lineno, col, msg))


def run_check(lint_obj, fname, *args):
    include = lint_obj["include"]
    exclude = lint_obj["exclude"]
    func = lint_obj["func"]
    if include is not None:
        for incl in include:
            if fnmatch.fnmatch(fname, incl):
                break
        else:
            return None
    for excl in exclude:
        if fnmatch.fnmatch(fname, excl):
            return None
    return func(*args)


def run_checks(lints, fname, *args):
    for lint in lints:
        start = time.process_time()
        try:
            add_errors(fname, run_check(lint, fname, *args))
        except Exception:
            print(f"Check {lint['func'].__name__} on file {fname} failed:")
            raise
        duration = time.process_time() - start
        lint.setdefault("durations", []).append(duration)


def _add_check(checks, func, include=None, exclude=None):
    checks.append(
        {
            "include": include,
            "exclude": exclude or [],
            "func": func,
        }
    )


def lint_file_check(**kwargs):
    def decorator(func):
        _add_check(LINT_FILE_CHECKS, func, **kwargs)
        return func

    return decorator


def lint_content_check(**kwargs):
    def decorator(func):
        _add_check(LINT_CONTENT_CHECKS, func, **kwargs)
        return func

    return decorator


def lint_post_check(func):
    _add_check(LINT_POST_CHECKS, func)
    return func


def lint_re_check(regex, **kwargs):
    flags = kwargs.pop("flags", re.MULTILINE)
    prog = re.compile(regex, flags)
    decor = lint_content_check(**kwargs)

    def decorator(func):
        @functools.wraps(func)
        def new_func(fname, content):
            errs = []
            for match in prog.finditer(content):
                if "NOLINT" in match.group(0):
                    continue
                lineno = content.count("\n", 0, match.start()) + 1
                substr = content[: match.start()]
                col = len(substr) - substr.rfind("\n")
                err = func(fname, match)
                if err is None:
                    continue
                errs.append((lineno, col + 1, err))
            return errs

        return decor(new_func)

    return decorator


def lint_content_find_check(find, only_first=False, **kwargs):
    decor = lint_content_check(**kwargs)

    def decorator(func):
        @functools.wraps(func)
        def new_func(fname, content):
            find_ = find
            if callable(find):
                find_ = find(fname, content)
            errs = []
            for line, col in find_all(content, find_):
                err = func(fname)
                errs.append((line + 1, col + 1, err))
                if only_first:
                    break
            return errs

        return decor(new_func)

    return decorator


@lint_file_check(include=["*.ino"])
def lint_ino(fname):
    return "This file extension (.ino) is not allowed. Please use either .cpp or .h"


@lint_file_check(
    exclude=[f"*{f}" for f in file_types]
    + [
        ".clang-*",
        ".dockerignore",
        ".editorconfig",
        "*.gitignore",
        "LICENSE",
        "pylintrc",
        "MANIFEST.in",
        "docker/Dockerfile*",
        "docker/rootfs/*",
        "script/*",
    ]
)
def lint_ext_check(fname):
    return (
        "This file extension is not a registered file type. If this is an error, please "
        "update the script/ci-custom.py script."
    )


@lint_file_check(
    exclude=[
        "**.sh",
        "docker/ha-addon-rootfs/**",
        "docker/*.py",
        "script/*",
        "setup.py",
    ]
)
def lint_executable_bit(fname):
    ex = EXECUTABLE_BIT[fname]
    if ex != 100644:
        return (
            f"File has invalid executable bit {ex}. If running from a windows machine please "
            "see disabling executable bit in git."
        )
    return None


@lint_content_find_check(
    "\t",
    only_first=True,
    exclude=[
        "esphome/dashboard/static/ace.js",
        "esphome/dashboard/static/ext-searchbox.js",
    ],
)
def lint_tabs(fname):
    return "File contains tab character. Please convert tabs to spaces."


@lint_content_find_check("\r", only_first=True)
def lint_newline(fname):
    return "File contains Windows newline. Please set your editor to Unix newline mode."


@lint_content_check(exclude=["*.svg"])
def lint_end_newline(fname, content):
    if content and not content.endswith("\n"):
        return "File does not end with a newline, please add an empty line at the end of the file."
    return None


CPP_RE_EOL = r".*?(?://.*?)?$"
PY_RE_EOL = r".*?(?:#.*?)?$"


def highlight(s):
    return f"\033[36m{s}\033[0m"


@lint_re_check(
    r"^#define\s+([a-zA-Z0-9_]+)\s+(0b[10]+|0x[0-9a-fA-F]+|\d+)\s*?(?:\/\/.*?)?$",
    include=cpp_include,
    exclude=[
        "esphome/core/log.h",
        "esphome/components/socket/headers.h",
        "esphome/core/defines.h",
    ],
)
def lint_no_defines(fname, match):
    s = highlight(f"static const uint8_t {match.group(1)} = {match.group(2)};")
    return (
        "#define macros for integer constants are not allowed, please use "
        f"{s} style instead (replace uint8_t with the appropriate "
        "datatype). See also Google style guide."
    )


@lint_re_check(r"^\s*delay\((\d+)\);" + CPP_RE_EOL, include=cpp_include)
def lint_no_long_delays(fname, match):
    duration_ms = int(match.group(1))
    if duration_ms < 50:
        return None
    return (
        f"{highlight(match.group(0).strip())} - long calls to delay() are not allowed "
        "in ESPHome because everything executes in one thread. Calling delay() will "
        "block the main thread and slow down ESPHome.\n"
        "If there's no way to work around the delay() and it doesn't execute often, please add "
        "a '// NOLINT' comment to the line."
    )


@lint_content_check(include=["esphome/const.py"])
def lint_const_ordered(fname, content):
    """Lint that value in const.py are ordered.

    Reason: Otherwise people add it to the end, and then that results in merge conflicts.
    """
    lines = content.splitlines()
    errs = []
    for start in ["CONF_", "ICON_", "UNIT_"]:
        matching = [
            (i + 1, line) for i, line in enumerate(lines) if line.startswith(start)
        ]
        ordered = list(sorted(matching, key=lambda x: x[1].replace("_", " ")))
        ordered = [(mi, ol) for (mi, _), (_, ol) in zip(matching, ordered)]
        for (mi, mline), (_, ol) in zip(matching, ordered):
            if mline == ol:
                continue
            target = next(i for i, line in ordered if line == mline)
            target_text = next(line for i, line in matching if target == i)
            errs.append(
                (
                    mi,
                    1,
                    f"Constant {highlight(mline)} is not ordered, please make sure all "
                    f"constants are ordered. See line {mi} (should go to line {target}, "
                    f"{target_text})",
                )
            )
    return errs


@lint_re_check(r'^\s*CONF_([A-Z_0-9a-z]+)\s+=\s+[\'"](.*?)[\'"]\s*?$', include=["*.py"])
def lint_conf_matches(fname, match):
    const = match.group(1)
    value = match.group(2)
    const_norm = const.lower()
    value_norm = value.replace(".", "_")
    if const_norm == value_norm:
        return None
    return (
        f"Constant {highlight('CONF_' + const)} does not match value {highlight(value)}! "
        "Please make sure the constant's name matches its value!"
    )


CONF_RE = r'^(CONF_[a-zA-Z0-9_]+)\s*=\s*[\'"].*?[\'"]\s*?$'
with codecs.open("esphome/const.py", "r", encoding="utf-8") as const_f_handle:
    constants_content = const_f_handle.read()
CONSTANTS = [m.group(1) for m in re.finditer(CONF_RE, constants_content, re.MULTILINE)]

CONSTANTS_USES = collections.defaultdict(list)


@lint_re_check(CONF_RE, include=["*.py"], exclude=["esphome/const.py"])
def lint_conf_from_const_py(fname, match):
    name = match.group(1)
    if name not in CONSTANTS:
        CONSTANTS_USES[name].append(fname)
        return None
    return (
        f"Constant {highlight(name)} has already been defined in const.py - "
        "please import the constant from const.py directly."
    )


RAW_PIN_ACCESS_RE = (
    r"^\s(pinMode|digitalWrite|digitalRead)\((.*)->get_pin\(\),\s*([^)]+).*\)"
)


@lint_re_check(RAW_PIN_ACCESS_RE, include=cpp_include)
def lint_no_raw_pin_access(fname, match):
    func = match.group(1)
    pin = match.group(2)
    mode = match.group(3)
    new_func = {
        "pinMode": "pin_mode",
        "digitalWrite": "digital_write",
        "digitalRead": "digital_read",
    }[func]
    new_code = highlight(f"{pin}->{new_func}({mode})")
    return f"Don't use raw {func} calls. Instead, use the `->{new_func}` function: {new_code}"


# Functions from Arduino framework that are forbidden to use directly
ARDUINO_FORBIDDEN = [
    "digitalWrite",
    "digitalRead",
    "pinMode",
    "shiftOut",
    "shiftIn",
    "radians",
    "degrees",
    "interrupts",
    "noInterrupts",
    "lowByte",
    "highByte",
    "bitRead",
    "bitSet",
    "bitClear",
    "bitWrite",
    "bit",
    "analogRead",
    "analogWrite",
    "pulseIn",
    "pulseInLong",
    "tone",
]
ARDUINO_FORBIDDEN_RE = r"[^\w\d](" + r"|".join(ARDUINO_FORBIDDEN) + r")\(.*"


@lint_re_check(
    ARDUINO_FORBIDDEN_RE,
    include=cpp_include,
    exclude=[
        "esphome/components/mqtt/custom_mqtt_device.h",
        "esphome/components/sun/sun.cpp",
    ],
)
def lint_no_arduino_framework_functions(fname, match):
    nolint = highlight("// NOLINT")
    return (
        f"The function {highlight(match.group(1))} from the Arduino framework is forbidden to be "
        f"used directly in the ESPHome codebase. Please use ESPHome's abstractions and equivalent "
        f"C++ instead.\n"
        f"\n"
        f"(If the function is strictly necessary, please add `{nolint}` to the end of the line)"
    )


IDF_CONVERSION_FORBIDDEN = {
    "ARDUINO_ARCH_ESP32": "USE_ESP32",
    "ARDUINO_ARCH_ESP8266": "USE_ESP8266",
    "pgm_read_byte": "progmem_read_byte",
    "ICACHE_RAM_ATTR": "IRAM_ATTR",
    "esphome/core/esphal.h": "esphome/core/hal.h",
}
IDF_CONVERSION_FORBIDDEN_RE = r"(" + r"|".join(IDF_CONVERSION_FORBIDDEN) + r").*"


@lint_re_check(
    IDF_CONVERSION_FORBIDDEN_RE,
    include=cpp_include,
)
def lint_no_removed_in_idf_conversions(fname, match):
    replacement = IDF_CONVERSION_FORBIDDEN[match.group(1)]
    return (
        f"The macro {highlight(match.group(1))} can no longer be used in ESPHome directly. "
        f"Please use {highlight(replacement)} instead."
    )


@lint_re_check(
    r"[^\w\d]byte +[\w\d]+\s*=",
    include=cpp_include,
    exclude={
        "esphome/components/tuya/tuya.h",
    },
)
def lint_no_byte_datatype(fname, match):
    return (
        f"The datatype {highlight('byte')} is not allowed to be used in ESPHome. "
        f"Please use {highlight('uint8_t')} instead."
    )


@lint_post_check
def lint_constants_usage():
    errs = []
    for constant, uses in CONSTANTS_USES.items():
        if len(uses) < 4:
            continue
        errs.append(
            f"Constant {highlight(constant)} is defined in {len(uses)} files. Please move all definitions of the "
            f"constant to const.py (Uses: {', '.join(uses)})"
        )
    return errs


def relative_cpp_search_text(fname, content):
    parts = fname.split("/")
    integration = parts[2]
    return f'#include "esphome/components/{integration}'


@lint_content_find_check(relative_cpp_search_text, include=["esphome/components/*.cpp"])
def lint_relative_cpp_import(fname):
    return (
        "Component contains absolute import - Components must always use "
        "relative imports.\n"
        "Change:\n"
        '  #include "esphome/components/abc/abc.h"\n'
        "to:\n"
        '  #include "abc.h"\n\n'
    )


def relative_py_search_text(fname, content):
    parts = fname.split("/")
    integration = parts[2]
    return f"esphome.components.{integration}"


@lint_content_find_check(
    relative_py_search_text,
    include=["esphome/components/*.py"],
    exclude=[
        "esphome/components/libretiny/generate_components.py",
        "esphome/components/web_server/__init__.py",
    ],
)
def lint_relative_py_import(fname):
    return (
        "Component contains absolute import - Components must always use "
        "relative imports within the integration.\n"
        "Change:\n"
        '  from esphome.components.abc import abc_ns"\n'
        "to:\n"
        "  from . import abc_ns\n\n"
    )


@lint_content_check(
    include=[
        "esphome/components/*.h",
        "esphome/components/*.cpp",
        "esphome/components/*.tcc",
    ],
    exclude=[
        "esphome/components/socket/headers.h",
        "esphome/components/esp32/core.cpp",
        "esphome/components/esp8266/core.cpp",
        "esphome/components/rp2040/core.cpp",
        "esphome/components/libretiny/core.cpp",
        "esphome/components/host/core.cpp",
    ],
)
def lint_namespace(fname, content):
    expected_name = re.match(
        r"^esphome/components/([^/]+)/.*", fname.replace(os.path.sep, "/")
    ).group(1)
    search = f"namespace {expected_name}"
    if search in content:
        return None
    return (
        "Invalid namespace found in C++ file. All integration C++ files should put all "
        "functions in a separate namespace that matches the integration's name. "
        f"Please make sure the file contains {highlight(search)}"
    )


@lint_content_find_check('"esphome.h"', include=cpp_include, exclude=["tests/custom.h"])
def lint_esphome_h(fname):
    return (
        "File contains reference to 'esphome.h' - This file is "
        "auto-generated and should only be used for *custom* "
        "components. Please replace with references to the direct files."
    )


@lint_content_check(include=["*.h"])
def lint_pragma_once(fname, content):
    if "#pragma once" not in content:
        return (
            "Header file contains no 'pragma once' header guard. Please add a "
            "'#pragma once' line at the top of the file."
        )
    return None


def lint_inclusive_language(fname, match):
    # From https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=49decddd39e5f6132ccd7d9fdc3d7c470b0061bb
    return (
        "Avoid the use of whitelist/blacklist/slave.\n"
        "Recommended replacements for 'master / slave' are:\n"
        "    '{primary,main} / {secondary,replica,subordinate}\n"
        "    '{initiator,requester} / {target,responder}'\n"
        "    '{controller,host} / {device,worker,proxy}'\n"
        "    'leader / follower'\n"
        "    'director / performer'\n"
        "\n"
        "Recommended replacements for 'blacklist/whitelist' are:\n"
        "    'denylist / allowlist'\n"
        "    'blocklist / passlist'"
    )


lint_re_check(
    r"(whitelist|blacklist|slave)" + PY_RE_EOL,
    include=py_include,
    exclude=["script/ci-custom.py"],
    flags=re.IGNORECASE | re.MULTILINE,
)(lint_inclusive_language)


lint_re_check(
    r"(whitelist|blacklist|slave)" + CPP_RE_EOL,
    include=cpp_include,
    flags=re.IGNORECASE | re.MULTILINE,
)(lint_inclusive_language)


@lint_re_check(r"[\t\r\f\v ]+$")
def lint_trailing_whitespace(fname, match):
    return "Trailing whitespace detected"


@lint_content_find_check(
    "ESP_LOG",
    include=["*.h", "*.tcc"],
    exclude=[
        "esphome/components/binary_sensor/binary_sensor.h",
        "esphome/components/button/button.h",
        "esphome/components/climate/climate.h",
        "esphome/components/cover/cover.h",
        "esphome/components/datetime/date_entity.h",
        "esphome/components/datetime/time_entity.h",
        "esphome/components/datetime/datetime_entity.h",
        "esphome/components/display/display.h",
        "esphome/components/event/event.h",
        "esphome/components/fan/fan.h",
        "esphome/components/i2c/i2c.h",
        "esphome/components/lock/lock.h",
        "esphome/components/mqtt/mqtt_component.h",
        "esphome/components/number/number.h",
        "esphome/components/text/text.h",
        "esphome/components/output/binary_output.h",
        "esphome/components/output/float_output.h",
        "esphome/components/nextion/nextion_base.h",
        "esphome/components/select/select.h",
        "esphome/components/sensor/sensor.h",
        "esphome/components/stepper/stepper.h",
        "esphome/components/switch/switch.h",
        "esphome/components/text_sensor/text_sensor.h",
        "esphome/components/valve/valve.h",
        "esphome/core/component.h",
        "esphome/core/gpio.h",
        "esphome/core/log.h",
        "tests/custom.h",
    ],
)
def lint_log_in_header(fname):
    return (
        "Found reference to ESP_LOG in header file. Using ESP_LOG* in header files "
        "is currently not possible - please move the definition to a source file (.cpp)"
    )


def main():
    colorama.init()

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "files", nargs="*", default=[], help="files to be processed (regex on path)"
    )
    parser.add_argument(
        "-c", "--changed", action="store_true", help="Only run on changed files"
    )
    parser.add_argument(
        "--print-slowest", action="store_true", help="Print the slowest checks"
    )
    args = parser.parse_args()

    global EXECUTABLE_BIT
    EXECUTABLE_BIT = git_ls_files()
    files = list(EXECUTABLE_BIT.keys())
    # Match against re
    file_name_re = re.compile("|".join(args.files))
    files = [p for p in files if file_name_re.search(p)]

    if args.changed:
        files = filter_changed(files)

    files.sort()

    for fname in files:
        _, ext = os.path.splitext(fname)
        run_checks(LINT_FILE_CHECKS, fname, fname)
        if ext in ignore_types:
            continue
        try:
            with codecs.open(fname, "r", encoding="utf-8") as f_handle:
                content = f_handle.read()
        except UnicodeDecodeError:
            add_errors(
                fname,
                "File is not readable as UTF-8. Please set your editor to UTF-8 mode.",
            )
            continue
        run_checks(LINT_CONTENT_CHECKS, fname, fname, content)

    run_checks(LINT_POST_CHECKS, "POST")

    for f, errs in sorted(errors.items()):
        bold = functools.partial(styled, colorama.Style.BRIGHT)
        bold_red = functools.partial(styled, (colorama.Style.BRIGHT, colorama.Fore.RED))
        err_str = (
            f"{bold(f'{f}:{lineno}:{col}:')} {bold_red('lint:')} {msg}\n"
            for lineno, col, msg in errs
        )
        print_error_for_file(f, "\n".join(err_str))

    if args.print_slowest:
        lint_times = []
        for lint in LINT_FILE_CHECKS + LINT_CONTENT_CHECKS + LINT_POST_CHECKS:
            durations = lint.get("durations", [])
            lint_times.append((sum(durations), len(durations), lint["func"].__name__))
        lint_times.sort(key=lambda x: -x[0])
        for i in range(min(len(lint_times), 10)):
            dur, invocations, name = lint_times[i]
            print(f" - '{name}' took {dur:.2f}s total (ran on {invocations} files)")
        print(f"Total time measured: {sum(x[0] for x in lint_times):.2f}s")

    return len(errors)


if __name__ == "__main__":
    sys.exit(main())
