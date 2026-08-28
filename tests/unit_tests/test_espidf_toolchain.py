"""Tests for esphome.espidf.toolchain helpers."""

# pylint: disable=protected-access

from collections.abc import Iterator
from contextlib import contextmanager
import json
import os
from pathlib import Path
import subprocess
from unittest.mock import patch

import pytest

from esphome.components.esp32.const import KEY_ESP32, KEY_VARIANT
from esphome.const import (
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_ESPHOME,
    CONF_FRAMEWORK,
    CONF_SOURCE,
)
from esphome.core import CORE, EsphomeError
from esphome.espidf import toolchain


def test_get_framework_source_override_no_config():
    """When CORE.config hasn't been set, no override is returned."""
    CORE.config = None
    assert toolchain._get_framework_source_override() is None


def test_get_framework_source_override_no_esp32_section():
    """A config without an esp32 section yields no override."""
    CORE.config = {}
    assert toolchain._get_framework_source_override() is None


def test_get_framework_source_override_no_framework_source():
    """An esp32 section without framework.source yields no override."""
    CORE.config = {"esp32": {CONF_FRAMEWORK: {}}}
    assert toolchain._get_framework_source_override() is None


def test_get_framework_source_override_returns_value():
    """A user-supplied framework source is returned verbatim."""
    url = "https://example.com/esp-idf-v{VERSION}.tar.xz"
    CORE.config = {"esp32": {CONF_FRAMEWORK: {CONF_SOURCE: url}}}
    assert toolchain._get_framework_source_override() == url


def test_get_esphome_esp_idf_paths_forwards_source_override():
    """_get_esphome_esp_idf_paths threads the override into check_esp_idf_install."""
    url = "https://my-mirror/esp-idf-v{VERSION}.tar.xz"
    CORE.config = {"esp32": {CONF_FRAMEWORK: {CONF_SOURCE: url}}}
    # Hit a fresh cache key so check_esp_idf_install is actually called.
    toolchain._cache().paths.clear()
    with patch.object(
        toolchain, "check_esp_idf_install", return_value=("/fw", "/penv")
    ) as mock_install:
        toolchain._get_esphome_esp_idf_paths("5.5.4")
    mock_install.assert_called_once_with("5.5.4", targets=None, source_url=url)


def test_get_esphome_esp_idf_paths_no_override():
    """When no source override is configured, source_url=None is passed."""
    CORE.config = {}
    toolchain._cache().paths.clear()
    with patch.object(
        toolchain, "check_esp_idf_install", return_value=("/fw", "/penv")
    ) as mock_install:
        toolchain._get_esphome_esp_idf_paths("5.5.4")
    mock_install.assert_called_once_with("5.5.4", targets=None, source_url=None)


def test_get_configured_targets_from_variant(monkeypatch: pytest.MonkeyPatch):
    """The configured variant restricts the toolchain install to its target."""
    monkeypatch.delenv("CI", raising=False)
    CORE.data[KEY_ESP32] = {KEY_VARIANT: "ESP32S3"}
    assert toolchain._get_configured_targets() == ["esp32s3"]


def test_get_configured_targets_without_variant(monkeypatch: pytest.MonkeyPatch):
    """No stored variant (e.g. tooling outside a build) keeps the default."""
    monkeypatch.delenv("CI", raising=False)
    CORE.data.pop(KEY_ESP32, None)
    assert toolchain._get_configured_targets() is None


def test_get_configured_targets_ci_installs_all(monkeypatch: pytest.MonkeyPatch):
    """CI installs every target so the shared cache covers all variants."""
    monkeypatch.setenv("CI", "true")
    CORE.data[KEY_ESP32] = {KEY_VARIANT: "ESP32S3"}
    assert toolchain._get_configured_targets() is None


def _setup_build(setup_core: Path) -> tuple[Path, Path]:
    """Point CORE at a build dir; return (compile_commands, idedata cache) paths."""
    CORE.name = "test"
    CORE.build_path = setup_core / "build" / "test"
    compile_commands = CORE.relative_build_path("build", "compile_commands.json")
    cache = CORE.relative_internal_path("idedata", "test.json")
    return compile_commands, cache


def test_has_outdated_files_detects_exclusion_change(setup_core: Path) -> None:
    """A newer exclude_components.esphomeinternal stamp forces a reconfigure
    so components that leave the exclusion set get rediscovered."""
    CORE.build_path = setup_core
    build = setup_core / "build"
    (build / "config").mkdir(parents=True)
    (build / "config" / "sdkconfig.h").write_text("")
    cmakecache = build / "CMakeCache.txt"
    cmakecache.write_text("")
    (build / "build.ninja").write_text("")

    with patch.object(CORE, "name", "test"):
        assert not toolchain.has_outdated_files()

        stamp = setup_core / "exclude_components.esphomeinternal"
        stamp.write_text("unity")
        os.utime(stamp, (cmakecache.stat().st_mtime + 10,) * 2)

        assert toolchain.has_outdated_files()

        # The flag must clear once the reference file is restamped (as
        # run_compile does after a successful discovery reconfigure);
        # otherwise every later build would repeat the discovery pass.
        os.utime(cmakecache, (stamp.stat().st_mtime + 10,) * 2)
        assert not toolchain.has_outdated_files()


def test_get_idedata_returns_none_without_compile_commands(setup_core: Path) -> None:
    """No compile DB yet -> None (rather than an error)."""
    _setup_build(setup_core)
    assert toolchain.get_idedata() is None


def test_get_idedata_generates_and_caches(setup_core: Path) -> None:
    """Generates from the compile DB and writes the cache."""
    compile_commands, cache = _setup_build(setup_core)
    compile_commands.parent.mkdir(parents=True, exist_ok=True)
    compile_commands.write_text("[]")

    with patch(
        "esphome.build_helpers.idedata.idedata_from_build",
        return_value={"cxx_path": "g++"},
    ) as mock_transform:
        result = toolchain.get_idedata()

    mock_transform.assert_called_once()
    prog_path = str(toolchain.get_elf_path())
    assert result == {"cxx_path": "g++", "prog_path": prog_path}
    assert json.loads(cache.read_text()) == {"cxx_path": "g++", "prog_path": prog_path}


def test_get_idedata_prog_path_points_at_firmware_elf(setup_core: Path) -> None:
    """The idedata exposes prog_path (the ELF) so consumers like build-action
    can locate firmware.factory.bin / firmware.ota.bin as its siblings."""
    compile_commands, _ = _setup_build(setup_core)
    compile_commands.parent.mkdir(parents=True, exist_ok=True)
    compile_commands.write_text("[]")

    with patch(
        "esphome.build_helpers.idedata.idedata_from_build",
        return_value={"cxx_path": "g++"},
    ):
        result = toolchain.get_idedata()

    # Use Path semantics so the contract holds on Windows too (backslashes).
    prog_path = Path(result["prog_path"])
    assert prog_path.name == "firmware.elf"
    assert prog_path.parent.name == "build"


def test_get_idf_env_sets_git_ceiling_directories(setup_core: Path) -> None:
    """The IDF env caps git's upward search at the config directory.

    This stops ESP-IDF's `git describe` from walking into an uninitialized or
    corrupt git repo in a parent directory and failing the build.
    """
    toolchain._cache().env.clear()
    # Set IDF_PATH so the framework-install branch is skipped.
    with patch.dict(os.environ, {"IDF_PATH": str(setup_core)}):
        env = toolchain._get_idf_env(version="5.5.4")
    assert CORE.config_dir == setup_core
    assert str(CORE.config_dir) in env["GIT_CEILING_DIRECTORIES"].split(os.pathsep)


def test_get_idf_env_pops_inherited_pythonpath(setup_core: Path) -> None:
    """A PYTHONPATH from the parent environment must not reach idf.py.

    It would override the IDF venv's isolation, shadowing its pinned
    packages and failing idf.py's dependency check.
    """
    toolchain._cache().env.clear()
    with patch.dict(
        os.environ,
        {"IDF_PATH": str(setup_core), "PYTHONPATH": "/outside/site-packages"},
    ):
        env = toolchain._get_idf_env(version="5.5.4")
    assert "PYTHONPATH" not in env


def test_get_cmake_output_without_build_dir(setup_core: Path) -> None:
    """A build dir that was never created raises EsphomeError.

    Without this, subprocess.run(cwd=build_dir) raises FileNotFoundError, which
    the log stack-trace decoder doesn't recognise as a decode failure.
    """
    _setup_build(setup_core)
    build_dir = CORE.relative_build_path("build")
    assert not build_dir.exists()

    with pytest.raises(EsphomeError, match="No ESP-IDF build found"):
        toolchain._get_cmake_output(build_dir)


def test_get_cmake_output_without_cmake_cache(setup_core: Path) -> None:
    """A build dir that exists but was never configured raises EsphomeError."""
    _setup_build(setup_core)
    build_dir = CORE.relative_build_path("build")
    build_dir.mkdir(parents=True)

    with pytest.raises(EsphomeError, match="No ESP-IDF build found"):
        toolchain._get_cmake_output(build_dir)


def test_get_cmake_output_with_configured_build(setup_core: Path) -> None:
    """A configured build still runs cmake and caches the output.

    The missing-build guard must not get in the way of a real build.
    """
    _setup_build(setup_core)
    build_dir = CORE.relative_build_path("build")
    build_dir.mkdir(parents=True)
    (build_dir / "CMakeCache.txt").write_text("")

    completed = subprocess.CompletedProcess(
        args=[], returncode=0, stdout="CMAKE_ADDR2LINE:FILEPATH=/tool/addr2line\n"
    )
    with (
        patch.object(toolchain, "_get_idf_env", return_value={}),
        patch.object(toolchain.subprocess, "run", return_value=completed) as mock_run,
    ):
        assert toolchain._get_cmake_output(build_dir) == completed.stdout
        # Second call is served from the cache rather than re-running cmake.
        assert toolchain._get_cmake_output(build_dir) == completed.stdout

    mock_run.assert_called_once()
    assert toolchain._get_cmake_tool_path("CMAKE_ADDR2LINE") == Path("/tool/addr2line")


def test_get_cmake_output_missing_build_does_not_resolve_idf_env(
    setup_core: Path,
) -> None:
    """The build check runs before the env is resolved.

    Resolving the env calls check_esp_idf_install(), which can download and
    extract the whole framework. A doomed call must never start that.
    """
    _setup_build(setup_core)
    build_dir = CORE.relative_build_path("build")

    with (
        patch.object(toolchain, "_get_idf_env") as mock_env,
        patch.object(toolchain.subprocess, "run") as mock_run,
        pytest.raises(EsphomeError),
    ):
        toolchain._get_cmake_output(build_dir)

    mock_env.assert_not_called()
    mock_run.assert_not_called()


def test_run_idf_py_jobs_sets_build_jobs_env(setup_core: Path) -> None:
    """The jobs argument is exported to idf.py as IDF_PY_BUILD_JOBS."""
    _setup_build(setup_core)

    with (
        patch.object(toolchain, "_get_idf_path", return_value=Path("/idf")),
        patch.object(toolchain, "_get_idf_env", return_value={"PATH": "/bin"}),
        patch.object(toolchain, "_get_idf_tool", return_value="python"),
        patch.object(toolchain.subprocess, "run") as mock_run,
    ):
        mock_run.return_value.returncode = 0

        toolchain.run_idf_py("build", jobs=2)
        env = mock_run.call_args.kwargs["env"]
        assert env["IDF_PY_BUILD_JOBS"] == "2"
        assert env["PATH"] == "/bin"

        toolchain.run_idf_py("build")
        env = mock_run.call_args.kwargs["env"]
        assert "IDF_PY_BUILD_JOBS" not in env


def test_run_compile_restamps_cmakecache_after_discovery(setup_core: Path) -> None:
    """After a successful discovery reconfigure the reference CMakeCache.txt
    is restamped; cmake does not rewrite it when only properties or plain
    variables change, so the staleness flag would otherwise never clear."""
    _setup_build(setup_core)
    config = {CONF_ESPHOME: {}}
    cmakecache = CORE.relative_build_path("build/CMakeCache.txt")
    build_ninja = CORE.relative_build_path("build/build.ninja")
    cmakecache.parent.mkdir(parents=True, exist_ok=True)
    cmakecache.write_text("")
    build_ninja.write_text("")
    old = cmakecache.stat().st_mtime - 100
    os.utime(cmakecache, (old, old))
    os.utime(build_ninja, (old, old))

    with (
        patch.object(toolchain, "need_reconfigure", return_value=True),
        patch.object(toolchain, "load_cached_builtin_components", return_value=None),
        patch.object(toolchain, "save_cached_builtin_components"),
        patch(
            "esphome.build_gen.espidf.get_available_components", return_value=["lwip"]
        ),
        patch("esphome.build_gen.espidf.write_project"),
        patch.object(toolchain, "run_reconfigure", return_value=0),
        patch.object(toolchain, "run_idf_py", return_value=0),
        patch.object(toolchain, "print_summary"),
    ):
        assert toolchain.run_compile(config, verbose=False) == 0

    assert cmakecache.stat().st_mtime > old
    # build.ninja must not be older than the cache or ninja re-runs cmake
    assert build_ninja.stat().st_mtime >= cmakecache.stat().st_mtime


def test_run_compile_discovery_without_cmakecache(setup_core: Path) -> None:
    """A discovery pass that produced no CMakeCache.txt (nothing to restamp)
    still completes normally."""
    _setup_build(setup_core)
    config = {CONF_ESPHOME: {}}

    with (
        patch.object(toolchain, "need_reconfigure", return_value=True),
        patch.object(toolchain, "load_cached_builtin_components", return_value=None),
        patch.object(toolchain, "save_cached_builtin_components"),
        patch(
            "esphome.build_gen.espidf.get_available_components", return_value=["lwip"]
        ),
        patch("esphome.build_gen.espidf.write_project"),
        patch.object(toolchain, "run_reconfigure", return_value=0),
        patch.object(toolchain, "run_idf_py", return_value=0),
        patch.object(toolchain, "print_summary"),
    ):
        assert toolchain.run_compile(config, verbose=False) == 0

    assert not CORE.relative_build_path("build/CMakeCache.txt").exists()


def test_run_compile_reconfigures_after_full_write_outside_testing_mode(
    setup_core: Path,
) -> None:
    """The full CMakeLists write is followed by a reconfigure (#18682); a
    failure there stops the build and leaves the cache unstamped."""
    _setup_build(setup_core)
    config = {CONF_ESPHOME: {}}
    cmakecache = CORE.relative_build_path("build/CMakeCache.txt")
    cmakecache.parent.mkdir(parents=True, exist_ok=True)
    cmakecache.write_text("")
    old = cmakecache.stat().st_mtime - 100
    os.utime(cmakecache, (old, old))
    calls: list[tuple] = []
    reconfigures = 0

    def record_write(minimal: bool = False, builtin_components=None) -> None:
        calls.append(("write_project", minimal))

    def record_reconfigure() -> int:
        nonlocal reconfigures
        reconfigures += 1
        calls.append(("run_reconfigure",))
        return 1 if reconfigures == 2 else 0

    with (
        patch.object(toolchain, "need_reconfigure", return_value=True),
        patch.object(toolchain, "load_cached_builtin_components", return_value=None),
        patch.object(toolchain, "save_cached_builtin_components"),
        patch(
            "esphome.build_gen.espidf.get_available_components", return_value=["lwip"]
        ),
        patch("esphome.build_gen.espidf.write_project", side_effect=record_write),
        patch.object(toolchain, "run_reconfigure", side_effect=record_reconfigure),
        patch.object(toolchain, "run_idf_py", return_value=0) as mock_build,
        patch.object(toolchain, "print_summary"),
    ):
        assert not CORE.testing_mode
        assert toolchain.run_compile(config, verbose=False) == 1

    assert calls == [
        ("write_project", True),
        ("run_reconfigure",),
        ("write_project", False),
        ("run_reconfigure",),
    ]
    mock_build.assert_not_called()
    assert cmakecache.stat().st_mtime == old


def _record_compile_calls(
    cached: list[str] | None,
    saved: list[str] | None = None,
    reconfigure_rcs: tuple[int, ...] = (),
    cache_file: Path | None = None,
) -> tuple[int, list[tuple]]:
    """Run run_compile with a stubbed cache and return (rc, call log).

    ``reconfigure_rcs`` overrides the exit codes of the first reconfigures;
    later ones succeed.
    """
    calls: list[tuple] = []
    rcs = iter(reconfigure_rcs)

    def record_reconfigure() -> int:
        calls.append(("run_reconfigure",))
        return next(rcs, 0)

    def record_write(minimal: bool = False, builtin_components=None) -> None:
        calls.append(("write_project", minimal, builtin_components))

    def record_save(components: list[str]) -> None:
        calls.append(("save", components))

    with (
        patch.object(toolchain, "need_reconfigure", return_value=True),
        patch.object(toolchain, "load_cached_builtin_components", return_value=cached),
        patch.object(
            toolchain, "save_cached_builtin_components", side_effect=record_save
        ),
        patch("esphome.build_gen.espidf.get_available_components", return_value=saved),
        patch("esphome.build_gen.espidf.write_project", side_effect=record_write),
        patch.object(toolchain, "run_reconfigure", side_effect=record_reconfigure),
        patch.object(
            toolchain, "_builtin_component_cache_path", return_value=cache_file
        ),
        patch.object(
            toolchain,
            "run_idf_py",
            side_effect=lambda *a, **kw: calls.append(("build",)) or 0,
        ),
        patch.object(toolchain, "print_summary"),
    ):
        rc = toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False)
    return rc, calls


def test_run_compile_poisoned_cache_is_dropped_and_rediscovered(
    setup_core: Path, tmp_path: Path
) -> None:
    """A cached list that fails the configure is deleted and discovery runs
    once more instead of every later build failing the same way."""
    _setup_build(setup_core)
    cache_file = tmp_path / "esp32-abc.json"
    cache_file.write_text("[]")
    rc, calls = _record_compile_calls(
        ["stale"], saved=["lwip"], reconfigure_rcs=(1,), cache_file=cache_file
    )
    assert rc == 0
    assert not cache_file.exists()
    assert calls == [
        ("write_project", False, ["stale"]),
        ("run_reconfigure",),
        ("write_project", True, None),
        ("run_reconfigure",),
        ("write_project", False, ["lwip"]),
        ("run_reconfigure",),
        ("save", ["lwip"]),
        ("build",),
    ]


def test_run_compile_cache_miss_discovers_and_saves(setup_core: Path) -> None:
    """Without a cached list the discovery configure runs, the discovered list
    feeds the full write and is cached only after that configure succeeds."""
    _setup_build(setup_core)
    rc, calls = _record_compile_calls(None, saved=["lwip"])
    assert rc == 0
    assert calls == [
        ("write_project", True, None),
        ("run_reconfigure",),
        ("write_project", False, ["lwip"]),
        ("run_reconfigure",),
        ("save", ["lwip"]),
        ("build",),
    ]


def test_run_compile_discovery_failure_stops_before_full_write(
    setup_core: Path,
) -> None:
    """A failed discovery configure returns its exit code and never writes
    the full CMakeLists, a cache entry or a build."""
    _setup_build(setup_core)
    rc, calls = _record_compile_calls(None, reconfigure_rcs=(2,))
    assert rc == 2
    assert calls == [("write_project", True, None), ("run_reconfigure",)]


@pytest.mark.parametrize("discovered", [None, []], ids=["no_manifest", "empty"])
def test_run_compile_fails_when_discovery_finds_nothing(
    setup_core: Path,
    caplog: pytest.LogCaptureFixture,
    discovered: list[str] | None,
) -> None:
    _setup_build(setup_core)
    rc, calls = _record_compile_calls(None, saved=discovered)
    assert rc == 1
    assert calls == [("write_project", True, None), ("run_reconfigure",)]
    assert "found no built-in ESP-IDF components" in caplog.text


def test_run_compile_does_not_cache_a_list_that_failed_to_configure(
    setup_core: Path,
) -> None:
    _setup_build(setup_core)
    rc, calls = _record_compile_calls(None, saved=["lwip"], reconfigure_rcs=(0, 3))
    assert rc == 3
    assert ("save", ["lwip"]) not in calls
    assert ("build",) not in calls


def test_run_compile_cache_hit_skips_discovery(setup_core: Path) -> None:
    """A cached list goes straight to the full write; the explicit reconfigure
    after it (#18730) still runs."""
    _setup_build(setup_core)
    rc, calls = _record_compile_calls(["esp_timer", "lwip"])
    assert rc == 0
    assert calls == [
        ("write_project", False, ["esp_timer", "lwip"]),
        ("run_reconfigure",),
        ("build",),
    ]


@contextmanager
def _cache_env(tmp_path: Path, excluded: str) -> Iterator[Path]:
    """Patch everything the cache key derives from onto a temp IDF tree and
    yield that tree's path."""
    idf_path = tmp_path / "idf"
    (idf_path / "components").mkdir(parents=True, exist_ok=True)
    with (
        patch.object(toolchain, "_get_idf_path", return_value=idf_path),
        patch.dict(CORE.data, {KEY_ESP32: {KEY_VARIANT: "ESP32"}}),
        patch.dict(CORE.cmake_args, {"EXCLUDE_COMPONENTS": excluded}),
    ):
        yield idf_path


def test_component_cache_round_trip(setup_core: Path, tmp_path: Path) -> None:
    """A saved list is read back until it is dropped."""
    _setup_build(setup_core)
    with _cache_env(tmp_path, "fatfs") as idf_path:
        for name in ("lwip", "esp_timer"):
            (idf_path / "components" / name).mkdir()
        assert toolchain.load_cached_builtin_components() is None
        toolchain.save_cached_builtin_components(["esp_timer", "lwip"])
        assert toolchain.load_cached_builtin_components() == ["esp_timer", "lwip"]
        toolchain._builtin_component_cache_path().unlink()
        assert toolchain.load_cached_builtin_components() is None


def test_component_cache_misses_on_key_change_or_missing_component(
    setup_core: Path, tmp_path: Path
) -> None:
    """A different exclusion set uses another entry, an entry naming a
    component that no longer exists is ignored, and a custom IDF_PATH is
    never cached."""
    _setup_build(setup_core)
    with _cache_env(tmp_path, "fatfs") as idf_path:
        (idf_path / "components" / "lwip").mkdir()
        toolchain.save_cached_builtin_components(["lwip"])
        path = toolchain._builtin_component_cache_path()
        assert path.parent == idf_path / ".esphome_component_lists"
        assert path.name.startswith("esp32-")
        assert toolchain.load_cached_builtin_components() == ["lwip"]
        with patch.dict(os.environ, {"IDF_PATH": str(idf_path)}):
            assert toolchain.load_cached_builtin_components() is None
    with _cache_env(tmp_path, "fatfs;unity"):
        assert toolchain.load_cached_builtin_components() is None
    with _cache_env(tmp_path, "fatfs") as idf_path:
        path.write_text(json.dumps(["lwip", "gone"]))
        assert toolchain.load_cached_builtin_components() is None
        # A plain file with the right name is not a component directory.
        (idf_path / "components" / "gone").write_text("not a directory")
        assert toolchain.load_cached_builtin_components() is None


def test_component_cache_save_skips_empty_list_or_custom_idf_path(
    setup_core: Path, tmp_path: Path
) -> None:
    _setup_build(setup_core)
    with _cache_env(tmp_path, "") as idf_path:
        toolchain.save_cached_builtin_components([])
        with patch.dict(os.environ, {"IDF_PATH": str(idf_path)}):
            toolchain.save_cached_builtin_components(["lwip"])
        assert not (idf_path / ".esphome_component_lists").exists()


def test_component_cache_write_failure_is_logged(
    setup_core: Path, tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _setup_build(setup_core)
    with (
        _cache_env(tmp_path, ""),
        patch.object(toolchain, "write_file", side_effect=EsphomeError("disk full")),
    ):
        toolchain.save_cached_builtin_components(["lwip"])
        assert toolchain.load_cached_builtin_components() is None
    assert "Could not write component list cache" in caplog.text


def test_component_cache_ignores_corrupt_file(setup_core: Path, tmp_path: Path) -> None:
    _setup_build(setup_core)
    with _cache_env(tmp_path, ""):
        path = toolchain._builtin_component_cache_path()
        path.parent.mkdir(parents=True)
        path.write_text("{not json")
        assert toolchain.load_cached_builtin_components() is None
        path.write_text(json.dumps({"components": ["lwip"]}))
        assert toolchain.load_cached_builtin_components() is None


def test_run_compile_passes_compile_process_limit(setup_core: Path) -> None:
    """compile_process_limit is forwarded to run_idf_py as the job limit."""
    _setup_build(setup_core)
    config = {CONF_ESPHOME: {CONF_COMPILE_PROCESS_LIMIT: 1}}

    with (
        patch.object(toolchain, "need_reconfigure", return_value=False),
        patch.object(toolchain, "run_idf_py", return_value=0) as mock_run,
        patch.object(toolchain, "print_summary"),
    ):
        assert toolchain.run_compile(config, verbose=False) == 0

    mock_run.assert_called_once_with("build", "size", jobs=1)


def test_run_compile_without_compile_process_limit(setup_core: Path) -> None:
    """When no compile_process_limit is set, no job limit is passed to idf.py."""
    _setup_build(setup_core)
    config = {CONF_ESPHOME: {}}

    with (
        patch.object(toolchain, "need_reconfigure", return_value=False),
        patch.object(toolchain, "run_idf_py", return_value=0) as mock_run,
        patch.object(toolchain, "print_summary"),
    ):
        assert toolchain.run_compile(config, verbose=False) == 0

    mock_run.assert_called_once_with("build", "size", jobs=None)


def test_get_core_framework_version_from_core_data():
    """The version is read from CORE.data when validation populated it."""
    from esphome.components.esp32.const import KEY_ESP32, KEY_IDF_VERSION
    import esphome.config_validation as cv

    CORE.data = {KEY_ESP32: {KEY_IDF_VERSION: cv.Version(5, 5, 4)}}
    assert toolchain._get_core_framework_version() == "5.5.4"
