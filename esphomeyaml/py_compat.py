import sys

PYTHON_MAJOR = sys.version_info[0]
IS_PY2 = PYTHON_MAJOR == 2
IS_PY3 = PYTHON_MAJOR == 3


# pylint: disable=no-else-return
def safe_input(line):
    if IS_PY2:
        return raw_input(line)
    else:
        return input(line)


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


def byte(val):
    if IS_PY2:
        return chr(val)
    else:
        return bytes([val])


def char(val):
    if IS_PY2:
        return ord(val)
    else:
        return val


def format_bytes(val):
    if IS_PY2:
        return ' '.join('{:02X}'.format(ord(x)) for x in val)
    else:
        return ' '.join('{:02X}'.format(x) for x in val)
