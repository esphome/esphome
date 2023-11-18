import logging
import os
import tempfile
from pathlib import Path

_LOGGER = logging.getLogger(__name__)


# from https://github.com/home-assistant/core/blob/dev/homeassistant/util/file.py
def write_utf8_file(
    filename: Path,
    utf8_data: str,
    private: bool = False,
) -> None:
    """Write a file and rename it into place.

    Writes all or nothing.
    """

    tmp_filename = ""
    try:
        # Modern versions of Python tempfile create this file with mode 0o600
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=os.path.dirname(filename), delete=False
        ) as fdesc:
            fdesc.write(utf8_data)
            tmp_filename = fdesc.name
            if not private:
                os.fchmod(fdesc.fileno(), 0o644)
        os.replace(tmp_filename, filename)
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
