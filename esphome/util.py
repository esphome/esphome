import typing
from typing import Union, List

import collections
import io
import logging
import os
import re
import subprocess
import sys
from pathlib import Path

from esphome import const

_LOGGER = logging.getLogger(__name__)


class RegistryEntry:
    def __init__(self, name, fun, type_id, schema):
        self.name = name
        self.fun = fun
        self.type_id = type_id
        self.raw_schema = schema

    @property
    def coroutine_fun(self):
        from esphome.core import coroutine

        return coroutine(self.fun)

    @property
    def schema(self):
        from esphome.config_validation import Schema

        return Schema(self.raw_schema)


class Registry(dict):
    def __init__(self, base_schema=None, type_id_key=None):
        super().__init__()
        self.base_schema = base_schema or {}
        self.type_id_key = type_id_key

    def register(self, name, type_id, schema):
        def decorator(fun):
            self[name] = RegistryEntry(name, fun, type_id, schema)
            return fun

        return decorator


class SimpleRegistry(dict):
    def register(self, name, data):
        def decorator(fun):
            self[name] = (fun, data)
            return fun

        return decorator


def safe_print(message=""):
    from esphome.core import CORE

    if CORE.dashboard:
        try:
            message = message.replace("\033", "\\033")
        except UnicodeEncodeError:
            pass

    try:
        print(message)
        return
    except UnicodeEncodeError:
        pass

    try:
        print(message.encode("utf-8", "backslashreplace"))
    except UnicodeEncodeError:
        try:
            print(message.encode("ascii", "backslashreplace"))
        except UnicodeEncodeError:
            print("Cannot print line because of invalid locale!")


def shlex_quote(s):
    if not s:
        return "''"
    if re.search(r"[^\w@%+=:,./-]", s) is None:
        return s

    return "'" + s.replace("'", "'\"'\"'") + "'"


ANSI_ESCAPE = re.compile(r"\033[@-_][0-?]*[ -/]*[@-~]")


class RedirectText:
    def __init__(self, out, filter_lines=None):
        self._out = out
        if filter_lines is None:
            self._filter_pattern = None
        else:
            pattern = r"|".join(r"(?:" + pattern + r")" for pattern in filter_lines)
            self._filter_pattern = re.compile(pattern)
        self._line_buffer = ""

    def __getattr__(self, item):
        return getattr(self._out, item)

    def _write_color_replace(self, s):
        from esphome.core import CORE

        if CORE.dashboard:
            # With the dashboard, we must create a little hack to make color output
            # work. The shell we create in the dashboard is not a tty, so python removes
            # all color codes from the resulting stream. We just convert them to something
            # we can easily recognize later here.
            s = s.replace("\033", "\\033")
        self._out.write(s)

    def write(self, s):
        # s is usually a str already (self._out is of type TextIOWrapper)
        # However, s is sometimes also a bytes object in python3. Let's make sure it's a
        # str
        # If the conversion fails, we will create an exception, which is okay because we won't
        # be able to print it anyway.
        if not isinstance(s, str):
            s = s.decode()

        if self._filter_pattern is not None:
            self._line_buffer += s
            lines = self._line_buffer.splitlines(True)
            for line in lines:
                if "\n" not in line and "\r" not in line:
                    # Not a complete line, set line buffer
                    self._line_buffer = line
                    break
                self._line_buffer = ""

                line_without_ansi = ANSI_ESCAPE.sub("", line)
                line_without_end = line_without_ansi.rstrip()
                if self._filter_pattern.match(line_without_end) is not None:
                    # Filter pattern matched, ignore the line
                    continue

                self._write_color_replace(line)
        else:
            self._write_color_replace(s)

        # write() returns the number of characters written
        # Let's print the number of characters of the original string in order to not confuse
        # any caller.
        return len(s)

    # pylint: disable=no-self-use
    def isatty(self):
        return True


def run_external_command(
    func, *cmd, capture_stdout: bool = False, filter_lines: str = None
) -> Union[int, str]:
    """
    Run a function from an external package that acts like a main method.

    Temporarily replaces stdin/stderr/stdout, sys.argv and sys.exit handler during the run.

    :param func: Function to execute
    :param cmd: Command to run as (eg first element of sys.argv)
    :param capture_stdout: Capture text from stdout and return that.
    :param filter_lines: Regular expression used to filter captured output.
    :return: str if `capture_stdout` is set else int exit code.

    """

    def mock_exit(return_code):
        raise SystemExit(return_code)

    orig_argv = sys.argv
    orig_exit = sys.exit  # mock sys.exit
    full_cmd = " ".join(shlex_quote(x) for x in cmd)
    _LOGGER.debug("Running:  %s", full_cmd)

    orig_stdout = sys.stdout
    sys.stdout = RedirectText(sys.stdout, filter_lines=filter_lines)
    orig_stderr = sys.stderr
    sys.stderr = RedirectText(sys.stderr, filter_lines=filter_lines)

    if capture_stdout:
        cap_stdout = sys.stdout = io.StringIO()

    try:
        sys.argv = list(cmd)
        sys.exit = mock_exit
        return func() or 0
    except KeyboardInterrupt:  # pylint: disable=try-except-raise
        raise
    except SystemExit as err:
        return err.args[0]
    except Exception as err:  # pylint: disable=broad-except
        _LOGGER.error("Running command failed: %s", err)
        _LOGGER.error("Please try running %s locally.", full_cmd)
        return 1
    finally:
        sys.argv = orig_argv
        sys.exit = orig_exit

        sys.stdout = orig_stdout
        sys.stderr = orig_stderr

        if capture_stdout:
            # pylint: disable=lost-exception
            return cap_stdout.getvalue()


def run_external_process(*cmd, **kwargs):
    full_cmd = " ".join(shlex_quote(x) for x in cmd)
    _LOGGER.debug("Running:  %s", full_cmd)
    filter_lines = kwargs.get("filter_lines")

    capture_stdout = kwargs.get("capture_stdout", False)
    if capture_stdout:
        sub_stdout = subprocess.PIPE
    else:
        sub_stdout = RedirectText(sys.stdout, filter_lines=filter_lines)

    sub_stderr = RedirectText(sys.stderr, filter_lines=filter_lines)

    try:
        proc = subprocess.run(
            cmd, stdout=sub_stdout, stderr=sub_stderr, encoding="utf-8", check=False
        )
        return proc.stdout if capture_stdout else proc.returncode
    except KeyboardInterrupt:  # pylint: disable=try-except-raise
        raise
    except Exception as err:  # pylint: disable=broad-except
        _LOGGER.error("Running command failed: %s", err)
        _LOGGER.error("Please try running %s locally.", full_cmd)
        return 1


def is_dev_esphome_version():
    return "dev" in const.__version__


def parse_esphome_version() -> typing.Tuple[int, int, int]:
    match = re.match(r"^(\d+).(\d+).(\d+)(-dev\d*|b\d*)?$", const.__version__)
    if match is None:
        raise ValueError(f"Failed to parse ESPHome version '{const.__version__}'")
    return int(match.group(1)), int(match.group(2)), int(match.group(3))


# Custom OrderedDict with nicer repr method for debugging
class OrderedDict(collections.OrderedDict):
    def __repr__(self):
        return dict(self).__repr__()


def list_yaml_files(folders):
    files = filter_yaml_files(
        [os.path.join(folder, p) for folder in folders for p in os.listdir(folder)]
    )
    files.sort()
    return files


def filter_yaml_files(files):
    return [
        f
        for f in files
        if (
            os.path.splitext(f)[1] in (".yaml", ".yml")
            and os.path.basename(f) not in ("secrets.yaml", "secrets.yml")
            and not os.path.basename(f).startswith(".")
        )
    ]


class SerialPort:
    def __init__(self, path: str, description: str):
        self.path = path
        self.description = description


# from https://github.com/pyserial/pyserial/blob/master/serial/tools/list_ports.py
def get_serial_ports() -> List[SerialPort]:
    from serial.tools.list_ports import comports

    result = []
    for port, desc, info in comports(include_links=True):
        if not port:
            continue
        if "VID:PID" in info:
            result.append(SerialPort(path=port, description=desc))
    # Also add objects in /dev/serial/by-id/
    # ref: https://github.com/esphome/issues/issues/1346

    by_id_path = Path("/dev/serial/by-id")
    if sys.platform.lower().startswith("linux") and by_id_path.exists():
        from serial.tools.list_ports_linux import SysFS

        for path in by_id_path.glob("*"):
            device = SysFS(path)
            if device.subsystem == "platform":
                result.append(SerialPort(path=str(path), description=info[1]))

    result.sort(key=lambda x: x.path)
    return result
