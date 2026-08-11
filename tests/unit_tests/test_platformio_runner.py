"""Tests for esphome.platformio.runner."""

from __future__ import annotations

from collections.abc import Callable
import io
import sys
from types import ModuleType

import pytest

from esphome.platformio import runner


def _prepare_main(
    monkeypatch: pytest.MonkeyPatch, pio_main: Callable[[], int]
) -> io.BytesIO:
    """Point ``runner.main()`` at a fake PlatformIO with a fake stdout.

    The real ``main`` patches PlatformIO internals and then hands control to
    it; both are stubbed out so only the stream wrapping is exercised. The
    fake stdout is block buffered like a pipe, so the caller can see what
    actually left the wrapper.
    """
    buf = io.BytesIO()
    stream = io.TextIOWrapper(buf, encoding="utf-8", newline="\n", line_buffering=False)

    monkeypatch.setattr(sys, "argv", ["pio", "run"])
    monkeypatch.setattr(sys, "stdout", stream)
    monkeypatch.setattr(sys, "stderr", stream)
    monkeypatch.setattr(runner, "patch_structhash", lambda: None)
    monkeypatch.setattr(runner, "patch_file_downloader", lambda: None)

    platformio = ModuleType("platformio")
    platformio_main = ModuleType("platformio.__main__")
    platformio_main.main = pio_main  # type: ignore[attr-defined]
    platformio.__main__ = platformio_main  # type: ignore[attr-defined]
    monkeypatch.setitem(sys.modules, "platformio", platformio)
    monkeypatch.setitem(sys.modules, "platformio.__main__", platformio_main)

    return buf


def test_main_drains_a_partial_line_on_a_clean_run(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A build ending mid line still shows that line."""

    def pio_main() -> int:
        print("Linking .pioenvs/firmware.elf\n", end="")
        print("Building took 12.4 seconds", end="")
        return 0

    buf = _prepare_main(monkeypatch, pio_main)

    assert runner.main() == 0
    assert buf.getvalue().decode("utf-8") == (
        "Linking .pioenvs/firmware.elf\nBuilding took 12.4 seconds\n"
    )


def test_main_drains_when_platformio_exits_early(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Leaving through ``sys.exit`` still drains, because it runs in a finally."""

    def pio_main() -> int:
        print("*** [.pioenvs/firmware.elf] Error 1", end="")
        sys.exit(1)

    buf = _prepare_main(monkeypatch, pio_main)

    with pytest.raises(SystemExit) as excinfo:
        runner.main()

    assert excinfo.value.code == 1
    assert buf.getvalue().decode("utf-8") == "*** [.pioenvs/firmware.elf] Error 1\n"


def test_main_still_filters_a_drained_partial_line(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Releasing a held line does not smuggle noise past the filter."""

    def pio_main() -> int:
        # Matches FILTER_PLATFORMIO_LINES, and arrives without a terminator.
        print("Verbose mode can be enabled via `-v, --verbose` option", end="")
        return 0

    buf = _prepare_main(monkeypatch, pio_main)

    assert runner.main() == 0
    assert buf.getvalue() == b""
