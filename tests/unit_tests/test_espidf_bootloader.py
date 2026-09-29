"""Tests for esphome.espidf.bootloader."""

# pylint: disable=protected-access

from collections.abc import Iterator
from contextlib import contextmanager
import json
import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.build_gen import espidf as build_gen
from esphome.components.esp32.const import KEY_ESP32, KEY_IDF_VERSION, KEY_VARIANT
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError
from esphome.espidf import bootloader, toolchain


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


def _tools_prefix(tmp_path: Path):
    from esphome.espidf import framework

    return patch.object(framework, "get_idf_tools_path", return_value=tmp_path)


def test_enabled_clean_snapshot_checks_macro(tmp_path: Path) -> None:
    _write_snapshot("CONFIG_FOO=y\n")
    with (
        _tools_prefix(tmp_path),
        patch.object(build_gen, "idf_macro_matches", return_value=True),
    ):
        assert bootloader._compute_enabled() is True


def test_enabled_macro_mismatch_disables_with_a_log(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _write_snapshot("CONFIG_FOO=y\n")
    with (
        _tools_prefix(tmp_path),
        patch.object(build_gen, "idf_macro_matches", return_value=False),
        caplog.at_level("INFO"),
    ):
        assert bootloader._compute_enabled() is False
    assert "changed its bootloader macro" in caplog.text


def test_enabled_after_recorded_failure(tmp_path: Path) -> None:
    """A recorded failure pins in-tree until the IDF version or target changes."""
    _write_snapshot("CONFIG_FOO=y\n")
    with (
        _tools_prefix(tmp_path),
        patch.object(build_gen, "idf_macro_matches", return_value=True),
    ):
        bootloader.record_failure()
        assert toolchain._cache().bootloader_enabled is False  # this run
        assert bootloader._compute_enabled() is False  # later runs
        CORE.data[KEY_ESP32][KEY_IDF_VERSION] = cv.Version(6, 1, 0)
        assert bootloader._compute_enabled() is True


def test_enabled_readonly_tools_prefix_disables(tmp_path: Path) -> None:
    """A shared read-only prefix would fail the cache on every build."""
    _write_snapshot("CONFIG_FOO=y\n")
    with (
        _tools_prefix(tmp_path),
        patch.object(bootloader.os, "access", return_value=False),
    ):
        assert bootloader._compute_enabled() is False


def test_enabled_swallows_errors_as_disabled(tmp_path: Path) -> None:
    """Any failure while deciding must read as disabled, never raise."""
    _write_snapshot("CONFIG_FOO=y\n")
    with (
        _tools_prefix(tmp_path),
        patch.object(toolchain, "_get_idf_path", side_effect=OSError("boom")),
    ):
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


# ------------------------------------------------------------------ cache key


def _key(
    names: list[str],
    config: dict,
    compiler: str = "/tc/gcc",
    version: str = "5.5.5",
) -> str:
    from esphome.espidf import framework

    with (
        patch.object(toolchain, "_get_core_framework_version", return_value=version),
        patch.object(toolchain, "_get_framework_source_override", return_value=None),
        patch.object(framework, "read_idf_version_txt", return_value="v5.5.5"),
    ):
        return bootloader._compute_key(bootloader._key_payload(names, config, compiler))


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
    CORE.data[KEY_ESP32][KEY_IDF_VERSION] = cv.Version(6, 1, 0)
    assert _key(["A"], {"A": "1"}, version="6.1.0") != base


def test_key_ignores_unlisted_config_values() -> None:
    """Only the names the bootloader consumes participate."""
    assert _key(["A"], {"A": "1", "Z": "app-only"}) == _key(["A"], {"A": "1"})


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


@contextmanager
def _standalone_tools(run_rc: int) -> Iterator:
    """Stub the tool lookup and runner; yield the run_build_tool mock."""
    with (
        patch.object(toolchain, "_get_idf_tool", side_effect=lambda n: f"/tools/{n}"),
        patch.object(toolchain, "_get_idf_path", return_value=Path("/idf")),
        patch.object(toolchain, "_tool_env", return_value={"A": "1"}),
        patch.object(bootloader, "run_build_tool", return_value=run_rc) as mock_run,
    ):
        yield mock_run


def test_build_standalone_runs_cmake_then_ninja(tmp_path: Path) -> None:
    build_dir = tmp_path / "work"
    with _standalone_tools(0) as mock_run:
        assert bootloader._build_standalone(build_dir, verbose=True) == 0
    cmake_call, ninja_call = mock_run.call_args_list
    assert cmake_call.args[0][0] == "/tools/cmake"
    assert cmake_call.args[0][-1] == f"{Path('/idf')}/components/bootloader/subproject"
    assert cmake_call.kwargs["cwd"] == build_dir
    assert cmake_call.kwargs["filter_lines"] is None  # verbose
    assert cmake_call.kwargs["log_path"] == build_dir / "log" / "cmake_output.log"
    assert ninja_call.args[0] == ["/tools/ninja"]


def test_build_standalone_failure_prints_hints(tmp_path: Path) -> None:
    build_dir = tmp_path / "work"
    with (
        _standalone_tools(2) as mock_run,
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


def _make_built_tree_rc(build_dir: Path, verbose: bool) -> int:
    """A _build_standalone stand-in that succeeds with a full output tree."""
    _make_built_tree(build_dir)
    return 0


def test_publish_creates_entry_with_the_key_payload(tmp_path: Path) -> None:
    root = tmp_path / "cache"
    build = tmp_path / "work"
    _make_built_tree(build)
    with patch.object(bootloader, "_cache_root", return_value=root):
        entry = bootloader._publish(build, "k" * 16, {"idf": "5.5.5"})
    assert entry == root / ("k" * 16)
    assert (entry / "bootloader.bin").read_bytes() == b"bootloader.bin"
    # meta.json is the full key payload, for debugging cache misses.
    assert json.loads((entry / "meta.json").read_text()) == {"idf": "5.5.5"}
    assert not list(root.glob(".stage-*"))


def test_publish_lost_race_reuses_winner(tmp_path: Path) -> None:
    """A rename beaten by another process keeps that process's entry."""
    root = tmp_path / "cache"
    build = tmp_path / "work"
    _make_built_tree(build)
    entry = root / ("k" * 16)

    def rename_raises(src: Path, dst: Path) -> None:
        entry.mkdir(parents=True)
        (entry / "bootloader.bin").write_bytes(b"winner")
        raise OSError("directory not empty")

    with (
        patch.object(bootloader, "_cache_root", return_value=root),
        patch.object(bootloader, "_rename_with_retry", rename_raises),
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
        patch.object(bootloader, "_rename_with_retry", side_effect=OSError("denied")),
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
    assert bootloader._install_into_build(entry) == dest
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
    assert bootloader.ensure_cached_bootloader() is False


@contextmanager
def _orchestration_env(tmp_path: Path) -> Iterator[Path]:
    """A configured app tree plus the patches every ensure_* test shares;
    yields the cache root."""
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
    with (
        patch.object(bootloader, "_cache_root", return_value=root),
        patch.object(toolchain, "_resolved_c_compiler", return_value="/tc/gcc"),
        patch.object(bootloader, "_key_payload", return_value={}),
        patch.object(bootloader, "_compute_key", return_value="deadbeefdeadbeef"),
    ):
        yield root


def test_ensure_cache_hit_installs_without_building(tmp_path: Path) -> None:
    with (
        _orchestration_env(tmp_path) as root,
        patch.object(bootloader, "_build_standalone") as mock_build,
    ):
        (root / "deadbeefdeadbeef").mkdir(parents=True)
        for name in bootloader._OUTPUTS:
            (root / "deadbeefdeadbeef" / name).write_bytes(b"\xe9" * 64)
        (root / "config_names.json").write_text('["A"]')
        stamp = bootloader._failure_stamp()
        stamp.write_text("{}")
        assert bootloader.ensure_cached_bootloader() is True
    mock_build.assert_not_called()
    assert not stamp.exists()  # success clears a stale failure stamp
    installed = CORE.relative_build_path("build", "bootloader", "bootloader.bin")
    assert installed.read_bytes() == b"\xe9" * 64


def test_ensure_cache_miss_builds_and_publishes(tmp_path: Path) -> None:
    """Known names but no matching entry: build, harvest, publish."""

    with (
        _orchestration_env(tmp_path) as root,
        patch.object(bootloader, "_build_standalone", side_effect=_make_built_tree_rc),
    ):
        root.mkdir(parents=True)
        (root / "config_names.json").write_text('["A"]')
        assert bootloader.ensure_cached_bootloader() is True
    # Entry published under the key, names harvested, work dir removed.
    assert (root / "deadbeefdeadbeef" / "bootloader.bin").is_file()
    assert "A" in json.loads((root / "config_names.json").read_text())
    assert not list(root.glob(".build-*"))
    installed = CORE.relative_build_path("build", "bootloader", "bootloader.bin")
    assert installed.is_file()


def test_ensure_returns_build_failure(tmp_path: Path) -> None:
    with (
        _orchestration_env(tmp_path) as root,
        patch.object(bootloader, "_build_standalone", return_value=2),
    ):
        assert bootloader.ensure_cached_bootloader() is False
    assert not list(root.glob(".build-*"))


def test_ensure_soft_fails_when_build_yields_no_config(tmp_path: Path) -> None:
    def build_without_config(build_dir: Path, verbose: bool) -> int:
        build_dir.mkdir(parents=True, exist_ok=True)
        return 0

    with (
        _orchestration_env(tmp_path),
        patch.object(bootloader, "_build_standalone", side_effect=build_without_config),
    ):
        assert bootloader.ensure_cached_bootloader() is False


@pytest.mark.parametrize(
    "err", [OSError("disk"), EsphomeError("write failed")], ids=["oserror", "esphome"]
)
def test_ensure_turns_cache_errors_into_a_soft_failure(
    tmp_path: Path, err: Exception
) -> None:
    """Any surprise while using the cache means fall back, never crash."""
    with (
        _orchestration_env(tmp_path),
        patch.object(bootloader, "_publish", side_effect=err),
        patch.object(bootloader, "_build_standalone", side_effect=_make_built_tree_rc),
    ):
        assert bootloader.ensure_cached_bootloader() is False


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


def _flash_build(tmp_path: Path, cached: bool = True, offset: int = 0x1000) -> Path:
    build = tmp_path / "build"
    (build / "config").mkdir(parents=True)
    (build / "config" / "sdkconfig.json").write_text(
        json.dumps({"BOOTLOADER_OFFSET_IN_FLASH": offset})
    )
    (build / "CMakeCache.txt").write_text(
        f"ESPHOME_USE_CACHED_BOOTLOADER:UNINITIALIZED={int(cached)}\n"
    )
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


def test_inject_noop_on_a_stock_tree(tmp_path: Path) -> None:
    """IDF omits the entry on purpose for some secure-boot builds; a tree
    not in cached mode must stay exactly as IDF wrote it."""
    build = _flash_build(tmp_path, cached=False)
    flash_data: dict = {"flash_files": {}}
    bootloader.inject_bootloader_flash_file(flash_data, build)
    assert flash_data == {"flash_files": {}}


def test_tree_uses_cached_bootloader(tmp_path: Path) -> None:
    assert bootloader.tree_uses_cached_bootloader(tmp_path) is False
    cache = tmp_path / "CMakeCache.txt"
    cache.write_text("ESPHOME_USE_CACHED_BOOTLOADER:UNINITIALIZED=0\n")
    assert bootloader.tree_uses_cached_bootloader(tmp_path) is False
    cache.write_text("ESPHOME_USE_CACHED_BOOTLOADER:UNINITIALIZED=1\n")
    assert bootloader.tree_uses_cached_bootloader(tmp_path) is True


def test_inject_noop_without_cmakecache(tmp_path: Path) -> None:
    flash_data: dict = {"flash_files": {}}
    bootloader.inject_bootloader_flash_file(flash_data, tmp_path / "missing")
    assert flash_data == {"flash_files": {}}


def test_inject_missing_pieces_log_an_error(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Cached mode without a bin or config is an incomplete factory image."""
    build = _flash_build(tmp_path)
    (build / "config" / "sdkconfig.json").unlink()
    flash_data: dict = {"flash_files": {}}
    with caplog.at_level("ERROR"):
        bootloader.inject_bootloader_flash_file(flash_data, build)
    assert flash_data == {"flash_files": {}}
    assert "factory image has no bootloader" in caplog.text
