import codecs
from contextlib import suppress

import logging
import os
from pathlib import Path
from typing import Union
import tempfile

_LOGGER = logging.getLogger(__name__)


def ensure_unique_string(preferred_string, current_strings):
    test_string = preferred_string
    current_strings_set = set(current_strings)

    tries = 1

    while test_string in current_strings_set:
        tries += 1
        test_string = f"{preferred_string}_{tries}"

    return test_string


def indent_all_but_first_and_last(text, padding="  "):
    lines = text.splitlines(True)
    if len(lines) <= 2:
        return text
    return lines[0] + "".join(padding + line for line in lines[1:-1]) + lines[-1]


def indent_list(text, padding="  "):
    return [padding + line for line in text.splitlines()]


def indent(text, padding="  "):
    return "\n".join(indent_list(text, padding))


# From https://stackoverflow.com/a/14945195/8924614
def cpp_string_escape(string, encoding="utf-8"):
    def _should_escape(byte):  # type: (int) -> bool
        if not 32 <= byte < 127:
            return True
        if byte in (ord("\\"), ord('"')):
            return True
        return False

    if isinstance(string, str):
        string = string.encode(encoding)
    result = ""
    for character in string:
        if _should_escape(character):
            result += f"\\{character:03o}"
        else:
            result += chr(character)
    return f'"{result}"'


def run_system_command(*args):
    import subprocess

    with subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as p:
        stdout, stderr = p.communicate()
        rc = p.returncode
        return rc, stdout, stderr


def mkdir_p(path):
    if not path:
        # Empty path - means create current dir
        return
    try:
        os.makedirs(path)
    except OSError as err:
        import errno

        if err.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            from esphome.core import EsphomeError

            raise EsphomeError(f"Error creating directories {path}: {err}") from err


def is_ip_address(host):
    parts = host.split(".")
    if len(parts) != 4:
        return False
    try:
        for p in parts:
            int(p)
        return True
    except ValueError:
        return False


def _resolve_with_zeroconf(host):
    from esphome.core import EsphomeError
    from esphome.zeroconf import EsphomeZeroconf

    try:
        zc = EsphomeZeroconf()
    except Exception as err:
        raise EsphomeError(
            "Cannot start mDNS sockets, is this a docker container without "
            "host network mode?"
        ) from err
    try:
        info = zc.resolve_host(f"{host}.")
    except Exception as err:
        raise EsphomeError(f"Error resolving mDNS hostname: {err}") from err
    finally:
        zc.close()
    if info is None:
        raise EsphomeError(
            "Error resolving address with mDNS: Did not respond. "
            "Maybe the device is offline."
        )
    return info


def resolve_ip_address(host):
    from esphome.core import EsphomeError
    import socket

    errs = []

    if host.endswith(".local"):
        try:
            return _resolve_with_zeroconf(host)
        except EsphomeError as err:
            errs.append(str(err))

    try:
        return socket.gethostbyname(host)
    except OSError as err:
        errs.append(str(err))
        raise EsphomeError(f"Error resolving IP address: {', '.join(errs)}") from err


def get_bool_env(var, default=False):
    return bool(os.getenv(var, default))


def is_ha_addon():
    return get_bool_env("ESPHOME_IS_HA_ADDON")


def walk_files(path):
    for root, _, files in os.walk(path):
        for name in files:
            yield os.path.join(root, name)


def read_file(path):
    try:
        with codecs.open(path, "r", encoding="utf-8") as f_handle:
            return f_handle.read()
    except OSError as err:
        from esphome.core import EsphomeError

        raise EsphomeError(f"Error reading file {path}: {err}") from err
    except UnicodeDecodeError as err:
        from esphome.core import EsphomeError

        raise EsphomeError(f"Error reading file {path}: {err}") from err


def _write_file(path: Union[Path, str], text: Union[str, bytes]):
    """Atomically writes `text` to the given path.

    Automatically creates all parent directories.
    """
    if not isinstance(path, Path):
        path = Path(path)
    data = text
    if isinstance(text, str):
        data = text.encode()

    directory = path.parent
    directory.mkdir(exist_ok=True, parents=True)

    tmp_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=directory, delete=False
        ) as f_handle:
            tmp_path = f_handle.name
            f_handle.write(data)
        # Newer tempfile implementations create the file with mode 0o600
        os.chmod(tmp_path, 0o644)
        # If destination exists, will be overwritten
        os.replace(tmp_path, path)
    finally:
        if tmp_path is not None and os.path.exists(tmp_path):
            try:
                os.remove(tmp_path)
            except OSError as err:
                _LOGGER.error("Write file cleanup failed: %s", err)


def write_file(path: Union[Path, str], text: str):
    try:
        _write_file(path, text)
    except OSError as err:
        from esphome.core import EsphomeError

        raise EsphomeError(f"Could not write file at {path}") from err


def write_file_if_changed(path: Union[Path, str], text: str) -> bool:
    """Write text to the given path, but not if the contents match already.

    Returns true if the file was changed.
    """
    if not isinstance(path, Path):
        path = Path(path)

    src_content = None
    if path.is_file():
        src_content = read_file(path)
    if src_content == text:
        return False
    write_file(path, text)
    return True


def copy_file_if_changed(src: os.PathLike, dst: os.PathLike) -> None:
    import shutil

    if file_compare(src, dst):
        return
    mkdir_p(os.path.dirname(dst))
    try:
        shutil.copyfile(src, dst)
    except OSError as err:
        if isinstance(err, PermissionError):
            # Older esphome versions copied over the src file permissions too.
            # So when the dst file had 444 permissions, the dst file would have those
            # too and subsequent writes would fail

            # -> delete file (it would be overwritten anyway), and try again
            # if that fails, use normal error handler
            with suppress(OSError):
                os.unlink(dst)
                shutil.copyfile(src, dst)
                return

        from esphome.core import EsphomeError

        raise EsphomeError(f"Error copying file {src} to {dst}: {err}") from err


def list_starts_with(list_, sub):
    return len(sub) <= len(list_) and all(list_[i] == x for i, x in enumerate(sub))


def file_compare(path1: os.PathLike, path2: os.PathLike) -> bool:
    """Return True if the files path1 and path2 have the same contents."""
    import stat

    try:
        stat1, stat2 = os.stat(path1), os.stat(path2)
    except OSError:
        # File doesn't exist or another error -> not equal
        return False

    if (
        stat.S_IFMT(stat1.st_mode) != stat.S_IFREG
        or stat.S_IFMT(stat2.st_mode) != stat.S_IFREG
    ):
        # At least one of them is not a regular file (or does not exist)
        return False
    if stat1.st_size != stat2.st_size:
        # Different sizes
        return False

    bufsize = 8 * 1024
    # Read files in blocks until a mismatch is found
    with open(path1, "rb") as fh1, open(path2, "rb") as fh2:
        while True:
            blob1, blob2 = fh1.read(bufsize), fh2.read(bufsize)
            if blob1 != blob2:
                # Different content
                return False
            if not blob1:
                # Reached end
                return True


# A dict of types that need to be converted to heaptypes before a class can be added
# to the object
_TYPE_OVERLOADS = {
    int: type("EInt", (int,), {}),
    float: type("EFloat", (float,), {}),
    str: type("EStr", (str,), {}),
    dict: type("EDict", (dict,), {}),
    list: type("EList", (list,), {}),
}

# cache created classes here
_CLASS_LOOKUP = {}


def add_class_to_obj(value, cls):
    """Add a class to a python type.

    This function modifies value so that it has cls as a basetype.
    The value itself may be modified by this action! You must use the return
    value of this function however, since some types need to be copied first (heaptypes).
    """
    if isinstance(value, cls):
        # If already is instance, do not add
        return value

    try:
        orig_cls = value.__class__
        key = (orig_cls, cls)
        new_cls = _CLASS_LOOKUP.get(key)
        if new_cls is None:
            new_cls = orig_cls.__class__(orig_cls.__name__, (orig_cls, cls), {})
            _CLASS_LOOKUP[key] = new_cls
        value.__class__ = new_cls
        return value
    except TypeError:
        # Non heap type, look in overloads dict
        for type_, func in _TYPE_OVERLOADS.items():
            # Use type() here, we only need to trigger if it's the exact type,
            # as otherwise we don't need to overload the class
            if type(value) is type_:  # pylint: disable=unidiomatic-typecheck
                return add_class_to_obj(func(value), cls)
        raise
