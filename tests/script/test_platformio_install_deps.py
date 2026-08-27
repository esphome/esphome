"""Tests for script/platformio_install_deps.py."""

from argparse import Namespace
import importlib.util
import inspect
from pathlib import Path
import shutil
import sys
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

from platformio import fs
from platformio.cache import ContentCache
from platformio.exception import InvalidJSONFile
from platformio.package.manager._install import PackageManagerInstallMixin
from platformio.package.manager.base import BasePackageManager
from platformio.package.manager.library import LibraryPackageManager
from platformio.package.manager.tool import ToolPackageManager
from platformio.package.meta import PackageCompatibility, PackageItem, PackageSpec
import pytest
from semantic_version import Version

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
    base_dir: str = ""  # per-test tmp base; set by _reset_fake

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

    @property
    def package_dir(self) -> str:
        return str(Path(type(self).base_dir) / "packages")

    def get_download_dir(self) -> str:
        return str(Path(type(self).base_dir) / "downloads")

    def get_tmp_dir(self) -> str:
        return str(Path(type(self).base_dir) / "tmp")

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
        type(self).installed.add(self._key(spec))  # atomic under the GIL

    def get_pkg_dependencies(self, pkg):
        return getattr(type(self), "deps", {}).get(pkg.spec)

    dependency_to_spec = staticmethod(BasePackageManager.dependency_to_spec)


def _reset_fake(base_dir: str = "", **kwargs) -> type:
    # A fresh subclass per test: nothing leaks between tests through the
    # class-level scripted state
    return type(
        "_ScriptedManager",
        (_FakeManager,),
        {
            "base_dir": base_dir,
            "installed": kwargs.get("installed", set()),
            "fail": kwargs.get("fail", set()),
            "calls": [],
            "compat_calls": [],
            "lock_events": [],
        },
    )


def test_parallel_install_empty_specs_is_a_no_op(tmp_path: Path) -> None:
    mod = _load_script()
    cls = _reset_fake(str(tmp_path))
    mod.parallel_install(cls, [])
    assert cls.calls == [] and cls.lock_events == []


def test_parallel_install_behavior(tmp_path: Path) -> None:
    """Duplicates collapse to one install, installed specs are filtered,
    URL specs stay out of the wave, and the lock wraps the pool."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), installed={"esphome/already @ 1.0"})
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


def test_parallel_install_failure_cleans_torn_destination(
    tmp_path: Path, capsys
) -> None:
    """A failed install resets the memcache, removes what get_package can
    see, and reports; the others still install."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})

    removed = []

    torn = str(tmp_path / "packages" / "torn-pkg")  # never created; only rmtree'd

    def get_package(self, spec):
        if spec == "esphome/bad @ 1.0" and getattr(cls, "resets", 0):
            return SimpleNamespace(path=torn, spec=spec)
        return _FakeManager.get_package(self, spec)

    cls.get_package = get_package  # throwaway subclass; nothing to restore
    with patch.object(mod.fs, "rmtree", side_effect=removed.append):
        mod.parallel_install(cls, ["esphome/bad @ 1.0", "esphome/good @ 1.0"])
    assert "esphome/good @ 1.0" in cls.calls
    assert removed == [torn]
    out = capsys.readouterr().out
    assert "Pre-install of esphome/bad @ 1.0 failed" in out
    assert "Pre-install failed for 1 of 2 package(s)" in out


def test_parallel_install_runs_dependency_waves(tmp_path: Path) -> None:
    """Dependencies of wave-installed packages install in a second wave,
    deduped by name; name-only platform libs stay with the serial pass."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path))
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


def test_dependency_wave_excludes_url_specs(tmp_path: Path) -> None:
    """A dependency pinned to a URL surfaces as spec.uri; it must stay out
    of the wave like string URL specs do."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path))
    cls.deps = {
        "esphome/noise-c @ 0.1.21": [
            {"name": "vendored", "version": "https://github.com/x/y.git"},
        ],
    }
    mod.parallel_install(cls, ["esphome/noise-c @ 0.1.21"])
    assert {mod.spec_key(c) for c in cls.calls} == {"noise-c"}


def test_failed_cleanup_fails_the_build(tmp_path: Path) -> None:
    """A torn destination still on disk after rmtree must fail the build:
    fs.rmtree never raises (its onexc handler prints), so only the
    destination's absence proves the cleanup worked."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    torn = tmp_path / "packages" / "torn-pkg"
    torn.mkdir(parents=True)

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


def test_unverifiable_torn_destination_fails_the_build(tmp_path: Path) -> None:
    """When the scan fails, the spec's own .piopm decides: an unremovable
    leftover fails the build."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    dest = Path(cls.base_dir) / "packages" / "bad"
    dest.mkdir(parents=True)
    (dest / ".piopm").write_text('{"spec": {"owner": "esphome", "name": "bad"}}')

    def bad_reset(self):
        raise OSError("scan broken")

    cls.memcache_reset = bad_reset
    with (
        patch.object(mod.fs, "rmtree", lambda path: None),  # onexc swallowed
        pytest.raises(mod.CleanupError, match="could not remove"),
    ):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])


def test_unverifiable_scan_without_leftover_degrades(tmp_path: Path, capsys) -> None:
    """A failing scan with no destination on disk is never a build
    failure blaming this spec."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    resets = {"n": 0}

    def bad_reset(self):
        # Fail clean_torn's reset; the coordinator's later reset works
        resets["n"] += 1
        if resets["n"] <= 1:
            raise OSError("scan broken")

    cls.memcache_reset = bad_reset
    mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert "No resolvable destination to clean" in capsys.readouterr().out


def test_unresolvable_torn_destination_is_printed(tmp_path: Path, capsys) -> None:
    """A failed install with no resolvable package prints, so an invisible
    torn directory is at least traceable."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert "No resolvable destination to clean" in capsys.readouterr().out


def test_unparsable_torn_destination_is_removed(tmp_path: Path, capsys) -> None:
    """A torn dir get_package cannot resolve but whose .piopm names the
    spec is removed instead of surviving into the serial pass."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    dest = Path(cls.base_dir) / "packages" / "bad"
    dest.mkdir(parents=True)
    (dest / ".piopm").write_text('{"spec": {"owner": "esphome", "name": "bad"}}')

    with patch.object(mod.fs, "rmtree", shutil.rmtree):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert not dest.exists()
    assert "Removed torn destination" in capsys.readouterr().out


def test_parse_specs_tools_branch(tmp_path: Path) -> None:
    """platform_packages parsing keeps owner'd tools and rewrites github
    URL pins to bare URLs the wave then skips via parsed.uri."""
    mod = _load_script()
    ini = tmp_path / "platformio.ini"
    ini.write_text(
        "[env:t]\n"
        "platform_packages =\n"
        "    ${common.platform_packages}\n"
        "    platformio/tool-scons@~4.40801.0\n"
        "    framework-arduinopico@https://github.com/earlephilhower/arduino-pico/releases/download/6.0.0/rp2040-6.0.0.zip\n"
    )
    args = Namespace(libraries=False, platforms=False, tools=True)
    libs, platforms, tools = mod.parse_specs(str(ini), args)
    assert libs == [] and platforms == []
    assert tools == [
        "platformio/tool-scons@~4.40801.0",
        "https://github.com/earlephilhower/arduino-pico/releases/download/6.0.0/rp2040-6.0.0.zip",
    ]
    assert mod.build_cli_args([], [], tools)[:2] == ["-t", tools[0]]


def test_warm_store_still_walks_dependencies(tmp_path: Path) -> None:
    """Already-installed top-level packages still feed the dependency
    wave; a warm store can be missing a transitive dep."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), installed={"esphome/noise-c @ 0.1.21"})
    cls.deps = {
        "esphome/noise-c @ 0.1.21": [
            {"owner": "esphome", "name": "libsodium", "version": "^1.0"},
        ],
    }
    mod.parallel_install(cls, ["esphome/noise-c @ 0.1.21"])
    assert [mod.spec_key(c) for c in cls.calls] == ["libsodium"]


def test_worker_system_exit_still_cleans(tmp_path: Path, capsys) -> None:
    """A worker SystemExit runs the torn cleanup before propagating; the
    serial pass must never trust its leftovers."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path))
    torn = tmp_path / "packages" / "torn-pkg"
    torn.mkdir(parents=True)

    def exiting_install(self, spec, skip_dependencies, compatibility=None):
        raise SystemExit(0)

    def get_package(self, spec):
        if getattr(cls, "resets", 0):
            return SimpleNamespace(path=str(torn), spec=spec)
        return None

    cls._install = exiting_install
    cls.get_package = get_package

    def real_rmtree(path):
        Path(path).rmdir()

    with (
        patch.object(mod.fs, "rmtree", real_rmtree),
        pytest.raises(SystemExit),
    ):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert not torn.exists()


def test_unlock_failure_is_fatal(tmp_path: Path) -> None:
    """A failed unlock must fail the build: the serial pass in another
    process would block on the held flock."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path))

    def bad_unlock(self):
        raise OSError("flock broke")

    cls.unlock = bad_unlock
    with pytest.raises(mod.LockReleaseError, match="manager lock"):
        mod.parallel_install(cls, ["esphome/good @ 1.0"])


def test_unlock_failure_keeps_inflight_error_as_context(tmp_path: Path) -> None:
    """An in-flight CleanupError stays attached when the unlock fault
    takes over the raise."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    torn = tmp_path / "packages" / "bad"
    torn.mkdir(parents=True)

    def get_package(self, spec):
        if getattr(cls, "resets", 0):
            return SimpleNamespace(path=str(torn), spec=spec)
        return None

    def bad_unlock(self):
        raise OSError("flock broke")

    cls.get_package = get_package
    cls.unlock = bad_unlock
    with (
        patch.object(mod.fs, "rmtree", lambda path: None),  # leaves torn
        pytest.raises(mod.LockReleaseError) as err,
    ):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert isinstance(err.value.__cause__.__context__, mod.CleanupError)


def test_chdir_failure_does_not_fail_the_wave(tmp_path: Path, monkeypatch) -> None:
    """A lost cwd is suppressed: further waves may misbehave and fall to
    the serial pass, whose cwd is pinned."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path))
    monkeypatch.setattr(mod.os, "chdir", MagicMock(side_effect=OSError("gone")))
    mod.parallel_install(cls, ["esphome/good @ 1.0"])
    assert cls.calls == ["esphome/good @ 1.0"]


def test_piopm_match_removes_manifest_named_torn_dir(tmp_path: Path, capsys) -> None:
    """A torn dir named by its manifest (not the registry spec) is found
    through its .piopm and removed."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    torn = tmp_path / "packages" / "ManifestName"
    torn.mkdir(parents=True)
    (torn / ".piopm").write_text('{"spec": {"owner": "esphome", "name": "bad"}}')
    innocent = tmp_path / "packages" / "innocent"
    innocent.mkdir()
    (innocent / ".piopm").write_text('{"spec": {"owner": "o", "name": "other"}}')
    with patch.object(mod.fs, "rmtree", shutil.rmtree):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert not torn.exists()
    assert innocent.exists()  # another package's valid metadata survives
    assert "Removed torn destination" in capsys.readouterr().out


def test_unscannable_package_dir_fails_the_build(tmp_path: Path) -> None:
    """A storage dir the cleanup cannot scan is not proof of cleanliness."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    real_iterdir = Path.iterdir

    def broken_iterdir(self):
        if self.name == "packages":
            raise PermissionError("denied")
        return real_iterdir(self)

    with (
        patch.object(Path, "iterdir", broken_iterdir),
        pytest.raises(mod.CleanupError, match="cleanup failed"),
    ):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])


def test_stray_file_in_package_dir_is_ignored(tmp_path: Path) -> None:
    """A plain file (or a pio-link) beside the packages is skipped by
    pio's own scan and must never hard-fail the build."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    (tmp_path / "packages").mkdir(parents=True)
    (tmp_path / "packages" / "stray.pio-link").write_text("x")
    (tmp_path / "packages" / "no-metadata").mkdir()  # pio overwrites these
    mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert (tmp_path / "packages" / "stray.pio-link").exists()
    assert (tmp_path / "packages" / "no-metadata").exists()


def test_unreadable_piopm_dir_is_removed(tmp_path: Path) -> None:
    """A persistently corrupt .piopm under this spec's own name would
    crash pio's storage scan; the dir is removed rather than left to
    break the serial pass."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    torn = tmp_path / "packages" / "bad"
    torn.mkdir(parents=True)
    (torn / ".piopm").write_text("{not json")
    mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert not torn.exists()


def test_unreadable_piopm_under_other_name_survives(tmp_path: Path) -> None:
    """A corrupt .piopm in another package's dir may be a worker mid-copy;
    a failing spec must not remove a directory it does not own."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})
    other = tmp_path / "packages" / "innocent"
    other.mkdir(parents=True)
    (other / ".piopm").write_text("{not json")
    mod.parallel_install(cls, ["esphome/bad @ 1.0"])
    assert other.exists()


def test_unexpected_cleanup_class_becomes_cleanup_error(tmp_path: Path) -> None:
    """Cleanup failures of any class fail the build; nothing may be
    downgraded to the serial fallback over a torn directory."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path), fail={"esphome/bad @ 1.0"})

    with (
        patch.object(
            mod, "piopm_matches", MagicMock(side_effect=ValueError("bad spec"))
        ),
        pytest.raises(mod.CleanupError, match="cleanup failed"),
    ):
        mod.parallel_install(cls, ["esphome/bad @ 1.0"])


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
    monkeypatch.setenv("PLATFORMIO_CACHE_DIR", str(tmp_path / "cache"))
    ContentCache("http")
    assert (tmp_path / "cache" / "http").is_dir()


def test_piopm_matches_without_name_matches_nothing(tmp_path: Path) -> None:
    """A spec with no derivable name can never match a directory."""
    mod = _load_script()
    assert mod.piopm_matches(str(tmp_path), "") == []


def test_unresolvable_spec_stays_out_of_the_wave(tmp_path: Path, capsys) -> None:
    """A spec with no derivable name is left to the serial pass; a raw
    string key would break the one-per-destination dedupe."""
    mod = _load_script()
    cls = _reset_fake(str(tmp_path))
    nameless = PackageSpec(requirements="^1.0")
    mod.parallel_install(cls, [nameless])
    assert cls.calls == []
    assert "Skipping unresolvable spec" in capsys.readouterr().out


def test_parallel_install_unlocks_when_pool_fails(tmp_path: Path) -> None:
    mod = _load_script()
    cls = _reset_fake(str(tmp_path))
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
    # Losing any of these turns the wave into main()'s silent serial
    # fallback: ensure_spec runs in the coordinator, the spec attributes
    # feed the dedupe, cleanup, and dependency filters
    assert callable(BasePackageManager.ensure_spec)
    spec = PackageSpec("owner/name @ ^1.0")
    assert spec.name == "name"
    assert spec.owner == "owner"
    assert spec.uri is None
    assert spec.external is False
    assert Version("1.5.0") in spec.requirements
    # The failure-cleanup path degrades to a single line if these vanish
    assert callable(fs.rmtree)
    assert callable(fs.load_json)
    # piopm_matches only tolerates a corrupt .piopm through this base;
    # losing it would flip a wave failure from degrade to build failure
    assert issubclass(InvalidJSONFile, ValueError)
    assert PackageItem("pkg-dir").path == "pkg-dir"
    assert callable(PackageCompatibility.from_dependency)
