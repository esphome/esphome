from __future__ import print_function

import io
import logging
import re
import subprocess
import sys

_LOGGER = logging.getLogger(__name__)


class Registry(dict):
    def register(self, name):
        def decorator(fun):
            self[name] = fun
            return fun

        return decorator


class ServiceRegistry(dict):
    def register(self, name, validator):
        def decorator(fun):
            self[name] = (validator, fun)
            return fun

        return decorator


def safe_print(message=""):
    from esphome.core import CORE

    if CORE.dashboard:
        try:
            message = message.replace('\033', '\\033')
        except UnicodeEncodeError:
            pass

    try:
        print(message)
        return
    except UnicodeEncodeError:
        pass

    try:
        print(message.encode('utf-8', 'backslashreplace'))
    except UnicodeEncodeError:
        try:
            print(message.encode('ascii', 'backslashreplace'))
        except UnicodeEncodeError:
            print("Cannot print line because of invalid locale!")


def shlex_quote(s):
    if not s:
        return u"''"
    if re.search(r'[^\w@%+=:,./-]', s) is None:
        return s

    return u"'" + s.replace(u"'", u"'\"'\"'") + u"'"


class RedirectText(object):
    def __init__(self, out):
        self._out = out

    def __getattr__(self, item):
        return getattr(self._out, item)

    def write(self, s):
        from esphome.core import CORE

        if CORE.dashboard:
            try:
                s = s.replace('\033', '\\033')
            except UnicodeEncodeError:
                pass

        self._out.write(s)

    # pylint: disable=no-self-use
    def isatty(self):
        return True


def run_external_command(func, *cmd, **kwargs):
    def mock_exit(return_code):
        raise SystemExit(return_code)

    orig_argv = sys.argv
    orig_exit = sys.exit  # mock sys.exit
    full_cmd = u' '.join(shlex_quote(x) for x in cmd)
    _LOGGER.info(u"Running:  %s", full_cmd)

    orig_stdout = sys.stdout
    sys.stdout = RedirectText(sys.stdout)
    orig_stderr = sys.stderr
    sys.stderr = RedirectText(sys.stderr)

    capture_stdout = kwargs.get('capture_stdout', False)
    if capture_stdout:
        cap_stdout = sys.stdout = io.BytesIO()

    try:
        sys.argv = list(cmd)
        sys.exit = mock_exit
        return func() or 0
    except KeyboardInterrupt:
        return 1
    except SystemExit as err:
        return err.args[0]
    except Exception as err:  # pylint: disable=broad-except
        _LOGGER.error(u"Running command failed: %s", err)
        _LOGGER.error(u"Please try running %s locally.", full_cmd)
    finally:
        sys.argv = orig_argv
        sys.exit = orig_exit

        sys.stdout = orig_stdout
        sys.stderr = orig_stderr

        if capture_stdout:
            # pylint: disable=lost-exception
            return cap_stdout.getvalue()


def run_external_process(*cmd, **kwargs):
    full_cmd = u' '.join(shlex_quote(x) for x in cmd)
    _LOGGER.info(u"Running:  %s", full_cmd)

    capture_stdout = kwargs.get('capture_stdout', False)
    if capture_stdout:
        sub_stdout = io.BytesIO()
    else:
        sub_stdout = RedirectText(sys.stdout)

    sub_stderr = RedirectText(sys.stderr)

    try:
        return subprocess.call(cmd,
                               stdout=sub_stdout,
                               stderr=sub_stderr)
    except Exception as err:  # pylint: disable=broad-except
        _LOGGER.error(u"Running command failed: %s", err)
        _LOGGER.error(u"Please try running %s locally.", full_cmd)
    finally:
        if capture_stdout:
            # pylint: disable=lost-exception
            return sub_stdout.getvalue()
