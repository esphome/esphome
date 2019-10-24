from __future__ import print_function

import codecs

import logging
import os

from esphome.py_compat import char_to_byte, text_type, IS_PY2, encode_text

_LOGGER = logging.getLogger(__name__)


def ensure_unique_string(preferred_string, current_strings):
    test_string = preferred_string
    current_strings_set = set(current_strings)

    tries = 1

    while test_string in current_strings_set:
        tries += 1
        test_string = u"{}_{}".format(preferred_string, tries)

    return test_string


def indent_all_but_first_and_last(text, padding=u'  '):
    lines = text.splitlines(True)
    if len(lines) <= 2:
        return text
    return lines[0] + u''.join(padding + line for line in lines[1:-1]) + lines[-1]


def indent_list(text, padding=u'  '):
    return [padding + line for line in text.splitlines()]


def indent(text, padding=u'  '):
    return u'\n'.join(indent_list(text, padding))


# From https://stackoverflow.com/a/14945195/8924614
def cpp_string_escape(string, encoding='utf-8'):
    def _should_escape(byte):  # type: (int) -> bool
        if not 32 <= byte < 127:
            return True
        if byte in (char_to_byte('\\'), char_to_byte('"')):
            return True
        return False

    if isinstance(string, text_type):
        string = string.encode(encoding)
    result = ''
    for character in string:
        character = char_to_byte(character)
        if _should_escape(character):
            result += '\\%03o' % character
        else:
            result += chr(character)
    return '"' + result + '"'


def color(the_color, message=''):
    from colorlog.escape_codes import escape_codes, parse_colors

    if not message:
        res = parse_colors(the_color)
    else:
        res = parse_colors(the_color) + message + escape_codes['reset']

    return res


def run_system_command(*args):
    import subprocess

    p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    rc = p.returncode
    return rc, stdout, stderr


def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as err:
        import errno
        if err.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            from esphome.core import EsphomeError
            raise EsphomeError(u"Error creating directories {}: {}".format(path, err))


def is_ip_address(host):
    parts = host.split('.')
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
    from esphome.zeroconf import Zeroconf

    try:
        zc = Zeroconf()
    except Exception:
        raise EsphomeError("Cannot start mDNS sockets, is this a docker container without "
                           "host network mode?")
    try:
        info = zc.resolve_host(host + '.')
    except Exception as err:
        raise EsphomeError("Error resolving mDNS hostname: {}".format(err))
    finally:
        zc.close()
    if info is None:
        raise EsphomeError("Error resolving address with mDNS: Did not respond. "
                           "Maybe the device is offline.")
    return info


def resolve_ip_address(host):
    from esphome.core import EsphomeError
    import socket

    errs = []

    if host.endswith('.local'):
        try:
            return _resolve_with_zeroconf(host)
        except EsphomeError as err:
            errs.append(str(err))

    try:
        return socket.gethostbyname(host)
    except socket.error as err:
        errs.append(str(err))
        raise EsphomeError("Error resolving IP address: {}"
                           "".format(', '.join(errs)))


def get_bool_env(var, default=False):
    return bool(os.getenv(var, default))


def is_hassio():
    return get_bool_env('ESPHOME_IS_HASSIO')


def walk_files(path):
    for root, _, files in os.walk(path):
        for name in files:
            yield os.path.join(root, name)


def read_file(path):
    try:
        with codecs.open(path, 'r', encoding='utf-8') as f_handle:
            return f_handle.read()
    except OSError as err:
        from esphome.core import EsphomeError
        raise EsphomeError(u"Error reading file {}: {}".format(path, err))
    except UnicodeDecodeError as err:
        from esphome.core import EsphomeError
        raise EsphomeError(u"Error reading file {}: {}".format(path, err))


def _write_file(path, text):
    import tempfile
    directory = os.path.dirname(path)
    mkdir_p(directory)

    tmp_path = None
    data = encode_text(text)
    try:
        with tempfile.NamedTemporaryFile(mode="wb", dir=directory, delete=False) as f_handle:
            tmp_path = f_handle.name
            f_handle.write(data)
        # Newer tempfile implementations create the file with mode 0o600
        os.chmod(tmp_path, 0o644)
        if IS_PY2:
            if os.path.exists(path):
                os.remove(path)
            os.rename(tmp_path, path)
        else:
            # If destination exists, will be overwritten
            os.replace(tmp_path, path)
    finally:
        if tmp_path is not None and os.path.exists(tmp_path):
            try:
                os.remove(tmp_path)
            except OSError as err:
                _LOGGER.error("Write file cleanup failed: %s", err)


def write_file(path, text):
    try:
        _write_file(path, text)
    except OSError:
        from esphome.core import EsphomeError
        raise EsphomeError(u"Could not write file at {}".format(path))


def write_file_if_changed(path, text):
    src_content = None
    if os.path.isfile(path):
        src_content = read_file(path)
    if src_content != text:
        write_file(path, text)


def copy_file_if_changed(src, dst):
    import shutil
    if file_compare(src, dst):
        return
    mkdir_p(os.path.dirname(dst))
    try:
        shutil.copy(src, dst)
    except OSError as err:
        from esphome.core import EsphomeError
        raise EsphomeError(u"Error copying file {} to {}: {}".format(src, dst, err))


def list_starts_with(list_, sub):
    return len(sub) <= len(list_) and all(list_[i] == x for i, x in enumerate(sub))


def file_compare(path1, path2):
    """Return True if the files path1 and path2 have the same contents."""
    import stat

    try:
        stat1, stat2 = os.stat(path1), os.stat(path2)
    except OSError:
        # File doesn't exist or another error -> not equal
        return False

    if stat.S_IFMT(stat1.st_mode) != stat.S_IFREG or stat.S_IFMT(stat2.st_mode) != stat.S_IFREG:
        # At least one of them is not a regular file (or does not exist)
        return False
    if stat1.st_size != stat2.st_size:
        # Different sizes
        return False

    bufsize = 8*1024
    # Read files in blocks until a mismatch is found
    with open(path1, 'rb') as fh1, open(path2, 'rb') as fh2:
        while True:
            blob1, blob2 = fh1.read(bufsize), fh2.read(bufsize)
            if blob1 != blob2:
                # Different content
                return False
            if not blob1:
                # Reached end
                return True
