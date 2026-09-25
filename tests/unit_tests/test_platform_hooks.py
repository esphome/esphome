"""Guard the platform CLI-hook registry in ``esphome.platform_hooks``.

The registry lets the logs/upload fast path skip importing platform
packages that don't provide a hook; these tests fail when a platform
gains or loses a hook without the registry being updated, and pin down
that the fast path really avoids the import.
"""

from __future__ import annotations

import importlib
import logging
from unittest.mock import Mock

import pytest

from esphome import platform_hooks
from esphome.const import PLATFORM_BK72XX, PLATFORM_ESP32, Platform


def test_no_unregistered_platform_exposes_a_hook() -> None:
    """Every platform hook the packages expose must be registered.

    Behavioural on purpose: a hook added as a re-export, an assignment,
    or an ``async def`` is invisible to source scanning but very visible
    to ``hasattr``, and an unregistered hook is silently never called.
    The registered direction is covered by
    test_every_registered_pair_resolves below.
    """
    for platform in frozenset(Platform):
        module = importlib.import_module(f"esphome.components.{platform}")
        for hook, registered in platform_hooks.PLATFORM_HOOKS.items():
            if hasattr(module, hook):
                assert platform in registered, (
                    f"{platform} exposes {hook} but is not registered for it. "
                    "Update esphome/platform_hooks.py."
                )


def test_registered_platform_resolves_hook() -> None:
    hook = platform_hooks.get_platform_hook(PLATFORM_ESP32, "process_stacktrace")
    from esphome.components import esp32

    assert hook is esp32.process_stacktrace


def test_every_registered_pair_resolves() -> None:
    """Each registered platform must actually expose the hook at runtime.

    Text scanning can miss re-exports or decorated definitions; this is
    the behavioural check for the direction that matters when the CLI
    runs.
    """
    for hook, platforms in platform_hooks.PLATFORM_HOOKS.items():
        for platform in platforms:
            assert callable(platform_hooks.get_platform_hook(platform, hook)), (
                f"{platform} is registered for {hook} but does not expose it"
            )


def test_external_platform_falls_back_to_probe(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Out-of-tree target platforms keep working via the dynamic probe."""
    module = type("FakePlatform", (), {"show_logs": staticmethod(lambda *a: True)})
    imported: list[str] = []

    def fake_import(name: str):
        imported.append(name)
        return module

    monkeypatch.setattr(platform_hooks, "import_module", fake_import)
    hook = platform_hooks.get_platform_hook("my_external_chip", "show_logs")
    assert hook is module.show_logs
    assert imported == ["esphome.components.my_external_chip"]


def test_external_platform_missing_module_degrades(
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A warm-cache run may not have the external package importable.

    Skipping a behavior-changing hook is visible at warning; losing
    stacktrace decoding is cosmetic and stays at debug.
    """
    monkeypatch.setattr(
        platform_hooks,
        "import_module",
        Mock(
            side_effect=ModuleNotFoundError(
                "not found", name="esphome.components.my_external_chip"
            )
        ),
    )
    assert platform_hooks.get_platform_hook("my_external_chip", "show_logs") is None
    assert "not importable" in caplog.text
    assert any(r.levelname == "WARNING" for r in caplog.records)

    caplog.clear()
    assert (
        platform_hooks.get_platform_hook("my_external_chip", "process_stacktrace")
        is None
    )
    assert not any(r.levelname == "WARNING" for r in caplog.records)


def test_external_platform_without_hook_logs_debug(
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The common no-hook case stays quiet but diagnosable."""
    caplog.set_level("DEBUG", logger="esphome.platform_hooks")
    module = type("ExternalPlatform", (), {})  # imports fine, no hook
    monkeypatch.setattr(platform_hooks, "import_module", Mock(return_value=module))
    assert platform_hooks.get_platform_hook("my_external_chip", "show_logs") is None
    assert "does not expose" in caplog.text
    assert not any(r.levelname == "WARNING" for r in caplog.records)


def test_stale_registry_entry_warns(
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A vendored tree where a registered hook vanished must say so."""
    module = type("StalePlatform", (), {})  # registered but no hook
    monkeypatch.setattr(platform_hooks, "import_module", Mock(return_value=module))
    assert platform_hooks.get_platform_hook("nrf52", "show_logs") is None
    assert "no longer exposes it" in caplog.text


def test_external_platform_broken_dependency_raises(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A missing dependency inside the external package must surface."""
    monkeypatch.setattr(
        platform_hooks,
        "import_module",
        Mock(side_effect=ModuleNotFoundError("not found", name="some_missing_dep")),
    )
    with pytest.raises(ModuleNotFoundError, match="not found"):
        platform_hooks.get_platform_hook("my_external_chip", "show_logs")


def test_lookup_miss_does_not_import_platform_package(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The whole point: probing a platform without hooks must not import it."""
    monkeypatch.setattr(
        platform_hooks,
        "import_module",
        Mock(side_effect=AssertionError("platform package imported on registry miss")),
    )
    assert platform_hooks.get_platform_hook(PLATFORM_ESP32, "show_logs") is None


def test_get_stacktrace_handler_resolves_registered_platform() -> None:
    hook = platform_hooks.get_stacktrace_handler(PLATFORM_ESP32)
    from esphome.components import esp32

    assert hook is esp32.process_stacktrace


def test_get_stacktrace_handler_reports_missing_analyzer(
    caplog: pytest.LogCaptureFixture,
) -> None:
    caplog.set_level("INFO", logger="esphome.platform_hooks")
    assert platform_hooks.get_stacktrace_handler(PLATFORM_BK72XX) is None
    assert "no compatible analyzer" in caplog.text
    # A capability gap is ordinary; it must not warn.
    assert not any(r.levelno >= logging.WARNING for r in caplog.records)


def test_get_stacktrace_handler_reports_import_failure(
    monkeypatch: pytest.MonkeyPatch,
    caplog: pytest.LogCaptureFixture,
) -> None:
    monkeypatch.setattr(
        platform_hooks,
        "import_module",
        Mock(side_effect=ImportError("broken install")),
    )
    assert platform_hooks.get_stacktrace_handler(PLATFORM_ESP32) is None
    assert "failed to import: broken install" in caplog.text
    # A broken install is a real breakage; it must warn, not inform.
    assert any(r.levelno == logging.WARNING for r in caplog.records)
