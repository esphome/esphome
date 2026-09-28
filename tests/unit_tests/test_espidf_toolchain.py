"""Tests for esphome.espidf.toolchain helpers."""

# pylint: disable=protected-access

from collections.abc import Iterator
from contextlib import contextmanager
import json
import os
from pathlib import Path
import subprocess
from unittest.mock import call, patch

import pytest

from esphome.components.esp32.const import KEY_ESP32, KEY_IDF_VERSION, KEY_VARIANT
import esphome.config_validation as cv
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


@pytest.fixture(autouse=True)
def _no_ccache(monkeypatch: pytest.MonkeyPatch) -> None:
    """Deterministic run_compile: no host ccache probe, no pch work."""
    monkeypatch.setenv("IDF_CCACHE_ENABLE", "0")
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")


def _setup_build(setup_core: Path) -> tuple[Path, Path]:
    """Point CORE at a build dir; return (compile_commands, idedata cache) paths."""
    CORE.name = "test"
    CORE.build_path = setup_core / "build" / "test"
    CORE.data.setdefault(KEY_ESP32, {})[KEY_IDF_VERSION] = cv.Version(5, 5, 5)
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
        patch.object(toolchain, "_run_ninja", return_value=0),
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
        patch.object(toolchain, "_run_ninja", return_value=0),
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

    def record_reconfigure(verbose: bool = False) -> int:
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
        patch.object(toolchain, "_run_ninja", return_value=0) as mock_build,
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

    def record_reconfigure(verbose: bool = False) -> int:
        calls.append(("run_reconfigure",))
        return next(rcs, 0)

    def record_write(minimal: bool = False, builtin_components=None) -> None:
        calls.append(("write_project", minimal, builtin_components))

    def record_save(components: list[str]) -> None:
        calls.append(("save", components))

    def record_ninja(target: str, **kwargs: object) -> int:
        if target == "all":
            calls.append(("build",))
        return 0

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
        patch.object(toolchain, "_run_ninja", side_effect=record_ninja),
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
    """compile_process_limit is the job limit for both ninja runs."""
    _setup_build(setup_core)
    config = {CONF_ESPHOME: {CONF_COMPILE_PROCESS_LIMIT: 1}}

    with (
        patch.object(toolchain, "need_reconfigure", return_value=False),
        patch.object(toolchain, "_cache_entries_changed", return_value=False),
        patch.object(toolchain, "_run_ninja", return_value=0) as mock_run,
        patch.object(toolchain, "print_summary"),
    ):
        assert toolchain.run_compile(config, verbose=False) == 0

    assert mock_run.call_args_list == [
        call("all", verbose=False, jobs=1, progress=True),
        call("size", verbose=False, jobs=1, extra_env=toolchain._size_env()),
    ]


def test_run_compile_passes_size_summary_paths(setup_core: Path) -> None:
    """print_summary receives the size json, partitions.csv, and the built
    ELF from get_built_elf_path, which must stay in lockstep with the
    project() name in the generated CMakeLists."""
    _setup_build(setup_core)
    config = {CONF_ESPHOME: {}}

    with (
        patch.object(toolchain, "need_reconfigure", return_value=False),
        patch.object(toolchain, "_cache_entries_changed", return_value=False),
        patch.object(toolchain, "_run_ninja", return_value=0),
        patch.object(toolchain, "print_summary") as mock_summary,
    ):
        assert toolchain.run_compile(config, verbose=False) == 0

    mock_summary.assert_called_once_with(
        CORE.relative_build_path("build", "esp_idf_size.json"),
        CORE.relative_build_path("partitions.csv"),
        CORE.relative_build_path("build", f"{CORE.name}.elf"),
    )


def test_create_elf_copy(setup_core: Path) -> None:
    """The built <name>.elf is copied to the firmware.elf dashboard name."""
    _setup_build(setup_core)
    src = toolchain.get_built_elf_path()
    src.parent.mkdir(parents=True, exist_ok=True)
    src.write_bytes(b"elf")
    assert toolchain.create_elf_copy() is True
    assert toolchain.get_elf_path().read_bytes() == b"elf"


def test_create_elf_copy_missing_source(setup_core: Path) -> None:
    """A missing built ELF is a warning and False, not a crash."""
    _setup_build(setup_core)
    assert toolchain.create_elf_copy() is False


def test_run_compile_without_compile_process_limit(setup_core: Path) -> None:
    """When no compile_process_limit is set, ninja gets no job limit."""
    _setup_build(setup_core)
    config = {CONF_ESPHOME: {}}

    with (
        patch.object(toolchain, "need_reconfigure", return_value=False),
        patch.object(toolchain, "_cache_entries_changed", return_value=False),
        patch.object(toolchain, "_run_ninja", return_value=0) as mock_run,
        patch.object(toolchain, "print_summary"),
    ):
        assert toolchain.run_compile(config, verbose=False) == 0

    assert [c.kwargs["jobs"] for c in mock_run.call_args_list] == [None, None]


def test_run_compile_writes_the_pch_checksum_before_the_build(
    setup_core: Path,
) -> None:
    _setup_build(setup_core)
    order: list[str] = []

    with (
        patch.object(toolchain, "need_reconfigure", return_value=False),
        patch.object(toolchain, "_cache_entries_changed", return_value=False),
        patch.object(
            toolchain,
            "_run_ninja",
            side_effect=lambda *a, **k: order.append("build") or 0,
        ),
        patch.object(toolchain, "print_summary"),
        patch(
            "esphome.build_gen.espidf.write_pch_checksum",
            side_effect=lambda: order.append("checksum"),
        ),
    ):
        toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False)
    assert order[:2] == ["checksum", "build"]


def test_get_core_framework_version_from_core_data():
    """The version is read from CORE.data when validation populated it."""
    from esphome.components.esp32.const import KEY_ESP32, KEY_IDF_VERSION
    import esphome.config_validation as cv

    CORE.data = {KEY_ESP32: {KEY_IDF_VERSION: cv.Version(5, 5, 4)}}
    assert toolchain._get_core_framework_version() == "5.5.4"


@contextmanager
def _fake_tools(env: dict[str, str] | None = None) -> Iterator:
    """Stub the IDF env and tool lookup; yield the run_build_tool mock."""
    with (
        patch.object(
            toolchain,
            "_get_idf_env",
            return_value={"PATH": "/bin", "IDF_CCACHE_ENABLE": "0", **(env or {})},
        ),
        patch.object(toolchain, "_get_idf_tool", side_effect=lambda n: f"/tools/{n}"),
        patch.object(toolchain, "run_build_tool", return_value=0) as mock_run,
    ):
        yield mock_run


def test_run_reconfigure_cmake_argv_matches_idf_py(setup_core: Path) -> None:
    """The configure is the command idf.py runs.

    IDF 5.5.5 tools/idf_py_actions/tools.py ensure_build_directory: cmake
    -G Ninja -DPYTHON_DEPS_CHECKED=1 -DPYTHON=<python> -DESP_PLATFORM=1,
    then the -D entries (SDKCONFIG, CCACHE_ENABLE), then the real project
    path, in the build dir. IDF 6.1 adds -B <build dir>, the same as the cwd.
    """
    _setup_build(setup_core)
    sdkconfig = CORE.relative_build_path("sdkconfig.test")
    sdkconfig.parent.mkdir(parents=True)
    sdkconfig.write_text("")
    project = os.path.realpath(CORE.build_path)

    with _fake_tools() as mock_run:
        assert toolchain.run_reconfigure() == 0

    assert mock_run.call_args.args[0] == [
        "/tools/cmake",
        "-G",
        "Ninja",
        "-DPYTHON_DEPS_CHECKED=1",
        "-DPYTHON=/tools/python",
        "-DESP_PLATFORM=1",
        f"-DSDKCONFIG={sdkconfig}",
        "-DCCACHE_ENABLE=0",
        project,
    ]
    kwargs = mock_run.call_args.kwargs
    assert kwargs["cwd"] == Path(project) / "build"
    assert kwargs["cwd"].is_dir()
    # idf.py only adds CLICOLOR_FORCE for ninja
    assert "CLICOLOR_FORCE" not in kwargs["env"]
    assert kwargs["filter_lines"] is toolchain.FILTER_IDF_LINES


@pytest.mark.parametrize(("ccache", "expected"), [("1", "True"), ("0", "False")])
def test_run_reconfigure_cmake_argv_matches_idf6_py(
    setup_core: Path, ccache: str, expected: str
) -> None:
    """IDF 6 idf.py adds -B <build dir> and formats CCACHE_ENABLE as True/False.

    IDF 6.1 tools/idf_py_actions/tools.py ensure_build_directory.
    """
    _setup_build(setup_core)
    CORE.data[KEY_ESP32][KEY_IDF_VERSION] = cv.Version(6, 1, 0)
    build_dir = Path(os.path.realpath(CORE.build_path)) / "build"

    with _fake_tools({"IDF_CCACHE_ENABLE": ccache}) as mock_run:
        assert toolchain.run_reconfigure() == 0

    assert mock_run.call_args.args[0] == [
        "/tools/cmake",
        "-G",
        "Ninja",
        "-B",
        str(build_dir),
        "-DPYTHON_DEPS_CHECKED=1",
        "-DPYTHON=/tools/python",
        "-DESP_PLATFORM=1",
        f"-DCCACHE_ENABLE={expected}",
        str(build_dir.parent),
    ]


def test_run_reconfigure_without_sdkconfig_or_filter(setup_core: Path) -> None:
    """No sdkconfig file means no SDKCONFIG entry; -v shows every line."""
    _setup_build(setup_core)
    with _fake_tools() as mock_run:
        assert toolchain.run_reconfigure(verbose=True) == 0
    cmd = mock_run.call_args.args[0]
    assert not any(arg.startswith("-DSDKCONFIG=") for arg in cmd)
    assert cmd[-2] == "-DCCACHE_ENABLE=0"
    assert mock_run.call_args.kwargs["filter_lines"] is None


def test_run_reconfigure_failure_removes_cmakecache(
    setup_core: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Like idf.py, a failed configure must not leave a cache that looks valid."""
    _setup_build(setup_core)
    cache = Path(os.path.realpath(CORE.build_path)) / "build" / "CMakeCache.txt"
    cache.parent.mkdir(parents=True)
    cache.write_text("")
    with _fake_tools() as mock_run:
        mock_run.return_value = 4
        assert toolchain.run_reconfigure() == 4
    assert not cache.exists()
    assert "CMake configure failed with exit code 4" in caplog.text


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("1", "1"),
        ("on", "1"),
        (" Yes ", "1"),
        ("0", "0"),
        ("", "0"),
        ("disable", "0"),
    ],
)
def test_cache_entries_ccache_follows_click_booleans(
    setup_core: Path, value: str, expected: str
) -> None:
    """IDF_CCACHE_ENABLE maps to CCACHE_ENABLE the way idf.py's click flag does."""
    _setup_build(setup_core)
    with _fake_tools({"IDF_CCACHE_ENABLE": value}):
        assert toolchain._cache_entries() == {"CCACHE_ENABLE": expected}


def test_parse_cmakecache(tmp_path: Path) -> None:
    cache = tmp_path / "CMakeCache.txt"
    cache.write_text(
        "# comment\n"
        "// help text\n"
        "SDKCONFIG:UNINITIALIZED=/a/sdkconfig.test\n"
        "CCACHE_ENABLE:UNINITIALIZED=1\n"
        "FLAGS:STRING=-DX=1\n"
        "\n"
    )
    assert toolchain._parse_cmakecache(cache) == {
        "SDKCONFIG": "/a/sdkconfig.test",
        "CCACHE_ENABLE": "1",
        "FLAGS": "-DX=1",
    }


@pytest.mark.parametrize(
    ("cache_text", "expected"),
    [
        (None, True),
        ("CCACHE_ENABLE:UNINITIALIZED=0\n", False),
        ("CCACHE_ENABLE:UNINITIALIZED=1\n", True),
        ("OTHER:STRING=0\n", True),
    ],
    ids=["no_cache", "same", "changed", "missing"],
)
def test_cache_entries_changed(
    setup_core: Path, cache_text: str | None, expected: bool
) -> None:
    """Mirrors idf.py's _new_cmakecache_entries."""
    _setup_build(setup_core)
    if cache_text is not None:
        cache = Path(os.path.realpath(CORE.build_path)) / "build" / "CMakeCache.txt"
        cache.parent.mkdir(parents=True)
        cache.write_text(cache_text)
    with _fake_tools():
        assert toolchain._cache_entries_changed() is expected


@pytest.mark.parametrize(
    ("version", "size_ng"), [(cv.Version(5, 5, 5), True), (cv.Version(6, 1, 0), False)]
)
def test_size_env(setup_core: Path, version: cv.Version, size_ng: bool) -> None:
    """IDF 5.x idf.py sets ESP_IDF_SIZE_NG; 6.x dropped it (core_ext.py size_target)."""
    CORE.data[KEY_ESP32] = {KEY_IDF_VERSION: version}
    env = toolchain._size_env()
    assert env["ESP_IDF_SIZE_FORCE_TERMINAL"] == "1"
    assert env["SIZE_OUTPUT_FORMAT"] == "default"
    assert ("ESP_IDF_SIZE_NG" in env) is size_ng


def test_run_ninja_matches_idf_py_run_target(setup_core: Path) -> None:
    """The command is ninja [-j N] [-v] <target> in the build dir, CLICOLOR_FORCE=1."""
    _setup_build(setup_core)
    with _fake_tools({"CLICOLOR_FORCE": "0"}) as mock_run:
        assert (
            toolchain._run_ninja(
                "size", verbose=True, jobs=2, progress=True, extra_env={"A": "b"}
            )
            == 0
        )
    assert mock_run.call_args.args[0] == ["/tools/ninja", "-j", "2", "-v", "size"]
    kwargs = mock_run.call_args.kwargs
    assert kwargs["cwd"] == Path(os.path.realpath(CORE.build_path)) / "build"
    assert kwargs["env"]["CLICOLOR_FORCE"] == "1"
    assert kwargs["env"]["A"] == "b"
    # -v shows every line as it comes, as idf.py does
    assert kwargs["filter_lines"] is None
    assert kwargs["progress"] is False


def test_run_ninja_filters_and_reports_failure(
    setup_core: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _setup_build(setup_core)
    with _fake_tools() as mock_run:
        mock_run.return_value = 1
        assert toolchain._run_ninja("all", verbose=False, jobs=None, progress=True) == 1
    assert mock_run.call_args.args[0] == ["/tools/ninja", "all"]
    assert mock_run.call_args.kwargs["filter_lines"] is toolchain.FILTER_IDF_LINES
    assert mock_run.call_args.kwargs["progress"] is True
    assert "ninja all failed with exit code 1" in caplog.text


@pytest.mark.parametrize("reconfigure_rc", [0, 5])
def test_run_compile_reconfigures_when_cache_entries_change(
    setup_core: Path, reconfigure_rc: int
) -> None:
    """A changed -D entry (ccache toggled) reconfigures, as idf.py build did."""
    _setup_build(setup_core)
    with (
        patch.object(toolchain, "need_reconfigure", return_value=False),
        patch.object(toolchain, "_cache_entries_changed", return_value=True),
        patch.object(
            toolchain, "run_reconfigure", return_value=reconfigure_rc
        ) as mock_reconfigure,
        patch.object(toolchain, "_run_ninja", return_value=0) as mock_ninja,
        patch.object(toolchain, "print_summary"),
    ):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=True) == reconfigure_rc
    mock_reconfigure.assert_called_once_with(True)
    assert mock_ninja.called is (reconfigure_rc == 0)


@pytest.mark.parametrize("failing", ["all", "size"])
def test_run_compile_stops_on_ninja_failure(setup_core: Path, failing: str) -> None:
    """A failed build skips size; either failure skips the summary."""
    _setup_build(setup_core)
    with (
        patch.object(toolchain, "need_reconfigure", return_value=False),
        patch.object(toolchain, "_cache_entries_changed", return_value=False),
        patch.object(
            toolchain,
            "_run_ninja",
            side_effect=lambda target, **kw: 7 if target == failing else 0,
        ) as mock_ninja,
        patch.object(toolchain, "print_summary") as mock_summary,
    ):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == 7
    targets = [c.args[0] for c in mock_ninja.call_args_list]
    assert targets == (["all"] if failing == "all" else ["all", "size"])
    mock_summary.assert_not_called()


@pytest.mark.parametrize("memory_ld_rc", [0, 3])
def test_run_compile_testing_mode_builds_memory_ld_first(
    setup_core: Path, memory_ld_rc: int
) -> None:
    """Testing mode builds and patches memory.ld before the main build."""
    _setup_build(setup_core)
    CORE.testing_mode = True
    targets: list[str] = []

    def record(target: str, **kwargs: object) -> int:
        targets.append(target)
        return memory_ld_rc if target.endswith("memory.ld") else 0

    with (
        patch.object(toolchain, "need_reconfigure", return_value=False),
        patch.object(toolchain, "_cache_entries_changed", return_value=False),
        patch.object(toolchain, "_run_ninja", side_effect=record),
        patch.object(toolchain, "_patch_memory_segments") as mock_patch,
        patch.object(toolchain, "print_summary"),
    ):
        assert toolchain.run_compile({CONF_ESPHOME: {}}, verbose=False) == memory_ld_rc
    memory_ld = str(Path("esp-idf", "esp_system", "ld", "memory.ld"))
    if memory_ld_rc:
        assert targets == [memory_ld]
        mock_patch.assert_not_called()
    else:
        assert targets == [memory_ld, "all", "size"]
        mock_patch.assert_called_once()


@pytest.mark.parametrize(
    ("version", "ninja", "env", "expected"),
    [
        # 5.x: ninja only, and it overrides the inherited value
        (cv.Version(5, 5, 5), True, {"CLICOLOR_FORCE": "0"}, {"CLICOLOR_FORCE": "1"}),
        (cv.Version(5, 5, 5), False, {}, {}),
        # 6.x: every tool, as defaults, unless NO_COLOR is set
        (
            cv.Version(6, 1, 0),
            False,
            {},
            {"CLICOLOR_FORCE": "1", "FORCE_COLOR": "1"},
        ),
        (
            cv.Version(6, 1, 0),
            True,
            {"CLICOLOR_FORCE": "0"},
            {"CLICOLOR_FORCE": "0", "FORCE_COLOR": "1"},
        ),
        (cv.Version(6, 1, 0), True, {"NO_COLOR": "1"}, {}),
    ],
    ids=["5-ninja", "5-cmake", "6-cmake", "6-user-value", "6-no-color"],
)
def test_tool_env_colors_follow_idf_py(
    setup_core: Path,
    version: cv.Version,
    ninja: bool,
    env: dict[str, str],
    expected: dict[str, str],
) -> None:
    """IDF 5 run_target and IDF 6 RunTool.__call__ color handling."""
    _setup_build(setup_core)
    CORE.data[KEY_ESP32][KEY_IDF_VERSION] = version
    with _fake_tools(env):
        result = toolchain._tool_env(ninja=ninja)
    colors = {k: v for k, v in result.items() if k in ("CLICOLOR_FORCE", "FORCE_COLOR")}
    assert colors == expected
