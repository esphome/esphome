"""Tests for esphome.platformio.runner."""

from __future__ import annotations

from collections.abc import Callable
import io
from pathlib import Path
import runpy
import sys
from types import ModuleType, SimpleNamespace
from unittest.mock import Mock

import platformio
from platformio.registry.client import RegistryClient
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
    monkeypatch.setattr(runner, "patch_registry_private_packages", lambda: None)

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


def test_main_applies_registry_private_packages_patch(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The probe is patched before PlatformIO runs."""
    order: list[str] = []
    _prepare_main(monkeypatch, lambda: order.append("pio") or 0)
    monkeypatch.setattr(
        runner, "patch_registry_private_packages", lambda: order.append("patch")
    )

    assert runner.main() == 0
    assert order == ["patch", "pio"]


def test_ensure_http_cache_dir_is_idempotent(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """The dir exists before PlatformIO's non-exist_ok makedirs, and a
    second caller (another thread or build) does not trip over it."""
    from platformio.project import helpers

    monkeypatch.setattr(helpers, "get_project_cache_dir", lambda: str(tmp_path / "c"))

    runner.ensure_http_cache_dir()
    assert (tmp_path / "c" / "http").is_dir()
    runner.ensure_http_cache_dir()


# Snapshot PlatformIO's own probe at import, before any test can patch it
_PIO_PROBE = RegistryClient.__dict__["allowed_private_packages"]


def test_patch_registry_private_packages_skips_account_probe(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Answers False without touching the account client."""
    from platformio.account.client import AccountClient

    monkeypatch.setattr(RegistryClient, "allowed_private_packages", _PIO_PROBE)
    monkeypatch.setattr(
        AccountClient,
        "get_account_info",
        Mock(side_effect=AssertionError("account probe must not run")),
    )

    runner.patch_registry_private_packages()

    assert RegistryClient.allowed_private_packages() is False
    assert RegistryClient().allowed_private_packages() is False


def test_patch_file_downloader_retries_when_session_cleanup_fails(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A failing close of the old session must not stop the retry."""
    package_exception = type("PackageException", (Exception,), {})
    attempts: list[int] = []

    def flaky_init(self: object, *args: object, **kwargs: object) -> None:
        attempts.append(1)
        if len(attempts) < 2:
            raise package_exception("502")

    downloader = type("FileDownloader", (), {"__init__": flaky_init})
    monkeypatch.setitem(
        sys.modules,
        "platformio.package.download",
        SimpleNamespace(FileDownloader=downloader),
    )
    monkeypatch.setitem(
        sys.modules,
        "platformio.package.exception",
        SimpleNamespace(PackageException=package_exception),
    )
    monkeypatch.setattr(runner.time, "sleep", lambda _delay: None)

    runner.patch_file_downloader()

    instance = object.__new__(downloader)
    instance._http_response = Mock(close=Mock(side_effect=OSError("boom")))
    downloader.__init__(instance)

    assert len(attempts) == 2
    instance._http_response.close.assert_called_once()


def test_runner_runs_main_when_executed_as_a_script(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Running the file as ``__main__`` exits with PlatformIO's return code."""
    monkeypatch.setattr(sys, "argv", ["runner", "run"])
    monkeypatch.setattr(sys, "stdout", io.StringIO())
    monkeypatch.setattr(sys, "stderr", io.StringIO())
    # The script re-imports and applies the PlatformIO patches on its own
    # module namespace, so give it stand-ins rather than the real internals.
    monkeypatch.setitem(sys.modules, "platformio.run", Mock())
    monkeypatch.setitem(
        sys.modules,
        "platformio.package.download",
        SimpleNamespace(FileDownloader=type("FileDownloader", (), {})),
    )
    monkeypatch.setitem(
        sys.modules,
        "platformio.package.exception",
        SimpleNamespace(PackageException=Exception),
    )
    monkeypatch.setitem(
        sys.modules,
        "platformio.registry.client",
        SimpleNamespace(RegistryClient=type("RegistryClient", (), {})),
    )
    pio_main = SimpleNamespace(main=lambda: 3)
    monkeypatch.setitem(sys.modules, "platformio.__main__", pio_main)
    monkeypatch.setattr(platformio, "__main__", pio_main, raising=False)

    with pytest.raises(SystemExit) as exc_info:
        runpy.run_path(str(Path(runner.__file__)), run_name="__main__")

    assert exc_info.value.code == 3
