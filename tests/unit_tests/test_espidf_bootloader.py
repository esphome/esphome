"""Tests for esphome.espidf.bootloader."""

# pylint: disable=protected-access

import json
import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.esp32.const import KEY_ESP32, KEY_IDF_VERSION, KEY_VARIANT
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError
from esphome.espidf import bootloader, toolchain

# The macro body as shipped in build.cmake; byte identical in IDF 5.5.5 and
# 6.1.0, so one fixture covers both supported versions.
IDF_BUILD_CMAKE = """\
some_other_cmake()

macro(__build_process_project_includes)
    # Include the sdkconfig cmake file, since the following operations require
    # knowledge of config values.
    idf_build_get_property(sdkconfig_cmake SDKCONFIG_CMAKE)
    include(${sdkconfig_cmake})

    # Make each build property available as a read-only variable
    idf_build_get_property(build_properties __BUILD_PROPERTIES)
    foreach(build_property ${build_properties})
        idf_build_get_property(val ${build_property})
        set(${build_property} "${val}")
    endforeach()

    idf_build_get_property(build_component_targets __BUILD_COMPONENT_TARGETS)

    # Include each component's project_include.cmake
    foreach(component_target ${build_component_targets})
        __component_get_property(dir ${component_target} COMPONENT_DIR)
        __component_get_property(_name ${component_target} COMPONENT_NAME)
        set(COMPONENT_NAME ${_name})
        set(COMPONENT_DIR ${dir})
        set(COMPONENT_PATH ${dir})  # this is deprecated, users are encouraged to use COMPONENT_DIR;
                                    # retained for compatibility
        if(EXISTS ${COMPONENT_DIR}/project_include.cmake)
            include(${COMPONENT_DIR}/project_include.cmake)
        endif()
    endforeach()
endmacro()
"""


@pytest.fixture(autouse=True)
def _fresh_cache(setup_core: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    """Point CORE at a build tree and reset the per-run toolchain cache."""
    monkeypatch.delenv(bootloader.BOOTLOADER_CACHE_ENV, raising=False)
    monkeypatch.delenv("IDF_PATH", raising=False)
    CORE.name = "dev"
    CORE.build_path = setup_core / "build" / "dev"
    CORE.data[KEY_ESP32] = {
        KEY_VARIANT: "ESP32",
        KEY_IDF_VERSION: cv.Version(5, 5, 5),
    }
    CORE.data.pop(toolchain.DOMAIN, None)


def _write_snapshot(text: str = "") -> Path:
    path = CORE.relative_build_path("sdkconfig.dev.esphomeinternal")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return path


def _write_idf_build_cmake(tmp_path: Path, text: str = IDF_BUILD_CMAKE) -> Path:
    idf = tmp_path / "idf"
    (idf / "tools" / "cmake").mkdir(parents=True)
    (idf / "tools" / "cmake" / "build.cmake").write_text(text)
    return idf


# ---------------------------------------------------------------- predicate


def test_enabled_env_zero_disables(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv(bootloader.BOOTLOADER_CACHE_ENV, "0")
    assert bootloader._compute_enabled() is False


def test_enabled_user_idf_path_disables(monkeypatch: pytest.MonkeyPatch) -> None:
    """An unmanaged IDF checkout can change under the cache."""
    monkeypatch.setenv("IDF_PATH", "/somewhere")
    assert bootloader._compute_enabled() is False


def test_enabled_missing_snapshot_disables() -> None:
    """No snapshot means the config origin is unknown; fail safe."""
    assert bootloader._compute_enabled() is False


def test_enabled_secure_option_disables() -> None:
    _write_snapshot("CONFIG_SECURE_BOOT=y\n")
    assert bootloader._compute_enabled() is False


def test_enabled_clean_snapshot_checks_macro(tmp_path: Path) -> None:
    _write_snapshot("CONFIG_FOO=y\n")
    idf = _write_idf_build_cmake(tmp_path)
    with patch.object(toolchain, "_get_idf_path", return_value=idf):
        assert bootloader._compute_enabled() is True


def test_enabled_swallows_errors_as_disabled(tmp_path: Path) -> None:
    """Any failure while deciding must read as disabled, never raise."""
    _write_snapshot("CONFIG_FOO=y\n")
    with patch.object(toolchain, "_get_idf_path", side_effect=OSError("boom")):
        assert bootloader._compute_enabled() is False


def test_bootloader_cache_enabled_is_cached_per_run() -> None:
    with patch.object(
        bootloader, "_compute_enabled", return_value=True
    ) as mock_compute:
        assert bootloader.bootloader_cache_enabled() is True
        assert bootloader.bootloader_cache_enabled() is True
    mock_compute.assert_called_once_with()


@pytest.mark.parametrize(
    ("text", "secure"),
    [
        ("CONFIG_SECURE_BOOT=y\n", True),
        ("CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT=y\n", True),
        ("CONFIG_FLASH_ENCRYPTION_ENABLED=y\n", True),
        ('CONFIG_SECURE_BOOT_SIGNING_KEY="key.pem"\n', True),
        ("CONFIG_SECURE_BOOT=n\n", False),
        ("CONFIG_SECURE_BOOT=0\n", False),
        ('CONFIG_SECURE_BOOT_SIGNING_KEY=""\n', False),
        ("# CONFIG_SECURE_BOOT is not set\n", False),
        ("CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y\n", False),
        ("", False),
    ],
    ids=[
        "secure-boot",
        "signed-apps",
        "flash-encryption",
        "signing-key",
        "value-n",
        "value-0",
        "empty-string",
        "comment-line",
        "unrelated",
        "empty-file",
    ],
)
def test_has_secure_options(tmp_path: Path, text: str, secure: bool) -> None:
    """Only an enabled secure option bypasses; disabled values are the
    compiled-out state ESPHome writes by default."""
    path = tmp_path / "sdkconfig"
    path.write_text(text)
    assert bootloader._has_secure_options(path) is secure


# ------------------------------------------------------------ macro tripwire


def test_normalized_macro_strips_comments_and_whitespace() -> None:
    text = "macro(__build_process_project_includes)\n  a( b )  # tail\n\n  # only\n  c(d)\nendmacro()"
    assert bootloader._normalized_macro(text) == ["a( b )", "c(d)"]


def test_normalized_macro_none_without_macro() -> None:
    assert bootloader._normalized_macro("nothing here") is None


def test_idf_macro_matches_the_shipped_body(tmp_path: Path) -> None:
    """The override's embedded copy must equal what build.cmake ships."""
    idf = _write_idf_build_cmake(tmp_path)
    with patch.object(toolchain, "_get_idf_path", return_value=idf):
        assert bootloader.idf_macro_matches() is True


def test_idf_macro_mismatch_detected(tmp_path: Path) -> None:
    changed = IDF_BUILD_CMAKE.replace(
        "include(${sdkconfig_cmake})", "include(${sdkconfig_cmake} NEW_ARG)"
    )
    idf = _write_idf_build_cmake(tmp_path, changed)
    with patch.object(toolchain, "_get_idf_path", return_value=idf):
        assert bootloader.idf_macro_matches() is False


# ------------------------------------------------------------------ cache key


def _key(names: list[str], config: dict, compiler: str = "/tc/gcc") -> str:
    with (
        patch.object(toolchain, "_get_core_framework_version", return_value="5.5.5"),
        patch.object(toolchain, "_get_framework_source_override", return_value=None),
        patch.object(bootloader, "_version_stamp", return_value="v5.5.5"),
    ):
        return bootloader._compute_key(names, config, compiler)


def test_key_is_stable_and_name_order_independent() -> None:
    config = {"A": "1", "B": True}
    key = _key(["A", "B"], config)
    assert key == _key(["B", "A"], config)
    assert len(key) == 16


def test_key_distinguishes_absent_from_empty() -> None:
    """An option missing from the app config is not the same as ''."""
    assert _key(["A"], {"A": ""}) != _key(["A"], {})


def test_key_changes_with_each_input() -> None:
    base = _key(["A"], {"A": "1"})
    assert _key(["A"], {"A": "2"}) != base
    assert _key(["A"], {"A": "1"}, compiler="/other/gcc") != base
    with (
        patch.object(toolchain, "_get_core_framework_version", return_value="6.1.0"),
        patch.object(toolchain, "_get_framework_source_override", return_value=None),
        patch.object(bootloader, "_version_stamp", return_value="v5.5.5"),
    ):
        CORE.data[KEY_ESP32][KEY_IDF_VERSION] = cv.Version(6, 1, 0)
        assert bootloader._compute_key(["A"], {"A": "1"}, "/tc/gcc") != base


def test_key_ignores_unlisted_config_values() -> None:
    """Only the names the bootloader consumes participate."""
    assert _key(["A"], {"A": "1", "Z": "app-only"}) == _key(["A"], {"A": "1"})


def test_version_stamp_reads_version_txt(tmp_path: Path) -> None:
    (tmp_path / "version.txt").write_text("v5.5.5\n")
    with patch.object(toolchain, "_get_idf_path", return_value=tmp_path):
        assert bootloader._version_stamp() == "v5.5.5"


def test_version_stamp_missing_file(tmp_path: Path) -> None:
    with patch.object(toolchain, "_get_idf_path", return_value=tmp_path):
        assert bootloader._version_stamp() == ""


def test_cache_root_layout(tmp_path: Path) -> None:
    """Entries live under the registered IDF tools cache, per version and
    target, so clean-all removes them with the toolchains."""
    from esphome.espidf import framework

    with (
        patch.object(framework, "get_idf_tools_path", return_value=tmp_path),
        patch.object(toolchain, "_get_core_framework_version", return_value="5.5.5"),
    ):
        assert bootloader._cache_root() == tmp_path / "bootloaders" / "5.5.5" / "esp32"


# ----------------------------------------------------------- config names


def test_merge_config_names_unions_and_sorts(tmp_path: Path) -> None:
    root = tmp_path / "cache"
    with patch.object(bootloader, "_cache_root", return_value=root):
        assert bootloader._merge_config_names(["B", "A"]) == ["A", "B"]
        assert bootloader._merge_config_names(["C", "A"]) == ["A", "B", "C"]
        assert bootloader._load_config_names() == ["A", "B", "C"]


@pytest.mark.parametrize(
    "content",
    [None, "not json", "[]", '["a", 1]', '"str"'],
    ids=["missing", "corrupt", "empty", "mixed-types", "not-a-list"],
)
def test_load_config_names_rejects_bad_files(
    tmp_path: Path, content: str | None
) -> None:
    if content is not None:
        (tmp_path / "config_names.json").write_text(content)
    with patch.object(bootloader, "_cache_root", return_value=tmp_path):
        assert bootloader._load_config_names() is None


def test_load_build_config(tmp_path: Path) -> None:
    assert bootloader._load_build_config(tmp_path) is None
    (tmp_path / "config").mkdir()
    (tmp_path / "config" / "sdkconfig.json").write_text('{"A": 1}')
    assert bootloader._load_build_config(tmp_path) == {"A": 1}


# ------------------------------------------------------------- compiler id


def test_compiler_id_from_compile_commands(tmp_path: Path) -> None:
    build = CORE.relative_build_path("build")
    build.mkdir(parents=True)
    (build / "compile_commands.json").write_text(
        json.dumps([{"command": "/tools/xtensa-esp32-elf-gcc -c a.c"}])
    )
    assert bootloader._compiler_id() == "/tools/xtensa-esp32-elf-gcc"


def test_compiler_id_falls_back_to_cmakecache(tmp_path: Path) -> None:
    build = CORE.relative_build_path("build")
    build.mkdir(parents=True)
    (build / "CMakeCache.txt").write_text(
        "CMAKE_C_COMPILER_AR:FILEPATH=/tools/xtensa-esp32-elf-gcc-ar\n"
    )
    assert bootloader._compiler_id() == "/tools/xtensa-esp32-elf-gcc-ar"


def test_compiler_id_none_without_configure_outputs() -> None:
    assert bootloader._compiler_id() is None


# ------------------------------------------------------------- cmake args


def test_subproject_args_for_idf_5() -> None:
    args = bootloader._subproject_cmake_args("cmake", "/py", "/sdk", "/idf", "/proj")
    assert args == [
        "cmake",
        "-G",
        "Ninja",
        "-DSDKCONFIG=/sdk",
        "-DIDF_PATH=/idf",
        "-DIDF_TARGET=esp32",
        "-DPYTHON_DEPS_CHECKED=1",
        "-DPYTHON=/py",
        "-DEXTRA_COMPONENT_DIRS=/idf/components/bootloader",
        "-DPROJECT_SOURCE_DIR=/proj",
        "-DIGNORE_EXTRA_COMPONENT=",
        "/idf/components/bootloader/subproject",
    ]


def test_subproject_args_for_idf_6() -> None:
    """6.x dropped the bootloader dir self-append and forwards IDF_BUILD_V2."""
    CORE.data[KEY_ESP32][KEY_IDF_VERSION] = cv.Version(6, 1, 0)
    args = bootloader._subproject_cmake_args("cmake", "/py", "/sdk", "/idf", "/proj")
    assert "-DEXTRA_COMPONENT_DIRS=" in args
    assert "-DIDF_BUILD_V2=" in args
    assert "-DEXTRA_COMPONENT_DIRS=/idf/components/bootloader" not in args
    assert args[-1] == "/idf/components/bootloader/subproject"


# -------------------------------------------------------- standalone build


def test_build_standalone_runs_cmake_then_ninja(tmp_path: Path) -> None:
    build_dir = tmp_path / "work"
    with (
        patch.object(toolchain, "_get_idf_tool", side_effect=lambda n: f"/tools/{n}"),
        patch.object(toolchain, "_get_idf_path", return_value=Path("/idf")),
        patch.object(toolchain, "_tool_env", return_value={"A": "1"}),
        patch.object(bootloader, "run_build_tool", return_value=0) as mock_run,
    ):
        assert bootloader._build_standalone(build_dir, verbose=True) == 0
    cmake_call, ninja_call = mock_run.call_args_list
    assert cmake_call.args[0][0] == "/tools/cmake"
    assert cmake_call.args[0][-1] == "/idf/components/bootloader/subproject"
    assert cmake_call.kwargs["cwd"] == build_dir
    assert cmake_call.kwargs["filter_lines"] is None  # verbose
    assert cmake_call.kwargs["log_path"] == build_dir / "log" / "cmake_output.log"
    assert ninja_call.args[0] == ["/tools/ninja"]


def test_build_standalone_failure_prints_hints(tmp_path: Path) -> None:
    build_dir = tmp_path / "work"
    with (
        patch.object(toolchain, "_get_idf_tool", side_effect=lambda n: f"/tools/{n}"),
        patch.object(toolchain, "_get_idf_path", return_value=Path("/idf")),
        patch.object(toolchain, "_tool_env", return_value={}),
        patch.object(bootloader, "run_build_tool", return_value=2) as mock_run,
        patch.object(toolchain, "_print_hints") as mock_hints,
    ):
        assert bootloader._build_standalone(build_dir, verbose=False) == 2
    # Failed at cmake; ninja never ran, the hint scanner got the log.
    assert len(mock_run.call_args_list) == 1
    assert mock_run.call_args.kwargs["filter_lines"] is toolchain.FILTER_IDF_LINES
    mock_hints.assert_called_once_with(build_dir / "log" / "cmake_output.log")


# ----------------------------------------------------------------- publish


def _make_built_tree(build_dir: Path) -> None:
    build_dir.mkdir(parents=True, exist_ok=True)
    for name in bootloader._OUTPUTS:
        (build_dir / name).write_bytes(name.encode())
    (build_dir / "config").mkdir(exist_ok=True)
    (build_dir / "config" / "sdkconfig.json").write_text('{"A": "1"}')


def test_publish_creates_entry(tmp_path: Path) -> None:
    root = tmp_path / "cache"
    build = tmp_path / "work"
    _make_built_tree(build)
    with patch.object(bootloader, "_cache_root", return_value=root):
        entry = bootloader._publish(build, "k" * 16, {"idf": "5.5.5"})
    assert entry == root / ("k" * 16)
    assert (entry / "bootloader.bin").read_bytes() == b"bootloader.bin"
    assert json.loads((entry / "meta.json").read_text()) == {"idf": "5.5.5"}
    assert not list(root.glob(".stage-*"))


def test_publish_existing_entry_short_circuits(tmp_path: Path) -> None:
    root = tmp_path / "cache"
    entry = root / ("k" * 16)
    entry.mkdir(parents=True)
    (entry / "bootloader.bin").write_bytes(b"winner")
    with patch.object(bootloader, "_cache_root", return_value=root):
        assert bootloader._publish(tmp_path / "work", "k" * 16, {}) == entry
    assert (entry / "bootloader.bin").read_bytes() == b"winner"


def test_publish_lost_race_reuses_winner(tmp_path: Path) -> None:
    """A rename beaten by another process keeps that process's entry."""
    root = tmp_path / "cache"
    build = tmp_path / "work"
    _make_built_tree(build)
    entry = root / ("k" * 16)

    def rename_raises(self: Path, target: Path) -> None:
        entry.mkdir(parents=True)
        (entry / "bootloader.bin").write_bytes(b"winner")
        raise OSError("directory not empty")

    with (
        patch.object(bootloader, "_cache_root", return_value=root),
        patch.object(Path, "rename", rename_raises),
    ):
        assert bootloader._publish(build, "k" * 16, {}) == entry
    assert (entry / "bootloader.bin").read_bytes() == b"winner"
    assert not list(root.glob(".stage-*"))


def test_publish_rename_failure_without_winner_raises(tmp_path: Path) -> None:
    root = tmp_path / "cache"
    build = tmp_path / "work"
    _make_built_tree(build)
    with (
        patch.object(bootloader, "_cache_root", return_value=root),
        patch.object(Path, "rename", side_effect=OSError("denied")),
        pytest.raises(OSError),
    ):
        bootloader._publish(build, "k" * 16, {})


# ----------------------------------------------------------------- install


def test_install_into_build_replaces_stale_subbuild(tmp_path: Path) -> None:
    entry = tmp_path / "entry"
    entry.mkdir()
    # No .map: an optional output missing from the entry is skipped.
    (entry / "bootloader.bin").write_bytes(b"bin")
    (entry / "bootloader.elf").write_bytes(b"elf")
    dest = CORE.relative_build_path("build", "bootloader")
    dest.mkdir(parents=True)
    (dest / "CMakeCache.txt").write_text("stale in-tree sub-build")
    (dest / "old.obj").write_bytes(b"x")
    bootloader._install_into_build(entry)
    assert (dest / "bootloader.bin").read_bytes() == b"bin"
    assert not (dest / "CMakeCache.txt").exists()
    assert not (dest / "old.obj").exists()
    assert not (dest / "bootloader.map").exists()


# -------------------------------------------------------------- size check


def test_size_check_passes_and_logs(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    bin_path = tmp_path / "bootloader.bin"
    bin_path.write_bytes(b"\xe9" * 0x6620)
    config = {"BOOTLOADER_OFFSET_IN_FLASH": 0x1000, "PARTITION_TABLE_OFFSET": 0x8000}
    with caplog.at_level("INFO"):
        bootloader._check_bootloader_size(bin_path, config)
    assert "0x9e0 bytes (9%) free" in caplog.text


def test_size_check_overflow_raises(tmp_path: Path) -> None:
    bin_path = tmp_path / "bootloader.bin"
    bin_path.write_bytes(b"\xe9" * 0x7100)
    config = {"BOOTLOADER_OFFSET_IN_FLASH": 0x1000, "PARTITION_TABLE_OFFSET": 0x8000}
    with pytest.raises(EsphomeError, match="overflows by 0x100 bytes"):
        bootloader._check_bootloader_size(bin_path, config)


# ------------------------------------------------------------ orchestration


def test_ensure_fails_soft_without_configure_outputs() -> None:
    """No app config or compiler id means fall back, not crash."""
    assert bootloader.ensure_cached_bootloader() == 1


def _orchestration_env(tmp_path: Path):
    """Patches shared by the ensure_cached_bootloader tests."""
    root = tmp_path / "cache-root"
    build = CORE.relative_build_path("build")
    build.mkdir(parents=True, exist_ok=True)
    (build / "config").mkdir(exist_ok=True)
    (build / "config" / "sdkconfig.json").write_text(
        json.dumps(
            {
                "A": "1",
                "BOOTLOADER_OFFSET_IN_FLASH": 0x1000,
                "PARTITION_TABLE_OFFSET": 0x8000,
            }
        )
    )
    return root


def test_ensure_cache_hit_installs_without_building(tmp_path: Path) -> None:
    root = _orchestration_env(tmp_path)
    with (
        patch.object(bootloader, "_cache_root", return_value=root),
        patch.object(bootloader, "_compiler_id", return_value="/tc/gcc"),
        patch.object(bootloader, "_compute_key", return_value="deadbeefdeadbeef"),
        patch.object(bootloader, "_build_standalone") as mock_build,
    ):
        (root / "deadbeefdeadbeef").mkdir(parents=True)
        for name in bootloader._OUTPUTS:
            (root / "deadbeefdeadbeef" / name).write_bytes(b"\xe9" * 64)
        (root / "config_names.json").write_text('["A"]')
        assert bootloader.ensure_cached_bootloader() == 0
    mock_build.assert_not_called()
    installed = CORE.relative_build_path("build", "bootloader", "bootloader.bin")
    assert installed.read_bytes() == b"\xe9" * 64


def test_ensure_cache_miss_builds_and_publishes(tmp_path: Path) -> None:
    """Known names but no matching entry: build, harvest, publish."""
    root = _orchestration_env(tmp_path)
    root.mkdir(parents=True)
    (root / "config_names.json").write_text('["A"]')

    def fake_build(build_dir: Path, verbose: bool) -> int:
        _make_built_tree(build_dir)
        return 0

    with (
        patch.object(bootloader, "_cache_root", return_value=root),
        patch.object(bootloader, "_compiler_id", return_value="/tc/gcc"),
        patch.object(bootloader, "_compute_key", return_value="deadbeefdeadbeef"),
        patch.object(bootloader, "_build_standalone", side_effect=fake_build),
    ):
        assert bootloader.ensure_cached_bootloader() == 0
    # Entry published under the key, names harvested, work dir removed.
    assert (root / "deadbeefdeadbeef" / "bootloader.bin").is_file()
    with patch.object(bootloader, "_cache_root", return_value=root):
        assert "A" in bootloader._load_config_names()
    assert not list(root.glob(".build-*"))
    installed = CORE.relative_build_path("build", "bootloader", "bootloader.bin")
    assert installed.is_file()


def test_ensure_returns_build_failure(tmp_path: Path) -> None:
    root = _orchestration_env(tmp_path)
    with (
        patch.object(bootloader, "_cache_root", return_value=root),
        patch.object(bootloader, "_compiler_id", return_value="/tc/gcc"),
        patch.object(bootloader, "_build_standalone", return_value=2),
    ):
        assert bootloader.ensure_cached_bootloader() == 2
    assert not list(root.glob(".build-*"))


def test_ensure_soft_fails_when_build_yields_no_config(tmp_path: Path) -> None:
    root = _orchestration_env(tmp_path)

    def build_without_config(build_dir: Path, verbose: bool) -> int:
        build_dir.mkdir(parents=True, exist_ok=True)
        return 0

    with (
        patch.object(bootloader, "_cache_root", return_value=root),
        patch.object(bootloader, "_compiler_id", return_value="/tc/gcc"),
        patch.object(bootloader, "_build_standalone", side_effect=build_without_config),
    ):
        assert bootloader.ensure_cached_bootloader() == 1


def test_prune_removes_only_old_work_dirs(tmp_path: Path) -> None:
    """A concurrent build's fresh work dir must survive the prune."""
    old = tmp_path / ".build-1"
    fresh = tmp_path / ".build-2"
    old.mkdir()
    fresh.mkdir()
    os.utime(old, (0, 0))
    with patch.object(bootloader, "_cache_root", return_value=tmp_path):
        bootloader._prune_stale_dirs()
    assert not old.exists()
    assert fresh.exists()


def test_prune_swallows_cache_root_errors() -> None:
    """Housekeeping never fails a build."""
    with patch.object(bootloader, "_cache_root", side_effect=OSError("gone")):
        bootloader._prune_stale_dirs()


# --------------------------------------------------------- flash injection


def _flash_build(tmp_path: Path, with_bin: bool = True, offset: int = 0x1000) -> Path:
    build = tmp_path / "build"
    (build / "config").mkdir(parents=True)
    (build / "config" / "sdkconfig.json").write_text(
        json.dumps({"BOOTLOADER_OFFSET_IN_FLASH": offset})
    )
    if with_bin:
        (build / "bootloader").mkdir()
        (build / "bootloader" / "bootloader.bin").write_bytes(b"\xe9")
    return build


def test_inject_adds_cached_bootloader(tmp_path: Path) -> None:
    build = _flash_build(tmp_path)
    flash_data = {"flash_files": {"0x10000": "dev.bin"}}
    bootloader.inject_bootloader_flash_file(flash_data, build)
    assert flash_data["flash_files"]["0x1000"] == "bootloader/bootloader.bin"


def test_inject_uses_configured_offset(tmp_path: Path) -> None:
    build = _flash_build(tmp_path, offset=0)
    flash_data: dict = {}
    bootloader.inject_bootloader_flash_file(flash_data, build)
    assert flash_data["flash_files"] == {"0x0": "bootloader/bootloader.bin"}


def test_inject_noop_when_idf_wrote_the_entry(tmp_path: Path) -> None:
    """Stock and bypass trees already carry a bootloader entry."""
    build = _flash_build(tmp_path)
    flash_data = {"flash_files": {"0x1000": "bootloader/bootloader.bin"}}
    bootloader.inject_bootloader_flash_file(flash_data, build)
    assert flash_data["flash_files"] == {"0x1000": "bootloader/bootloader.bin"}


def test_inject_noop_without_bin_or_config(tmp_path: Path) -> None:
    flash_data: dict = {"flash_files": {}}
    bootloader.inject_bootloader_flash_file(
        flash_data, _flash_build(tmp_path / "a", with_bin=False)
    )
    no_config = tmp_path / "b" / "build"
    (no_config / "bootloader").mkdir(parents=True)
    (no_config / "bootloader" / "bootloader.bin").write_bytes(b"\xe9")
    bootloader.inject_bootloader_flash_file(flash_data, no_config)
    assert flash_data == {"flash_files": {}}
