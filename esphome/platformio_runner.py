"""Subprocess entry point that applies ESPHome's PlatformIO patches.

Invoked via ``python -m esphome.platformio_runner`` instead of
``python -m platformio`` so that the patches (incremental rebuild
preservation, download retries) apply inside the subprocess. Running
PlatformIO in a subprocess keeps its ``sys.path`` mutations and other
global state from leaking into the ESPHome process.
"""

from __future__ import annotations

import logging
from pathlib import Path
import sys
import time
from typing import Any

_LOGGER = logging.getLogger(__name__)


def patch_structhash() -> None:
    """Avoid full rebuilds when files are added or removed.

    PlatformIO clears the build dir whenever its structure hash changes.
    We replace that with an mtime check against ``platformio.ini`` so
    incremental builds are preserved unless the project config changed.
    """
    from platformio.run import cli, helpers

    def patched_clean_build_dir(build_dir, *_args):
        from platformio import fs
        from platformio.project.helpers import get_project_dir

        platformio_ini = Path(get_project_dir()) / "platformio.ini"
        build_dir = Path(build_dir)

        if (
            build_dir.is_dir()
            and platformio_ini.stat().st_mtime > build_dir.stat().st_mtime
        ):
            fs.rmtree(build_dir)

        if not build_dir.is_dir():
            build_dir.mkdir(parents=True)

    helpers.clean_build_dir = patched_clean_build_dir
    cli.clean_build_dir = patched_clean_build_dir


def patch_file_downloader() -> None:
    """Retry PlatformIO package downloads with exponential backoff.

    PlatformIO's ``FileDownloader`` uses an ``HTTPSession`` without built-in
    retry for 502/503 errors. We wrap ``__init__`` to retry on
    ``PackageException`` and close the session between attempts so a new
    TCP connection can route to a different CDN edge node.
    """
    from platformio.package.download import FileDownloader
    from platformio.package.exception import PackageException

    if getattr(FileDownloader.__init__, "_esphome_patched", False):
        return

    original_init = FileDownloader.__init__

    def patched_init(self, *args: Any, **kwargs: Any) -> None:
        max_retries = 5

        for attempt in range(max_retries):
            try:
                original_init(self, *args, **kwargs)
                return
            except PackageException as e:
                if attempt < max_retries - 1:
                    delay = 2 ** (attempt + 1)
                    _LOGGER.warning(
                        "Package download failed: %s. "
                        "Retrying in %d seconds... (attempt %d/%d)",
                        str(e),
                        delay,
                        attempt + 1,
                        max_retries,
                    )
                    # pylint: disable=protected-access,broad-except
                    try:
                        if (
                            hasattr(self, "_http_response")
                            and self._http_response is not None
                        ):
                            self._http_response.close()
                        if hasattr(self, "_http_session"):
                            self._http_session.close()
                    except Exception:
                        pass
                    # pylint: enable=protected-access,broad-except
                    time.sleep(delay)
                else:
                    raise

    patched_init._esphome_patched = True  # type: ignore[attr-defined]  # pylint: disable=protected-access
    FileDownloader.__init__ = patched_init


def main() -> int:
    patch_structhash()
    patch_file_downloader()

    # Wrap stdout/stderr with RedirectText before PlatformIO runs:
    #
    # 1. RedirectText.isatty() unconditionally returns True. Click, tqdm, and
    #    PlatformIO's own progress-bar code check ``stream.isatty()`` to
    #    decide whether to emit TTY-format output (``\r`` cursor moves, ANSI
    #    colors, fancy progress bars). With the wrapper in place they always
    #    emit TTY format, even when our real stdout is a pipe to the parent
    #    process. Downstream consumers (local terminals and the Home
    #    Assistant dashboard log viewer) render the TTY control sequences
    #    correctly, so the user sees real progress bars.
    #
    # 2. FILTER_PLATFORMIO_LINES is applied inside RedirectText.write() in
    #    this subprocess, so noisy PlatformIO output is dropped before it
    #    ever leaves the runner. This replaces the parent-side filtering
    #    that was lost when we switched from in-process to subprocess — the
    #    parent's ``subprocess.run`` uses ``.fileno()`` on RedirectText and
    #    bypasses its ``write()`` path entirely.
    #
    # Filtering is disabled when the user passed -v / --verbose to
    # ``esphome compile``, preserving the previous in-process behavior where
    # verbose mode let all PlatformIO output through unfiltered.
    from esphome.platformio_api import FILTER_PLATFORMIO_LINES
    from esphome.util import RedirectText

    is_verbose = any(arg in ("-v", "--verbose") for arg in sys.argv[1:])
    filter_lines = None if is_verbose else FILTER_PLATFORMIO_LINES

    sys.stdout = RedirectText(sys.stdout, filter_lines=filter_lines)
    sys.stderr = RedirectText(sys.stderr, filter_lines=filter_lines)

    import platformio.__main__

    return platformio.__main__.main() or 0


if __name__ == "__main__":
    sys.exit(main())
