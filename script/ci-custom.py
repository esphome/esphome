#!/usr/bin/env python
from __future__ import print_function

import codecs
import collections
import os.path
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


files = []
for root, _, fs in os.walk('esphome'):
    for f in fs:
        _, ext = os.path.splitext(f)
        if ext in ('.h', '.c', '.cpp', '.tcc', '.yaml', '.yml', '.ini', '.txt',
                   '.py', '.html', '.js', '.md'):
            files.append(os.path.join(root, f))
ignore = [
    'esphome/dashboard/static/materialize.min.js',
    'esphome/dashboard/static/ace.js',
    'esphome/dashboard/static/mode-yaml.js',
    'esphome/dashboard/static/theme-dreamweaver.js',
    'esphome/dashboard/static/jquery.validate.min.js',
    'esphome/dashboard/static/ext-searchbox.js',
]
files = [f for f in files if f not in ignore]
files.sort()

errors = collections.defaultdict(list)
for f in files:
    try:
        with codecs.open(f, 'r', encoding='utf-8') as f_handle:
            content = f_handle.read()
    except UnicodeDecodeError:
        errors[f].append("File is not readable as UTF-8. Please set your editor to UTF-8 mode.")
        continue
    for line, col in find_all(content, '\t'):
        errors[f].append("File contains tab character on line {}:{}. "
                         "Please convert tabs to spaces.".format(line, col))
    for line, col in find_all(content, '\r'):
        errors[f].append("File contains windows newline on line {}:{}. "
                         "Please set your editor to unix newline mode.".format(line, col))
    if content and not content.endswith('\n'):
        errors[f].append("File does not end with a newline, please add an empty line at the end of "
                         "the file.")
    _, ext = os.path.splitext(f)
    if ext in ('.h', '.c', '.cpp', '.tcc'):
        for line, col in find_all(content, '"esphome.h"'):
            errors[f].append("File contains reference to 'esphome.h' - This file is "
                             "auto-generated and should only be used for *custom* "
                             "components. Please replace with references to the direct "
                             "files.")

for f, errs in errors.items():
    print("\033[0;32m************* File \033[1;32m{}\033[0m".format(f))
    for err in errs:
        print(err)
    print()

sys.exit(len(errors))
