#!/usr/bin/env python
from __future__ import print_function

import codecs
import collections
import fnmatch
import os.path
import re
import subprocess
import sys


def find_all(a_str, sub):
    for i, line in enumerate(a_str.splitlines()):
        column = 0
        while True:
            column = line.find(sub, column)
            if column == -1:
                break
            yield i, column
            column += len(sub)


command = ['git', 'ls-files', '-s']
proc = subprocess.Popen(command, stdout=subprocess.PIPE)
output, err = proc.communicate()
lines = [x.split() for x in output.decode('utf-8').splitlines()]
EXECUTABLE_BIT = {
    s[3].strip(): int(s[0]) for s in lines
}
files = [s[3].strip() for s in lines]
files = list(filter(os.path.exists, files))
files.sort()

file_types = ('.h', '.c', '.cpp', '.tcc', '.yaml', '.yml', '.ini', '.txt', '.ico', '.svg',
              '.py', '.html', '.js', '.md', '.sh', '.css', '.proto', '.conf', '.cfg',
              '.woff', '.woff2', '')
cpp_include = ('*.h', '*.c', '*.cpp', '*.tcc')
ignore_types = ('.ico', '.woff', '.woff2', '')

LINT_FILE_CHECKS = []
LINT_CONTENT_CHECKS = []
LINT_POST_CHECKS = []


def run_check(lint_obj, fname, *args):
    include = lint_obj['include']
    exclude = lint_obj['exclude']
    func = lint_obj['func']
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
        add_errors(fname, run_check(lint, fname, *args))


def _add_check(checks, func, include=None, exclude=None):
    checks.append({
        'include': include,
        'exclude': exclude or [],
        'func': func,
    })


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
    prog = re.compile(regex, re.MULTILINE)
    decor = lint_content_check(**kwargs)

    def decorator(func):
        def new_func(fname, content):
            errors = []
            for match in prog.finditer(content):
                if 'NOLINT' in match.group(0):
                    continue
                lineno = content.count("\n", 0, match.start()) + 1
                err = func(fname, match)
                if err is None:
                    continue
                errors.append("{} See line {}.".format(err, lineno))
            return errors
        return decor(new_func)
    return decorator


def lint_content_find_check(find, **kwargs):
    decor = lint_content_check(**kwargs)

    def decorator(func):
        def new_func(fname, content):
            find_ = find
            if callable(find):
                find_ = find(fname, content)
            errors = []
            for line, col in find_all(content, find_):
                err = func(fname)
                errors.append("{err} See line {line}:{col}."
                              "".format(err=err, line=line+1, col=col+1))
            return errors
        return decor(new_func)
    return decorator


@lint_file_check(include=['*.ino'])
def lint_ino(fname):
    return "This file extension (.ino) is not allowed. Please use either .cpp or .h"


@lint_file_check(exclude=['*{}'.format(f) for f in file_types] + [
    '.clang-*', '.dockerignore', '.editorconfig', '*.gitignore', 'LICENSE', 'pylintrc',
    'MANIFEST.in', 'docker/Dockerfile*', 'docker/rootfs/*', 'script/*',
])
def lint_ext_check(fname):
    return "This file extension is not a registered file type. If this is an error, please " \
           "update the script/ci-custom.py script."


@lint_file_check(exclude=[
    'docker/rootfs/*', 'script/*', 'setup.py'
])
def lint_executable_bit(fname):
    ex = EXECUTABLE_BIT[fname]
    if ex != 100644:
        return 'File has invalid executable bit {}. If running from a windows machine please ' \
               'see disabling executable bit in git.'.format(ex)
    return None


@lint_content_find_check('\t', exclude=[
    'esphome/dashboard/static/ace.js', 'esphome/dashboard/static/ext-searchbox.js',
])
def lint_tabs(fname):
    return "File contains tab character. Please convert tabs to spaces."


@lint_content_find_check('\r')
def lint_newline(fname):
    return "File contains windows newline. Please set your editor to unix newline mode."


@lint_content_check(exclude=['*.svg'])
def lint_end_newline(fname, content):
    if content and not content.endswith('\n'):
        return "File does not end with a newline, please add an empty line at the end of the file."
    return None


CPP_RE_EOL = r'\s*?(?://.*?)?$'


def highlight(s):
    return '\033[36m{}\033[0m'.format(s)


@lint_re_check(r'^#define\s+([a-zA-Z0-9_]+)\s+([0-9bx]+)' + CPP_RE_EOL,
               include=cpp_include, exclude=['esphome/core/log.h'])
def lint_no_defines(fname, match):
    s = highlight('static const uint8_t {} = {};'.format(match.group(1), match.group(2)))
    return ("#define macros for integer constants are not allowed, please use "
            "{} style instead (replace uint8_t with the appropriate "
            "datatype). See also Google style guide.".format(s))


@lint_re_check(r'^\s*delay\((\d+)\);' + CPP_RE_EOL, include=cpp_include)
def lint_no_long_delays(fname, match):
    duration_ms = int(match.group(1))
    if duration_ms < 50:
        return None
    return (
        "{} - long calls to delay() are not allowed in ESPHome because everything executes "
        "in one thread. Calling delay() will block the main thread and slow down ESPHome.\n"
        "If there's no way to work around the delay() and it doesn't execute often, please add "
        "a '// NOLINT' comment to the line."
        "".format(highlight(match.group(0).strip()))
    )


@lint_content_check(include=['esphome/const.py'])
def lint_const_ordered(fname, content):
    lines = content.splitlines()
    errors = []
    for start in ['CONF_', 'ICON_', 'UNIT_']:
        matching = [(i+1, line) for i, line in enumerate(lines) if line.startswith(start)]
        ordered = list(sorted(matching, key=lambda x: x[1].replace('_', ' ')))
        ordered = [(mi, ol) for (mi, _), (_, ol) in zip(matching, ordered)]
        for (mi, ml), (oi, ol) in zip(matching, ordered):
            if ml == ol:
                continue
            target = next(i for i, l in ordered if l == ml)
            target_text = next(l for i, l in matching if target == i)
            errors.append("Constant {} is not ordered, please make sure all constants are ordered. "
                          "See line {} (should go to line {}, {})"
                          "".format(highlight(ml), mi, target, target_text))
    return errors


@lint_re_check(r'^\s*CONF_([A-Z_0-9a-z]+)\s+=\s+[\'"](.*?)[\'"]\s*?$', include=['*.py'])
def lint_conf_matches(fname, match):
    const = match.group(1)
    value = match.group(2)
    const_norm = const.lower()
    value_norm = value.replace('.', '_')
    if const_norm == value_norm:
        return None
    return ("Constant {} does not match value {}! Please make sure the constant's name matches its "
            "value!"
            "".format(highlight('CONF_' + const), highlight(value)))


CONF_RE = r'^(CONF_[a-zA-Z0-9_]+)\s*=\s*[\'"].*?[\'"]\s*?$'
with codecs.open('esphome/const.py', 'r', encoding='utf-8') as f_handle:
    constants_content = f_handle.read()
CONSTANTS = [m.group(1) for m in re.finditer(CONF_RE, constants_content, re.MULTILINE)]

CONSTANTS_USES = collections.defaultdict(list)


@lint_re_check(CONF_RE, include=['*.py'], exclude=['esphome/const.py'])
def lint_conf_from_const_py(fname, match):
    name = match.group(1)
    if name not in CONSTANTS:
        CONSTANTS_USES[name].append(fname)
        return None
    return ("Constant {} has already been defined in const.py - please import the constant from "
            "const.py directly.".format(highlight(name)))


@lint_post_check
def lint_constants_usage():
    errors = []
    for constant, uses in CONSTANTS_USES.items():
        if len(uses) < 4:
            continue
        errors.append("Constant {} is defined in {} files. Please move all definitions of the "
                      "constant to const.py (Uses: {})"
                      "".format(highlight(constant), len(uses), ', '.join(uses)))
    return errors


def relative_cpp_search_text(fname, content):
    parts = fname.split('/')
    integration = parts[2]
    return '#include "esphome/components/{}'.format(integration)


@lint_content_find_check(relative_cpp_search_text, include=['esphome/components/*.cpp'])
def lint_relative_cpp_import(fname):
    return ("Component contains absolute import - Components must always use "
            "relative imports.\n"
            "Change:\n"
            '  #include "esphome/components/abc/abc.h"\n'
            'to:\n'
            '  #include "abc.h"\n\n')


def relative_py_search_text(fname, content):
    parts = fname.split('/')
    integration = parts[2]
    return 'esphome.components.{}'.format(integration)


@lint_content_find_check(relative_py_search_text, include=['esphome/components/*.py'],
                         exclude=['esphome/components/web_server/__init__.py'])
def lint_relative_py_import(fname):
    return ("Component contains absolute import - Components must always use "
            "relative imports within the integration.\n"
            "Change:\n"
            '  from esphome.components.abc import abc_ns"\n'
            'to:\n'
            '  from . import abc_ns\n\n')


@lint_content_find_check('"esphome.h"', include=cpp_include, exclude=['tests/custom.h'])
def lint_esphome_h(fname):
    return ("File contains reference to 'esphome.h' - This file is "
            "auto-generated and should only be used for *custom* "
            "components. Please replace with references to the direct files.")


@lint_content_check(include=['*.h'])
def lint_pragma_once(fname, content):
    if '#pragma once' not in content:
        return ("Header file contains no 'pragma once' header guard. Please add a "
                "'#pragma once' line at the top of the file.")
    return None


@lint_content_find_check('ESP_LOG', include=['*.h', '*.tcc'], exclude=[
    'esphome/components/binary_sensor/binary_sensor.h',
    'esphome/components/cover/cover.h',
    'esphome/components/display/display_buffer.h',
    'esphome/components/i2c/i2c.h',
    'esphome/components/mqtt/mqtt_component.h',
    'esphome/components/output/binary_output.h',
    'esphome/components/output/float_output.h',
    'esphome/components/sensor/sensor.h',
    'esphome/components/stepper/stepper.h',
    'esphome/components/switch/switch.h',
    'esphome/components/text_sensor/text_sensor.h',
    'esphome/components/climate/climate.h',
    'esphome/core/component.h',
    'esphome/core/esphal.h',
    'esphome/core/log.h',
    'tests/custom.h',
])
def lint_log_in_header(fname):
    return ('Found reference to ESP_LOG in header file. Using ESP_LOG* in header files '
            'is currently not possible - please move the definition to a source file (.cpp)')


errors = collections.defaultdict(list)


def add_errors(fname, errs):
    if not isinstance(errs, list):
        errs = [errs]
    errs = [x for x in errs if x is not None]
    for err in errs:
        if not isinstance(err, str):
            raise ValueError("Error is not instance of string!")
    if not errs:
        return
    errors[fname].extend(errs)


for fname in files:
    _, ext = os.path.splitext(fname)
    run_checks(LINT_FILE_CHECKS, fname, fname)
    if ext in ignore_types:
        continue
    try:
        with codecs.open(fname, 'r', encoding='utf-8') as f_handle:
            content = f_handle.read()
    except UnicodeDecodeError:
        add_errors(fname, "File is not readable as UTF-8. Please set your editor to UTF-8 mode.")
        continue
    run_checks(LINT_CONTENT_CHECKS, fname, fname, content)

run_checks(LINT_POST_CHECKS, 'POST')

for f, errs in sorted(errors.items()):
    print("\033[0;32m************* File \033[1;32m{}\033[0m".format(f))
    for err in errs:
        print(err)
    print()

sys.exit(len(errors))
