import codecs
import os.path
import re
import subprocess
import json
from pathlib import Path

root_path = os.path.abspath(os.path.normpath(os.path.join(__file__, "..", "..")))
basepath = os.path.join(root_path, "esphome")
temp_folder = os.path.join(root_path, ".temp")
temp_header_file = os.path.join(temp_folder, "all-include.cpp")


def shlex_quote(s):
    if not s:
        return "''"
    if re.search(r"[^\w@%+=:,./-]", s) is None:
        return s

    return "'" + s.replace("'", "'\"'\"'") + "'"


def build_all_include():
    # Build a cpp file that includes all header files in this repo.
    # Otherwise header-only integrations would not be tested by clang-tidy
    headers = []
    for path in walk_files(basepath):
        filetypes = (".h",)
        ext = os.path.splitext(path)[1]
        if ext in filetypes:
            path = os.path.relpath(path, root_path)
            include_p = path.replace(os.path.sep, "/")
            headers.append(f'#include "{include_p}"')
    headers.sort()
    headers.append("")
    content = "\n".join(headers)
    p = Path(temp_header_file)
    p.parent.mkdir(exist_ok=True)
    p.write_text(content)


def walk_files(path):
    for root, _, files in os.walk(path):
        for name in files:
            yield os.path.join(root, name)


def get_output(*args):
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, err = proc.communicate()
    return output.decode("utf-8")


def get_err(*args):
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, err = proc.communicate()
    return err.decode("utf-8")


def splitlines_no_ends(string):
    return [s.strip() for s in string.splitlines()]


def changed_files():
    check_remotes = ["upstream", "origin"]
    check_remotes.extend(splitlines_no_ends(get_output("git", "remote")))
    for remote in check_remotes:
        command = ["git", "merge-base", f"refs/remotes/{remote}/dev", "HEAD"]
        try:
            merge_base = splitlines_no_ends(get_output(*command))[0]
            break
        # pylint: disable=bare-except
        except:
            pass
    else:
        raise ValueError("Git not configured")
    command = ["git", "diff", merge_base, "--name-only"]
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


def git_ls_files(patterns=None):
    command = ["git", "ls-files", "-s"]
    if patterns is not None:
        command.extend(patterns)
    proc = subprocess.Popen(command, stdout=subprocess.PIPE)
    output, err = proc.communicate()
    lines = [x.split() for x in output.decode("utf-8").splitlines()]
    return {s[3].strip(): int(s[0]) for s in lines}


def load_idedata(environment):
    platformio_ini = Path(root_path) / "platformio.ini"
    temp_idedata = Path(temp_folder) / f"idedata-{environment}.json"
    if not platformio_ini.is_file() or not temp_idedata.is_file():
        changed = True
    elif platformio_ini.stat().st_mtime >= temp_idedata.stat().st_mtime:
        changed = True
    else:
        changed = False

    if not changed:
        return json.loads(temp_idedata.read_text())

    stdout = subprocess.check_output(["pio", "run", "-t", "idedata", "-e", environment])
    match = re.search(r'{\s*".*}', stdout.decode("utf-8"))
    data = json.loads(match.group())

    temp_idedata.parent.mkdir(exist_ok=True)
    temp_idedata.write_text(json.dumps(data, indent=2) + "\n")
    return data
