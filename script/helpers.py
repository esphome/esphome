import codecs
import json
import os.path
import re
import subprocess
import sys

root_path = os.path.abspath(os.path.normpath(os.path.join(__file__, '..', '..')))
basepath = os.path.join(root_path, 'esphome')
temp_header_file = os.path.join(root_path, '.temp-clang-tidy.cpp')


def shlex_quote(s):
    if not s:
        return "''"
    if re.search(r'[^\w@%+=:,./-]', s) is None:
        return s

    return "'" + s.replace("'", "'\"'\"'") + "'"


def build_all_include():
    # Build a cpp file that includes all header files in this repo.
    # Otherwise header-only integrations would not be tested by clang-tidy
    headers = []
    for path in walk_files(basepath):
        filetypes = ('.h',)
        ext = os.path.splitext(path)[1]
        if ext in filetypes:
            path = os.path.relpath(path, root_path)
            include_p = path.replace(os.path.sep, '/')
            headers.append(f'#include "{include_p}"')
    headers.sort()
    headers.append('')
    content = '\n'.join(headers)
    with codecs.open(temp_header_file, 'w', encoding='utf-8') as f:
        f.write(content)


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
    includes = [f'-I{p}' for p in include_paths]
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
    source_files.append(temp_header_file)
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


def walk_files(path):
    for root, _, files in os.walk(path):
        for name in files:
            yield os.path.join(root, name)


def get_output(*args):
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, err = proc.communicate()
    return output.decode('utf-8')


def splitlines_no_ends(string):
    return [s.strip() for s in string.splitlines()]


def changed_files():
    check_remotes = ['upstream', 'origin']
    check_remotes.extend(splitlines_no_ends(get_output('git', 'remote')))
    for remote in check_remotes:
        command = ['git', 'merge-base', f'refs/remotes/{remote}/dev', 'HEAD']
        try:
            merge_base = splitlines_no_ends(get_output(*command))[0]
            break
        except:
            pass
    else:
        raise ValueError("Git not configured")
    command = ['git', 'diff', merge_base, '--name-only']
    changed = splitlines_no_ends(get_output(*command))
    changed = [os.path.relpath(f, os.getcwd()) for f in changed]
    changed.sort()
    return changed


def filter_changed(files):
    changed = changed_files()
    files = [f for f in files if f in changed]
    print("Changed files:")
    if not files:
        print("    No changed files!")
    for c in files:
        print(f"    {c}")
    return files


def git_ls_files():
    command = ['git', 'ls-files', '-s']
    proc = subprocess.Popen(command, stdout=subprocess.PIPE)
    output, err = proc.communicate()
    lines = [x.split() for x in output.decode('utf-8').splitlines()]
    return {
        s[3].strip(): int(s[0]) for s in lines
    }
