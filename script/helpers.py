import colorama
import os.path
import re
import subprocess
import json
from pathlib import Path

root_path = os.path.abspath(os.path.normpath(os.path.join(__file__, "..", "..")))
basepath = os.path.join(root_path, "esphome")
temp_folder = os.path.join(root_path, ".temp")
temp_header_file = os.path.join(temp_folder, "all-include.cpp")


def styled(color, msg, reset=True):
    prefix = "".join(color) if isinstance(color, tuple) else color
    suffix = colorama.Style.RESET_ALL if reset else ""
    return prefix + msg + suffix


def print_error_for_file(file, body):
    print(
        styled(colorama.Fore.GREEN, "### File ")
        + styled((colorama.Fore.GREEN, colorama.Style.BRIGHT), file)
    )
    print()
    if body is not None:
        print(body)
        print()


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


def filter_grep(files, value):
    matched = []
    for file in files:
        with open(file) as handle:
            contents = handle.read()
        if value in contents:
            matched.append(file)
    return matched


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
    changed = False
    if not platformio_ini.is_file() or not temp_idedata.is_file():
        changed = True
    elif platformio_ini.stat().st_mtime >= temp_idedata.stat().st_mtime:
        changed = True

    if "idf" in environment:
        # remove full sdkconfig when the defaults have changed so that it is regenerated
        default_sdkconfig = Path(root_path) / "sdkconfig.defaults"
        temp_sdkconfig = Path(temp_folder) / f"sdkconfig-{environment}"

        if not temp_sdkconfig.is_file():
            changed = True
        elif default_sdkconfig.stat().st_mtime >= temp_sdkconfig.stat().st_mtime:
            temp_sdkconfig.unlink()
            changed = True

    if not changed:
        return json.loads(temp_idedata.read_text())

    # ensure temp directory exists before running pio, as it writes sdkconfig to it
    Path(temp_folder).mkdir(exist_ok=True)

    stdout = subprocess.check_output(["pio", "run", "-t", "idedata", "-e", environment])
    match = re.search(r'{\s*".*}', stdout.decode("utf-8"))
    data = json.loads(match.group())

    temp_idedata.write_text(json.dumps(data, indent=2) + "\n")
    return data
