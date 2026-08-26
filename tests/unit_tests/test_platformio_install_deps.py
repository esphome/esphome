"""Tests for script/platformio_install_deps.py."""

from argparse import Namespace
import importlib.util
import inspect
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import pytest

_SCRIPT = Path(__file__).parents[2] / "script" / "platformio_install_deps.py"


def _load_script():
    spec = importlib.util.spec_from_file_location("platformio_install_deps", _SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_spec_key_collapses_destinations() -> None:
    """Two specs delivering one package share a directory and one key."""
    mod = _load_script()
    assert mod.spec_key("esphome/noise-c @ 0.1.21") == "noise-c"
    assert mod.spec_key("esphome/noise-c@0.1.21") == "noise-c"
    assert mod.spec_key("ESP32Async/AsyncTCP @ ^3.4.10") == mod.spec_key(
        "esp32async/asynctcp @ 3.5.0"
    )
    url = "https://github.com/pioarduino/platform-espressif32/releases/download/{v}/platform-espressif32.zip"
    assert mod.spec_key(url.format(v="55.03.311")) == mod.spec_key(
        url.format(v="54.03.20")
    )


def test_parse_specs_and_cli_args(tmp_path: Path) -> None:
    """Parsing skips unpinned and interpolated entries; the CLI rebuild
    keeps the original flag pairing."""
    ini = tmp_path / "platformio.ini"
    ini.write_text(
        "[env:a]\n"
        "platform = fake/platform@1\n"
        "lib_deps =\n"
        "    esphome/noise-c @ 0.1.21\n"
        "    ${common.lib_deps}\n"
        "    internal_lib\n"
        "[env:b]\n"
        "lib_deps =\n"
        "    esphome/noise-c @ 0.1.21\n"
    )
    mod = _load_script()
    args = Namespace(libraries=True, platforms=True, tools=False)
    libs, platforms, tools = mod.parse_specs(str(ini), args)
    # exact-string duplicates collapse; distinct version pins survive
    assert libs == ["esphome/noise-c @ 0.1.21"]
    assert platforms == ["fake/platform@1"]
    assert tools == []
    assert mod.build_cli_args(libs, platforms, tools) == [
        "-l",
        "esphome/noise-c @ 0.1.21",
        "-p",
        "fake/platform@1",
    ]


class _FakeManager:
    """Scripted manager_cls: records installs, raises on demand."""

    installed: set = set()
    fail: set = set()
    calls: list = []
    lock_events: list = []

    def __init__(self, package_dir) -> None:
        assert package_dir is None

    def get_package(self, spec):
        if spec in self.installed:
            return SimpleNamespace(path="/tmp/fake-pkg")
        return None

    def memcache_reset(self) -> None:
        type(self).calls.append("memcache_reset")

    def lock(self) -> None:
        type(self).lock_events.append("lock")

    def unlock(self) -> None:
        type(self).lock_events.append("unlock")

    def _install(self, spec, skip_dependencies):
        assert skip_dependencies is True
        if spec in self.fail:
            raise RuntimeError("boom")
        type(self).calls.append(spec)


def _reset_fake(**kwargs) -> type:
    cls = _FakeManager
    cls.installed = kwargs.get("installed", set())
    cls.fail = kwargs.get("fail", set())
    cls.calls = []
    cls.lock_events = []
    return cls


def test_parallel_install_behavior() -> None:
    """Duplicates collapse to one install, installed specs are filtered,
    URL specs stay out of the wave, and the lock wraps the pool."""
    mod = _load_script()
    cls = _reset_fake(installed={"esphome/already @ 1.0"})
    mod.parallel_install(
        cls,
        [
            "esphome/noise-c @ 0.1.21",
            "esphome/noise-c @ 0.1.21",
            "esphome/already @ 1.0",
            "https://x/framework.tar.xz",
        ],
    )
    assert cls.calls == ["esphome/noise-c @ 0.1.21"]
    assert cls.lock_events == ["lock", "unlock"]


def test_parallel_install_failure_cleans_torn_destination(capsys) -> None:
    """A failed install resets the memcache, removes what get_package can
    see, and reports; the others still install."""
    mod = _load_script()
    cls = _reset_fake(fail={"esphome/bad @ 1.0"})

    removed = []
    orig_get = cls.get_package

    def get_package(self, spec):
        if spec == "esphome/bad @ 1.0" and "memcache_reset" in cls.calls:
            return SimpleNamespace(path="/tmp/torn-pkg")
        return orig_get(self, spec)

    cls.get_package = get_package
    try:
        with patch.object(mod.fs, "rmtree", side_effect=removed.append):
            mod.parallel_install(cls, ["esphome/bad @ 1.0", "esphome/good @ 1.0"])
    finally:
        cls.get_package = orig_get
    assert "esphome/good @ 1.0" in cls.calls
    assert removed == ["/tmp/torn-pkg"]
    out = capsys.readouterr().out
    assert "Pre-install of esphome/bad @ 1.0 failed" in out
    assert "Pre-install failed for 1 of 2 package(s)" in out


def test_parallel_install_unlocks_when_pool_fails() -> None:
    mod = _load_script()
    cls = _reset_fake()
    with (
        patch.object(mod, "ThreadPoolExecutor", side_effect=RuntimeError("no")),
        pytest.raises(RuntimeError),
    ):
        mod.parallel_install(cls, ["esphome/a @ 1.0"])
    assert cls.lock_events == ["lock", "unlock"]


def test_parse_specs_unreadable_ini_fails_loudly(tmp_path: Path) -> None:
    """A bad path must not silently build an image with no dependencies."""
    mod = _load_script()
    args = Namespace(libraries=True, platforms=False, tools=False)
    with pytest.raises(SystemExit):
        mod.parse_specs(str(tmp_path / "missing.ini"), args)


def test_platformio_surface_for_install_deps_script() -> None:
    """A PlatformIO bump that changes these members must fail here, not
    silently turn the docker image's parallel preinstall into a no-op."""
    from platformio.package.manager._install import PackageManagerInstallMixin
    from platformio.package.manager.base import BasePackageManager
    from platformio.package.manager.library import LibraryPackageManager
    from platformio.package.manager.tool import ToolPackageManager
    from platformio.package.meta import PackageSpec

    params = inspect.signature(PackageManagerInstallMixin._install).parameters
    assert "spec" in params
    assert "skip_dependencies" in params
    for cls in (ToolPackageManager, LibraryPackageManager):
        assert "package_dir" in inspect.signature(cls.__init__).parameters
    for name in ("lock", "unlock", "get_package"):
        assert callable(getattr(BasePackageManager, name))
    assert PackageSpec("owner/name @ ^1.0").name == "name"
