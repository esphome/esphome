import functools
import sys

PYTHON_MAJOR = sys.version_info[0]
IS_PY2 = PYTHON_MAJOR == 2
IS_PY3 = PYTHON_MAJOR == 3


# pylint: disable=no-else-return
def safe_input(prompt=None):
    if IS_PY2:
        if prompt is None:
            return raw_input()
        return raw_input(prompt)
    else:
        if prompt is None:
            return input()
        return input(prompt)


if IS_PY2:
    text_type = unicode
    string_types = (str, unicode)
    integer_types = (int, long)
    binary_type = str
else:
    text_type = str
    string_types = (str,)
    integer_types = (int,)
    binary_type = bytes


def byte_to_bytes(val):  # type: (int) -> bytes
    if IS_PY2:
        return chr(val)
    else:
        return bytes([val])


def char_to_byte(val):  # type: (str) -> int
    if IS_PY2:
        if isinstance(val, string_types):
            return ord(val)
        elif isinstance(val, int):
            return val
        else:
            raise ValueError
    else:
        if isinstance(val, str):
            return ord(val)
        elif isinstance(val, int):
            return val
        else:
            raise ValueError


def format_bytes(val):
    if IS_PY2:
        return ' '.join('{:02X}'.format(ord(x)) for x in val)
    else:
        return ' '.join('{:02X}'.format(x) for x in val)


def sort_by_cmp(list_, cmp):
    if IS_PY2:
        list_.sort(cmp=cmp)
    else:
        list_.sort(key=functools.cmp_to_key(cmp))


def indexbytes(buf, i):
    if IS_PY3:
        return buf[i]
    else:
        return ord(buf[i])


if IS_PY2:
    def decode_text(data, encoding='utf-8', errors='strict'):
        # type: (str, str, str) -> unicode
        return unicode(data, encoding=encoding, errors=errors)
else:
    def decode_text(data, encoding='utf-8', errors='strict'):
        # type: (bytes, str, str) -> str
        return data.decode(encoding=encoding, errors=errors)
