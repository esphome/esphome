from __future__ import print_function

import errno
import logging
import os
import socket
import subprocess
import ctypes

from ctypes import wintypes
from esphome.py_compat import char_to_byte, text_type
from esphome.zeroconf import Zeroconf

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
    p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    rc = p.returncode
    return rc, stdout, stderr


def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise


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

    try:
        ip = socket.gethostbyname(host)
    except socket.error as err:
        if host.endswith('.local'):
            ip = _resolve_with_zeroconf(host)
        else:
            raise EsphomeError("Error resolving IP address: {}".format(err))

    return ip


def get_bool_env(var, default=False):
    return bool(os.getenv(var, default))


def is_hassio():
    return get_bool_env('ESPHOME_IS_HASSIO')


def symlink(src, dst):
    if hasattr(os, 'symlink'):
        os.symlink(src, dst)
    else:
        csl = ctypes.windll.kernel32.CreateSymbolicLinkW
        csl.argtypes = (ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint32)
        csl.restype = ctypes.c_ubyte
        flags = 1 if os.path.isdir(src) else 0
        if csl(dst, src, flags) == 0:
            error = ctypes.WinError()
            if error.winerror == 1314 and error.errno == 22:
                from esphome.core import EsphomeError
                raise EsphomeError("Cannot create symlink from '%s' to '%s'. Try running tool \
with elevated privileges" % (src, dst))
            raise error


#https://stackoverflow.com/questions/27972776/having-trouble-implementing-a-readlink-function

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

FILE_READ_ATTRIBUTES = 0x0080
OPEN_EXISTING = 3
FILE_FLAG_OPEN_REPARSE_POINT = 0x00200000
FILE_FLAG_BACKUP_SEMANTICS = 0x02000000
FILE_ATTRIBUTE_REPARSE_POINT = 0x0400

IO_REPARSE_TAG_MOUNT_POINT = 0xA0000003
IO_REPARSE_TAG_SYMLINK = 0xA000000C
FSCTL_GET_REPARSE_POINT = 0x000900A8
MAXIMUM_REPARSE_DATA_BUFFER_SIZE = 0x4000

LPDWORD = ctypes.POINTER(wintypes.DWORD)
LPWIN32_FIND_DATA = ctypes.POINTER(wintypes.WIN32_FIND_DATAW)
INVALID_HANDLE_VALUE = wintypes.HANDLE(-1).value

def IsReparseTagNameSurrogate(tag):
    return bool(tag & 0x20000000)

def _check_invalid_handle(result, func, args):
    if result == INVALID_HANDLE_VALUE:
        raise ctypes.WinError(ctypes.get_last_error())
    return args

def _check_bool(result, func, args):
    if not result:
        raise ctypes.WinError(ctypes.get_last_error())
    return args

kernel32.FindFirstFileW.errcheck = _check_invalid_handle
kernel32.FindFirstFileW.restype = wintypes.HANDLE
kernel32.FindFirstFileW.argtypes = (
    wintypes.LPCWSTR,  # _In_  lpFileName
    LPWIN32_FIND_DATA) # _Out_ lpFindFileData

kernel32.FindClose.argtypes = (
    wintypes.HANDLE,) # _Inout_ hFindFile

kernel32.CreateFileW.errcheck = _check_invalid_handle
kernel32.CreateFileW.restype = wintypes.HANDLE
kernel32.CreateFileW.argtypes = (
    wintypes.LPCWSTR, # _In_     lpFileName
    wintypes.DWORD,   # _In_     dwDesiredAccess
    wintypes.DWORD,   # _In_     dwShareMode
    wintypes.LPVOID,  # _In_opt_ lpSecurityAttributes
    wintypes.DWORD, # _In_     dwCreationDisposition
    wintypes.DWORD, # _In_     dwFlagsAndAttributes
    wintypes.HANDLE)  # _In_opt_ hTemplateFile

kernel32.CloseHandle.argtypes = (
    wintypes.HANDLE,) # _In_ hObject

kernel32.DeviceIoControl.errcheck = _check_bool
kernel32.DeviceIoControl.argtypes = (
    wintypes.HANDLE,  # _In_        hDevice
    wintypes.DWORD,   # _In_        dwIoControlCode
    wintypes.LPVOID,  # _In_opt_    lpInBuffer
    wintypes.DWORD,   # _In_        nInBufferSize
    wintypes.LPVOID,  # _Out_opt_   lpOutBuffer
    wintypes.DWORD,   # _In_        nOutBufferSize
    LPDWORD,          # _Out_opt_   lpBytesReturned
    wintypes.LPVOID)  # _Inout_opt_ lpOverlapped

class REPARSE_DATA_BUFFER(ctypes.Structure):
    class ReparseData(ctypes.Union):
        class LinkData(ctypes.Structure):
            _fields_ = (('SubstituteNameOffset', wintypes.USHORT),
                        ('SubstituteNameLength', wintypes.USHORT),
                        ('PrintNameOffset', wintypes.USHORT),
                        ('PrintNameLength', wintypes.USHORT))
            @property
            def PrintName(self):
                dt = wintypes.WCHAR * (self.PrintNameLength //
                                       ctypes.sizeof(wintypes.WCHAR))
                name = dt.from_address(ctypes.addressof(self.PathBuffer) +
                                       self.PrintNameOffset).value
                if name.startswith(r'\??'):
                    name = r'\\?' + name[3:] # NT => Windows
                return name
        class SymbolicLinkData(LinkData):
            _fields_ = (('Flags', wintypes.ULONG), ('PathBuffer', wintypes.BYTE * 0))
        class MountPointData(LinkData):
            _fields_ = (('PathBuffer', wintypes.BYTE * 0),)
        class GenericData(ctypes.Structure):
            _fields_ = (('DataBuffer', wintypes.BYTE * 0),)
        _fields_ = (('SymbolicLinkReparseBuffer', SymbolicLinkData),
                    ('MountPointReparseBuffer', MountPointData),
                    ('GenericReparseBuffer', GenericData))
    _fields_ = (('ReparseTag', wintypes.ULONG),
                ('ReparseDataLength', wintypes.USHORT),
                ('Reserved', wintypes.USHORT),
                ('ReparseData', ReparseData))
    _anonymous_ = ('ReparseData',)

def islink(path):
    if hasattr(os, 'symlink'):
        return os.path.islink(path)
    if not os.path.isdir(path):
        return False
    data = wintypes.WIN32_FIND_DATAW()
    kernel32.FindClose(kernel32.FindFirstFileW(path, ctypes.byref(data)))
    if not data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT:
        return False
    return IsReparseTagNameSurrogate(data.dwReserved0)

def readlink(path):
    if hasattr(os, 'symlink'):
        return os.readlink(path)
    n = wintypes.DWORD()
    buf = (wintypes.BYTE * MAXIMUM_REPARSE_DATA_BUFFER_SIZE)()
    flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS
    handle = kernel32.CreateFileW(path, FILE_READ_ATTRIBUTES, 0, None,
                                  OPEN_EXISTING, flags, None)
    try:
        kernel32.DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, None, 0,
                                 buf, ctypes.sizeof(buf), ctypes.byref(n), None)
    finally:
        kernel32.CloseHandle(handle)
    rb = REPARSE_DATA_BUFFER.from_buffer(buf)
    tag = rb.ReparseTag
    if tag == IO_REPARSE_TAG_SYMLINK:
        return rb.SymbolicLinkReparseBuffer.PrintName
    if tag == IO_REPARSE_TAG_MOUNT_POINT:
        return rb.MountPointReparseBuffer.PrintName
    if not IsReparseTagNameSurrogate(tag):
        raise ValueError("not a link")
    raise ValueError("unsupported reparse tag: %d" % tag)

def unlink(path):
    if hasattr(os, 'symlink'):
        return os.unlink(path)
    return os.rmdir(path)

def pathIsLink(path):
    if hasattr(os, 'symlink'):
        return os.path.islink(path)
    else:
        gfa = ctypes.windll.kernel32.GetFileAttributesW
        gfa.argtypes = (ctypes.c_wchar_p,)
        gfa.restype = ctypes.c_ulonglong
        gfaRet = gfa(path)
        if gfaRet == 0:
            raise ctypes.WinError()
        return bool(os.path.isdir(path) and (gfaRet & FILE_ATTRIBUTE_REPARSE_POINT))
