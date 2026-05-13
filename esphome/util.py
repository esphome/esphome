import collections
from collections.abc import Callable
from dataclasses import dataclass
import io
import logging
from pathlib import Path
import re
import subprocess
import sys
from typing import TYPE_CHECKING, Any

from esphome import const

_LOGGER = logging.getLogger(__name__)

if TYPE_CHECKING:
    from esphome.config_validation import Schema
    from esphome.cpp_generator import MockObjClass


class RegistryEntry:
    def __init__(
        self,
        name: str,
        fun: Callable[..., Any],
        type_id: "MockObjClass",
        schema: "Schema",
        *,
        synchronous: bool = False,
    ):
        self.name = name
        self.fun = fun
        self.type_id = type_id
        self.raw_schema = schema
        self.synchronous = synchronous

    @property
    def coroutine_fun(self):
        from esphome.core import coroutine

        return coroutine(self.fun)

    @property
    def schema(self):
        from esphome.config_validation import Schema

        return Schema(self.raw_schema)


class Registry(dict[str, RegistryEntry]):
    def __init__(self, base_schema=None, type_id_key=None):
        super().__init__()
        self.base_schema = base_schema or {}
        self.type_id_key = type_id_key

    def register(
        self,
        name: str,
        type_id: "MockObjClass",
        schema: "Schema",
        *,
        synchronous: bool = False,
    ):
        def decorator(fun: Callable[..., Any]):
            self[name] = RegistryEntry(
                name, fun, type_id, schema, synchronous=synchronous
            )
            return fun

        return decorator


class SimpleRegistry(dict):
    def register(self, name: str, data: Any):
        def decorator(fun: Callable[..., Any]):
            self[name] = (fun, data)
            return fun

        return decorator


def safe_print(message="", end="\n"):
    from esphome.core import CORE

    if CORE.dashboard:
        try:  # noqa: SIM105
            message = message.replace("\033", "\\033")
        except UnicodeEncodeError:
            pass

    try:
        print(message, end=end)
        return
    except UnicodeEncodeError:
        pass

    # Fall back to the stream's actual encoding (e.g. cp1252 on Windows
    # redirected pipes). Use "backslashreplace" so unencodable code points
    # like the wifi signal-bar block characters (U+2582..U+2588) become
    # readable ``\uXXXX`` escapes, and decode back to ``str`` so ``print``
    # never receives a ``bytes`` object (which would render as a ``b'...'``
    # repr).
    encoding = sys.stdout.encoding or "ascii"
    try:
        print(
            message.encode(encoding, "backslashreplace").decode(encoding),
            end=end,
        )
        return
    except UnicodeEncodeError:
        pass

    try:
        print(
            message.encode("ascii", "backslashreplace").decode("ascii"),
            end=end,
        )
    except UnicodeEncodeError:
        print("Cannot print line because of invalid locale!")


def safe_input(prompt=""):
    if prompt:
        safe_print(prompt, end="")
    return input()


def shlex_quote(s: str | Path) -> str:
    # Convert Path objects to strings
    if isinstance(s, Path):
        s = str(s)
    if not s:
        return "''"
    if re.search(r"[^\w@%+=:,./-]", s) is None:
        return s

    return "'" + s.replace("'", "'\"'\"'") + "'"


ANSI_ESCAPE = re.compile(r"\033[@-_][0-?]*[ -/]*[@-~]")


class RedirectText:
    def __init__(
        self,
        out,
        filter_lines: list[str] | None = None,
        line_callbacks: list[Callable[[str], str | None]] | None = None,
    ) -> None:
        self._out = out
        if filter_lines is None:
            self._filter_pattern = None
        else:
            pattern = r"|".join(r"(?:" + pattern + r")" for pattern in filter_lines)
            self._filter_pattern = re.compile(pattern)
        self._line_buffer = ""
        self._line_callbacks = line_callbacks or []

    def __getattr__(self, item):
        return getattr(self._out, item)

    def _write_color_replace(self, s: str | bytes) -> None:
        from esphome.core import CORE

        if CORE.dashboard:
            # With the dashboard, we must create a little hack to make color output
            # work. The shell we create in the dashboard is not a tty, so python removes
            # all color codes from the resulting stream. We just convert them to something
            # we can easily recognize later here.
            s = s.replace("\033", "\\033")
        self._out.write(s)

    def write(self, s: str | bytes) -> int:
        # s is usually a str already (self._out is of type TextIOWrapper)
        # However, s is sometimes also a bytes object in python3. Let's make sure it's a
        # str
        # If the conversion fails, we will create an exception, which is okay because we won't
        # be able to print it anyway.
        if not isinstance(s, str):
            s = s.decode()

        if self._filter_pattern is not None or self._line_callbacks:
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
                if (
                    self._filter_pattern is not None
                    and self._filter_pattern.match(line_without_end) is not None
                ):
                    # Filter pattern matched, ignore the line
                    continue

                self._write_color_replace(line)
                # Check for flash size error and provide helpful guidance
                if (
                    "Error: The program size" in line
                    and "is greater than maximum allowed" in line
                    and (help_msg := get_esp32_arduino_flash_error_help())
                ):
                    self._write_color_replace(help_msg)
                for callback in self._line_callbacks:
                    if msg := callback(line_without_end):
                        self._write_color_replace(msg)
        else:
            self._write_color_replace(s)

        # write() returns the number of characters written
        # Let's print the number of characters of the original string in order to not confuse
        # any caller.
        return len(s)

    def isatty(self):
        return True


def run_external_command(
    func,
    *cmd,
    capture_stdout: bool = False,
    filter_lines: list[str] | None = None,
    line_callbacks: list[Callable[[str], str | None]] | None = None,
) -> int | str:
    """
    Run a function from an external package that acts like a main method.

    Temporarily replaces stdin/stderr/stdout, sys.argv and sys.exit handler during the run.

    :param func: Function to execute
    :param cmd: Command to run as (eg first element of sys.argv)
    :param capture_stdout: Capture text from stdout and return that.
        Note: line_callbacks are not invoked when capture_stdout is True.
    :param filter_lines: Regular expressions used to filter captured output.
    :param line_callbacks: Callbacks invoked per line; non-None returns are written to output.
    :return: str if `capture_stdout` is set else int exit code.

    """

    def mock_exit(return_code):
        raise SystemExit(return_code)

    orig_argv = sys.argv
    orig_exit = sys.exit  # mock sys.exit
    full_cmd = " ".join(shlex_quote(x) for x in cmd)
    _LOGGER.debug("Running:  %s", full_cmd)

    orig_stdout = sys.stdout
    sys.stdout = RedirectText(
        sys.stdout, filter_lines=filter_lines, line_callbacks=line_callbacks
    )
    orig_stderr = sys.stderr
    sys.stderr = RedirectText(
        sys.stderr, filter_lines=filter_lines, line_callbacks=line_callbacks
    )

    if capture_stdout:
        cap_stdout = sys.stdout = io.StringIO()

    try:
        sys.argv = list(cmd)
        sys.exit = mock_exit
        retval = func() or 0
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
        return cap_stdout.getvalue()

    return retval


def run_external_process(*cmd: str, **kwargs: Any) -> int | str:
    full_cmd = " ".join(shlex_quote(x) for x in cmd)
    _LOGGER.debug("Running:  %s", full_cmd)
    filter_lines = kwargs.get("filter_lines")
    line_callbacks = kwargs.get("line_callbacks")

    capture_stdout = kwargs.get("capture_stdout", False)
    if capture_stdout:
        sub_stdout = subprocess.PIPE
    else:
        sub_stdout = RedirectText(
            sys.stdout, filter_lines=filter_lines, line_callbacks=line_callbacks
        )

    sub_stderr = RedirectText(
        sys.stderr, filter_lines=filter_lines, line_callbacks=line_callbacks
    )

    try:
        proc = subprocess.run(
            cmd,
            stdout=sub_stdout,
            stderr=sub_stderr,
            encoding="utf-8",
            check=False,
            close_fds=False,
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


def parse_esphome_version() -> tuple[int, int, int]:
    match = re.match(r"^(\d+).(\d+).(\d+)(-dev\d*|b\d*)?$", const.__version__)
    if match is None:
        raise ValueError(f"Failed to parse ESPHome version '{const.__version__}'")
    return int(match.group(1)), int(match.group(2)), int(match.group(3))


# Custom OrderedDict with nicer repr method for debugging
class OrderedDict(collections.OrderedDict):
    def __repr__(self):
        return dict(self).__repr__()


def list_yaml_files(configs: list[str | Path]) -> list[Path]:
    files: list[Path] = []
    for config in configs:
        config = Path(config)
        if not config.exists():
            raise FileNotFoundError(f"Config path '{config}' does not exist!")
        if config.is_file():
            files.append(config)
        else:
            files.extend(config.glob("*"))
    files = filter_yaml_files(files)
    return sorted(files)


def filter_yaml_files(files: list[Path]) -> list[Path]:
    return [
        f
        for f in files
        if (
            f.suffix in (".yaml", ".yml")
            and f.name not in ("secrets.yaml", "secrets.yml")
            and not f.name.startswith(".")
        )
    ]


class SerialPort:
    def __init__(self, path: str, description: str):
        self.path = path
        self.description = description


# from https://github.com/pyserial/pyserial/blob/master/serial/tools/list_ports.py
def get_serial_ports() -> list[SerialPort]:
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


PICOTOOL_PACKAGE = "tool-picotool-rp2040-earlephilhower"


def get_picotool_path(cc_path: str) -> Path | None:
    """Derive the picotool binary path from the PlatformIO toolchain cc_path.

    The cc_path from IDEData points to the toolchain package, e.g.:
    ~/.platformio/packages/toolchain-rp2040-earlephilhower/bin/arm-none-eabi-gcc
    Picotool is in a sibling package:
    ~/.platformio/packages/tool-picotool-rp2040-earlephilhower/picotool
    """
    cc = Path(cc_path)
    # Go from .../packages/toolchain-.../bin/gcc up to .../packages/
    packages_dir = cc.parent.parent.parent
    binary_name = "picotool.exe" if sys.platform == "win32" else "picotool"
    picotool = packages_dir / PICOTOOL_PACKAGE / binary_name
    if picotool.is_file():
        return picotool
    return None


def is_picotool_usb_permission_error(output: str | bytes) -> bool:
    """Check if picotool output indicates a USB permission error."""
    if isinstance(output, str):
        return (
            "unable to connect" in output
            or "LIBUSB_ERROR_ACCESS" in output
            or "Permission denied" in output
        )
    return (
        b"unable to connect" in output
        or b"LIBUSB_ERROR_ACCESS" in output
        or b"Permission denied" in output
    )


@dataclass
class BootselResult:
    """Result of RP2040 BOOTSEL detection."""

    device_count: int
    permission_error: bool = False


def detect_rp2040_bootsel(picotool_path: str | Path) -> BootselResult:
    """Detect RP2040/RP2350 devices in BOOTSEL mode using picotool.

    Returns a BootselResult with the number of devices found (by counting
    'type:' lines in output), and whether a permission error was detected.
    """
    try:
        result = subprocess.run(
            [str(picotool_path), "info", "-d"],
            capture_output=True,
            timeout=10,
            check=False,
        )
        device_count = result.stdout.count(b"type:")
        if device_count > 0:
            return BootselResult(device_count)
        # Check for permission issues — picotool can see the device
        # on the USB bus but can't connect without proper permissions
        if is_picotool_usb_permission_error(result.stderr + result.stdout):
            return BootselResult(0, permission_error=True)
        return BootselResult(0)
    except (OSError, subprocess.TimeoutExpired):
        return BootselResult(0)


def get_esp32_arduino_flash_error_help() -> str | None:
    """Returns helpful message when ESP32 with Arduino runs out of flash space."""
    from esphome.core import CORE

    if not (CORE.is_esp32 and CORE.using_arduino):
        return None

    from esphome.log import AnsiFore, color

    return (
        "\n"
        + color(
            AnsiFore.YELLOW,
            "💡 TIP: Your ESP32 with Arduino framework has run out of flash space.\n",
        )
        + "\n"
        + "To fix this, switch to the ESP-IDF framework which is more memory efficient:\n"
        + "\n"
        + "1. In your YAML configuration, modify the framework section:\n"
        + "\n"
        + "   esp32:\n"
        + "     framework:\n"
        + "       type: esp-idf\n"
        + "\n"
        + "2. Clean build files and compile again\n"
        + "\n"
        + "Note: ESP-IDF uses less flash space and provides better performance.\n"
        + "Some Arduino-specific libraries may need alternatives.\n"
        + "\n"
        + "For detailed migration instructions, see:\n"
        + color(
            AnsiFore.BLUE,
            "https://esphome.io/guides/esp32_arduino_to_idf/\n\n",
        )
    )


@dataclass
class FlashImage:
    path: Path
    offset: str
