import logging
import os
from pathlib import Path
import tempfile

_LOGGER = logging.getLogger(__name__)


def write_utf8_file(
    filename: Path,
    utf8_str: str,
    private: bool = False,
) -> None:
    """Write a file and rename it into place.

    Writes all or nothing.
    """
    write_file(filename, utf8_str.encode("utf-8"), private)


# from https://github.com/home-assistant/core/blob/dev/homeassistant/util/file.py
def write_file(
    filename: Path,
    utf8_data: bytes,
    private: bool = False,
) -> None:
    """Write a file and rename it into place.

    Writes all or nothing.
    """

    tmp_filename = ""
    missing_fchmod = False
    try:
        # Modern versions of Python tempfile create this file with mode 0o600
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=os.path.dirname(filename), delete=False
        ) as fdesc:
            fdesc.write(utf8_data)
            tmp_filename = fdesc.name
            if not private:
                try:
                    os.fchmod(fdesc.fileno(), 0o644)
                except AttributeError:
                    # os.fchmod is not available on Windows
                    missing_fchmod = True

        os.replace(tmp_filename, filename)
        if missing_fchmod:
            os.chmod(filename, 0o644)
    finally:
        if os.path.exists(tmp_filename):
            try:
                os.remove(tmp_filename)
            except OSError as err:
                # If we are cleaning up then something else went wrong, so
                # we should suppress likely follow-on errors in the cleanup
                _LOGGER.error(
                    "File replacement cleanup failed for %s while saving %s: %s",
                    tmp_filename,
                    filename,
                    err,
                )
