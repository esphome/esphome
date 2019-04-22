#!/usr/bin/env python

from __future__ import print_function

import codecs
import json
import multiprocessing
import os
import re

import pexpect
import shutil
import subprocess
import sys
import tempfile

import argparse
import click
import threading

is_py2 = sys.version[0] == '2'

if is_py2:
    import Queue as queue
else:
    import queue as queue

root_path = os.path.abspath(os.path.normpath(os.path.join(__file__, '..', '..')))
basepath = os.path.join(root_path, 'esphome')
rel_basepath = os.path.relpath(basepath, os.getcwd())
temp_header_file = os.path.join(root_path, '.temp-clang-tidy.cpp')


def run_tidy(args, tmpdir, queue, lock, failed_files):
    while True:
        path = queue.get()
        invocation = ['clang-tidy-7', '-header-filter=^{}/.*'.format(re.escape(basepath))]
        if tmpdir is not None:
            invocation.append('-export-fixes')
            # Get a temporary file. We immediately close the handle so clang-tidy can
            # overwrite it.
            (handle, name) = tempfile.mkstemp(suffix='.yaml', dir=tmpdir)
            os.close(handle)
            invocation.append(name)
        invocation.append('-p=.')
        if args.quiet:
            invocation.append('-quiet')
        invocation.append(os.path.abspath(path))
        invocation_s = ' '.join(shlex_quote(x) for x in invocation)

        # Use pexpect for a pseudy-TTY with colored output
        output, rc = pexpect.run(invocation_s, withexitstatus=True, encoding='utf-8',
                                 timeout=15*60)
        with lock:
            if rc != 0:
                print()
                print("\033[0;32m************* File \033[1;32m{}\033[0m".format(path))
                print(invocation_s)
                print(output)
                print()
                failed_files.append(path)
        queue.task_done()


def progress_bar_show(value):
    if value is None:
        return ''
    return value


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


def filter_changed(files):
    for remote in ('upstream', 'origin'):
        command = ['git', 'merge-base', '{}/dev'.format(remote), 'HEAD']
        try:
            merge_base = splitlines_no_ends(get_output(*command))[0]
            break
        except:
            pass
    else:
        return files
    command = ['git', 'diff', merge_base, '--name-only']
    changed = splitlines_no_ends(get_output(*command))
    changed = {os.path.relpath(f, os.getcwd()) for f in changed}
    print("Changed Files:")
    files = [p for p in files if p in changed]
    for p in files:
        print("  {}".format(p))
    if not files:
        print("  No changed files")
    return files


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
    command.append('-Wfor-loop-analysis')
    command.append('-Wshadow-field')
    command.append('-Wshadow-field-in-constructor')
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
            headers.append('#include "{}"'.format(include_p))
    headers.sort()
    headers.append('')
    content = '\n'.join(headers)
    with codecs.open(temp_header_file, 'w', encoding='utf-8') as f:
        f.write(content)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-j', '--jobs', type=int,
                        default=multiprocessing.cpu_count(),
                        help='number of tidy instances to be run in parallel.')
    parser.add_argument('files', nargs='*', default=[],
                        help='files to be processed (regex on path)')
    parser.add_argument('--fix', action='store_true', help='apply fix-its')
    parser.add_argument('-q', '--quiet', action='store_false',
                        help='Run clang-tidy in quiet mode')
    parser.add_argument('-c', '--changed', action='store_true',
                        help='Only run on changed files')
    parser.add_argument('--all-headers', action='store_true',
                        help='Create a dummy file that checks all headers')
    args = parser.parse_args()

    try:
        get_output('clang-tidy-7', '-version')
    except:
        print("""
        Oops. It looks like clang-tidy is not installed. 
        
        Please check you can run "clang-tidy-7 -version" in your terminal and install
        clang-tidy (v7) if necessary.
        
        Note you can also upload your code as a pull request on GitHub and see the CI check
        output to apply clang-tidy.
        """)
        return 1

    build_compile_commands()

    files = []
    for path in walk_files(basepath):
        filetypes = ('.cpp',)
        ext = os.path.splitext(path)[1]
        if ext in filetypes:
            path = os.path.relpath(path, os.getcwd())
            files.append(path)
    # Match against re
    file_name_re = re.compile('|'.join(args.files))
    files = [p for p in files if file_name_re.search(p)]

    if args.changed:
        files = filter_changed(files)

    files.sort()

    if args.all_headers:
        build_all_include()
        files.insert(0, temp_header_file)

    tmpdir = None
    if args.fix:
        tmpdir = tempfile.mkdtemp()

    failed_files = []
    return_code = 0
    try:
        task_queue = queue.Queue(args.jobs)
        lock = threading.Lock()
        for _ in range(args.jobs):
            t = threading.Thread(target=run_tidy,
                                 args=(args, tmpdir, task_queue, lock, failed_files))
            t.daemon = True
            t.start()

        # Fill the queue with files.
        with click.progressbar(files, width=30, file=sys.stderr,
                               item_show_func=progress_bar_show) as bar:
            for name in bar:
                task_queue.put(name)

        # Wait for all threads to be done.
        task_queue.join()
        return_code = len(failed_files)

    except KeyboardInterrupt:
        print()
        print('Ctrl-C detected, goodbye.')
        if tmpdir:
            shutil.rmtree(tmpdir)
        if os.path.exists(temp_header_file):
            os.remove(temp_header_file)
        os.kill(0, 9)

    if args.fix and failed_files:
        print('Applying fixes ...')
        try:
            subprocess.call(['clang-apply-replacements-7', tmpdir])
        except:
            print('Error applying fixes.\n', file=sys.stderr)
            if os.path.exists(temp_header_file):
                os.remove(temp_header_file)
            raise

    if os.path.exists(temp_header_file):
        os.remove(temp_header_file)
    sys.exit(return_code)


if __name__ == '__main__':
    main()
