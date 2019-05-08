#!/usr/bin/env python
import codecs
import json
import os
import re
import sys

root_path = os.path.abspath(os.path.normpath(os.path.join(__file__, '..', '..')))
basepath = os.path.join(root_path, 'esphome')


def walk_files(path):
    for root, _, files in os.walk(path):
        for name in files:
            yield os.path.join(root, name)


def shlex_quote(s):
    if not s:
        return u"''"
    if re.search(r'[^\w@%+=:,./-]', s) is None:
        return s

    return u"'" + s.replace(u"'", u"'\"'\"'") + u"'"


def build_compile_commands():
    gcc_flags_json = os.path.join(root_path, '.gcc-flags.json')
    if not os.path.isfile(gcc_flags_json):
        print("Could not find {} file which is required for clang-tidy.")
        print('Please run "pio init --ide atom" in the root esphome folder to generate that file.')
        sys.exit(1)
    with codecs.open(gcc_flags_json, 'r', encoding='utf-8') as f:
        gcc_flags = json.load(f)
    exec_path = gcc_flags['execPath']
    include_paths = gcc_flags['gccIncludePaths'].split(',')
    includes = ['-I{}'.format(p) for p in include_paths]
    cpp_flags = gcc_flags['gccDefaultCppFlags'].split(' ')
    defines = [flag for flag in cpp_flags if flag.startswith('-D')]
    command = [exec_path]
    command.extend(includes)
    command.extend(defines)
    command.append('-std=gnu++11')
    command.append('-Wall')
    command.append('-Wno-delete-non-virtual-dtor')
    command.append('-Wno-unused-variable')
    command.append('-Wunreachable-code')

    source_files = []
    for path in walk_files(basepath):
        filetypes = ('.cpp',)
        ext = os.path.splitext(path)[1]
        if ext in filetypes:
            source_files.append(os.path.abspath(path))
    source_files.sort()
    compile_commands = [{
        'directory': root_path,
        'command': ' '.join(shlex_quote(x) for x in (command + ['-o', p + '.o', '-c', p])),
        'file': p
    } for p in source_files]
    compile_commands_json = os.path.join(root_path, 'compile_commands.json')
    if os.path.isfile(compile_commands_json):
        with codecs.open(compile_commands_json, 'r', encoding='utf-8') as f:
            try:
                if json.load(f) == compile_commands:
                    return
            except:
                pass
    with codecs.open(compile_commands_json, 'w', encoding='utf-8') as f:
        json.dump(compile_commands, f, indent=2)


def main():
    build_compile_commands()
    print("Done.")


if __name__ == '__main__':
    main()
