"""Tests for script/platformio_install_deps.py."""

from argparse import Namespace
import importlib.util
import inspect
from pathlib import Path
import sys
from types import SimpleNamespace
from unittest.mock import patch

import pytest

_SCRIPT = Path(__file__).parents[2] / "script" / "platformio_install_deps.py"


def _load_script():
    spec = importlib.util.spec_from_file_location("platformio_install_deps", _SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    # The real ContentCache would create dirs under the user's core dir
    module.ContentCache = lambda *_: None
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

    @staticmethod
    def _key(spec) -> str:
        return spec if isinstance(spec, str) else str(spec)

    def get_package(self, spec):
        if self._key(spec) in self.installed:
            return SimpleNamespace(path="/tmp/fake-pkg", spec=self._key(spec))
        return None

    def memcache_reset(self) -> None:
        type(self).resets = getattr(type(self), "resets", 0) + 1

    def get_download_dir(self) -> str:
        return "/tmp/fake-downloads"

    def get_tmp_dir(self) -> str:
        return "/tmp/fake-tmp"

    def lock(self) -> None:
        type(self).lock_events.append("lock")

    def unlock(self) -> None:
        type(self).lock_events.append("unlock")

    def _install(self, spec, skip_dependencies, compatibility=None):
        assert skip_dependencies is True
        if self._key(spec) in self.fail:
            raise RuntimeError("boom")
        type(self).calls.append(spec)
        type(self).compat_calls.append((self._key(spec), compatibility))
        type(self).installed = type(self).installed | {self._key(spec)}

    def get_pkg_dependencies(self, pkg):
        return getattr(type(self), "deps", {}).get(pkg.spec)

    @staticmethod
    def dependency_to_spec(dependency):
        from platformio.package.meta import PackageSpec

        return PackageSpec(
            owner=dependency.get("owner"),
            name=dependency.get("name"),
            requirements=dependency.get("version"),
        )


def _reset_fake(**kwargs) -> type:
    # A fresh subclass per test: nothing leaks between tests through the
    # class-level scripted state
    return type(
        "_ScriptedManager",
        (_FakeManager,),
        {
            "installed": kwargs.get("installed", set()),
            "fail": kwargs.get("fail", set()),
            "calls": [],
            "compat_calls": [],
            "lock_events": [],
        },
    )


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

    def get_package(self, spec):
        if spec == "esphome/bad @ 1.0" and getattr(cls, "resets", 0):
            return SimpleNamespace(path="/tmp/torn-pkg", spec=spec)
        return _FakeManager.get_package(self, spec)

    cls.get_package = get_package  # throwaway subclass; nothing to restore
    with patch.object(mod.fs, "rmtree", side_effect=removed.append):
        mod.parallel_install(cls, ["esphome/bad @ 1.0", "esphome/good @ 1.0"])
    assert "esphome/good @ 1.0" in cls.calls
    assert removed == ["/tmp/torn-pkg"]
    out = capsys.readouterr().out
    assert "Pre-install of esphome/bad @ 1.0 failed" in out
    assert "Pre-install failed for 1 of 2 package(s)" in out


def test_parallel_install_runs_dependency_waves() -> None:
    """Dependencies of wave-installed packages install in a second wave,
    deduped by name; name-only platform libs stay with the serial pass."""
    mod = _load_script()
    cls = _reset_fake()
    cls.deps = {
        "esphome/noise-c @ 0.1.21": [
            {"owner": "esphome", "name": "libsodium", "version": "^1.0"},
            {"name": "SPI"},
        ],
        "esphome/wg @ 1.0": [
            {"owner": "esphome", "name": "libsodium", "version": "^1.0"},
        ],
    }
    mod.parallel_install(cls, ["esphome/noise-c @ 0.1.21", "esphome/wg @ 1.0"])
    assert len(cls.calls) == 3  # the shared dep installs exactly once
    assert {mod.spec_key(c) for c in cls.calls} == {"noise-c", "wg", "libsodium"}
    # Wave-1 strings carry no compatibility; the dependency wave does
    compats = dict(cls.compat_calls)
    assert compats["esphome/noise-c @ 0.1.21"] is None
    dep_compat = next(v for k, v in cls.compat_calls if "libsodium" in k)
    assert dep_compat is not None  # mirrors pio's install_dependency


def test_dependency_wave_excludes_url_specs() -> None:
    """A dependency pinned to a URL surfaces as spec.uri; it must stay out
    of the wave like string URL specs do."""
    mod = _load_script()
    cls = _reset_fake()
    cls.deps = {
        "esphome/noise-c @ 0.1.21": [
            {"name": "vendored", "version": "https://github.com/x/y.git"},
        ],
    }
    mod.parallel_install(cls, ["esphome/noise-c @ 0.1.21"])
    assert {mod.spec_key(c) for c in cls.calls} == {"noise-c"}


def test_success_without_package_prints_anomaly(capsys) -> None:
    """An install that reported success but resolves to nothing prints the
    anomaly instead of silently pruning its dependency subtree."""
    mod = _load_script()
    cls = _reset_fake()
    real_get = cls.get_package
    cls.get_package = lambda self, spec: None  # never resolvable
    mod.parallel_install(cls, ["esphome/ghost @ 1.0"])
    cls.get_package = real_get
    assert "not resolvable afterwards" in capsys.readouterr().out


def test_wave_limit_prints_truncation(capsys) -> None:
    """Hitting the cycle backstop announces what is left to pkg install."""
    mod = _load_script()
    cls = _reset_fake()
    cls.deps = {
        "esphome/noise-c @ 0.1.21": [
            {"owner": "esphome", "name": "libsodium", "version": "^1.0"},
        ],
    }
    prior = {f"seen{i}" for i in range(200)}
    mod.parallel_install(cls, ["esphome/noise-c @ 0.1.21"], prior)
    out = capsys.readouterr().out
    assert "Dependency wave limit reached; 1 spec(s) left to pkg install" in out


def test_failed_cleanup_fails_the_build(tmp_path: Path) -> None:
    """A torn destination still on disk after rmtree must fail the build:
    fs.rmtree never raises (its onexc handler prints), so only the
    destination's absence proves the cleanup worked."""
    mod = _load_script()
    cls = _reset_fake(fail={"esphome/bad @ 1.0"})
    torn = tmp_path / "torn-pkg"
    torn.mkdir()

    def get_package(self, spec):
        if getattr(cls, "resets", 0):
            return SimpleNamespace(path=str(torn), spec=spec)
        return None

    cls.get_package = get_package  # throwaway subclass; nothing to restore
    with (
        patch.object(mod.fs, "rmtree", lambda path: None),  # onexc swallowed
        pytest.raises(mod.CleanupError, match="could not remove"),
    ):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert cls.lock_events == ["lock", "unlock"]  # still released


def test_api_break_cleans_before_surfacing(tmp_path: Path) -> None:
    """An AttributeError from _install still removes the torn destination
    before propagating to the logged serial fallback."""
    mod = _load_script()
    cls = _reset_fake()
    torn = tmp_path / "torn-pkg"
    torn.mkdir()

    def bad_install(self, spec, skip_dependencies, compatibility=None):
        raise AttributeError("_install went away")

    def get_package(self, spec):
        if getattr(cls, "resets", 0):
            return SimpleNamespace(path=str(torn), spec=spec)
        return None

    cls._install = bad_install
    cls.get_package = get_package
    removed: list[str] = []

    def fake_rmtree(path):
        removed.append(path)
        Path(path).rmdir()

    with (
        patch.object(mod.fs, "rmtree", fake_rmtree),
        pytest.raises(AttributeError),
    ):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert removed == [str(torn)]


def test_unverifiable_torn_destination_fails_the_build() -> None:
    """An inspect that keeps failing cannot prove the destination is
    clean; the serial pass would trust it, so the build fails."""
    mod = _load_script()
    cls = _reset_fake(fail={"esphome/bad @ 1.0"})

    def bad_reset(self):
        raise OSError("scan broken")

    cls.memcache_reset = bad_reset
    with (
        patch.object(mod.time, "sleep", lambda s: None),
        pytest.raises(mod.CleanupError, match="could not verify"),
    ):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])


def test_transient_inspect_failure_retries(capsys) -> None:
    """A transient read of another worker's copy is retried, not fatal."""
    mod = _load_script()
    cls = _reset_fake(fail={"esphome/bad @ 1.0"})
    attempts: list[int] = []

    def flaky_reset(self):
        attempts.append(1)
        if len(attempts) < 2:
            raise OSError("mid-copy read")

    cls.memcache_reset = flaky_reset
    with patch.object(mod.time, "sleep", lambda s: None):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert len(attempts) >= 2
    assert "No resolvable destination to clean" in capsys.readouterr().out


def test_unresolvable_torn_destination_is_printed(capsys) -> None:
    """A failed install with no resolvable package prints, so an invisible
    torn directory is at least traceable."""
    mod = _load_script()
    cls = _reset_fake(fail={"esphome/bad @ 1.0"})
    mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert "No resolvable destination to clean" in capsys.readouterr().out


def test_main_cleanup_error_fails_before_generic_fallback(tmp_path: Path) -> None:
    """A CleanupError must escape main's serial fallback: the clause order
    decides whether a stuck torn package fails the image build."""
    mod = _load_script()
    ini = tmp_path / "platformio.ini"
    ini.write_text("[env:t]\nlib_deps =\n    esphome/x @ 1.0\n")
    with (
        patch.object(
            mod, "parallel_install", side_effect=mod.CleanupError("stuck torn pkg")
        ),
        patch.object(mod.subprocess, "check_call"),
        patch.object(sys, "argv", ["platformio_install_deps.py", str(ini), "-l"]),
        pytest.raises(mod.CleanupError),
    ):
        mod.main()


def test_main_generic_failure_still_runs_serial_pass(tmp_path: Path) -> None:
    """A non-CleanupError wave failure prints, dumps the traceback, and
    still reaches the authoritative serial pass with the pinned cwd."""
    mod = _load_script()
    ini = tmp_path / "platformio.ini"
    ini.write_text("[env:t]\nlib_deps =\n    esphome/x @ 1.0\n")
    with (
        patch.object(mod, "parallel_install", side_effect=RuntimeError("boom")),
        patch.object(mod.subprocess, "check_call") as mock_call,
        patch.object(sys, "argv", ["platformio_install_deps.py", str(ini), "-l"]),
    ):
        mod.main()
    mock_call.assert_called_once()
    args, kwargs = mock_call.call_args
    assert args[0][:4] == ["platformio", "pkg", "install", "-g"]
    assert "esphome/x @ 1.0" in args[0]
    assert kwargs["cwd"] == Path.cwd()


def test_content_cache_creates_its_dir(tmp_path: Path, monkeypatch) -> None:
    """The cold-cache hardening relies on ContentCache.__init__ creating
    the namespace dir; pin the side effect, not mere callability."""
    from platformio.cache import ContentCache

    monkeypatch.setenv("PLATFORMIO_CACHE_DIR", str(tmp_path / "cache"))
    ContentCache("http")
    assert (tmp_path / "cache" / "http").is_dir()


def test_unresolvable_spec_stays_out_of_the_wave(capsys) -> None:
    """A spec with no derivable name is left to the serial pass; a raw
    string key would break the one-per-destination dedupe."""
    mod = _load_script()
    cls = _reset_fake()
    from platformio.package.meta import PackageSpec

    nameless = PackageSpec(requirements="^1.0")
    mod.parallel_install(cls, [nameless])
    assert cls.calls == []
    assert "Skipping unresolvable spec" in capsys.readouterr().out


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
    from platformio import fs
    from platformio.package.manager._install import PackageManagerInstallMixin
    from platformio.package.manager.base import BasePackageManager
    from platformio.package.manager.library import LibraryPackageManager
    from platformio.package.manager.tool import ToolPackageManager
    from platformio.package.meta import PackageCompatibility, PackageItem, PackageSpec

    # The script calls these positionally; pin the positions, not just
    # membership, so a parameter reorder trips the wire too
    params = inspect.signature(PackageManagerInstallMixin._install).parameters
    assert list(params)[1] == "spec"
    assert "skip_dependencies" in params
    assert "compatibility" in params
    for cls in (ToolPackageManager, LibraryPackageManager):
        assert list(inspect.signature(cls.__init__).parameters)[1] == "package_dir"
    for name in (
        "lock",
        "unlock",
        "get_package",
        "memcache_reset",
        "get_pkg_dependencies",
        "dependency_to_spec",
        "get_download_dir",
        "get_tmp_dir",
    ):
        assert callable(getattr(BasePackageManager, name))
    assert PackageSpec("owner/name @ ^1.0").name == "name"
    # The failure-cleanup path degrades to a single line if these vanish
    assert callable(fs.rmtree)
    assert PackageItem("pkg-dir").path == "pkg-dir"
    assert callable(PackageCompatibility.from_dependency)
