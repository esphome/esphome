"""nrf52 sdk-nrf pch wiring: the CMake consumer block, the prepare wrapper,
and the two-phase west split in run_compile."""

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components import nrf52
from esphome.components.zephyr.const import KEY_BOARD
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION, Toolchain
from esphome.core import CORE, EsphomeError


@pytest.fixture(autouse=True)
def pch_env(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("ESPHOME_PCH_ENABLE", raising=False)
    monkeypatch.delenv("ESPHOME_PCH_STRICT", raising=False)


@pytest.fixture
def build_dir(tmp_path: Path) -> Path:
    d = tmp_path / "build" / ".pioenvs" / "livingroom"
    d.mkdir(parents=True)
    return d


_AUTOCONF_TEXT = "#define CONFIG_GPIO 1\n"


def _write_autoconf(build_dir: Path) -> Path:
    autoconf = build_dir / "zephyr" / "include" / "generated" / "zephyr" / "autoconf.h"
    autoconf.parent.mkdir(parents=True)
    autoconf.write_text(_AUTOCONF_TEXT)
    return autoconf


def test_prepare_pch_disabled_discards_and_degrades(
    monkeypatch: pytest.MonkeyPatch, build_dir: Path
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    gch = build_dir / "esphome_pch.h.gch"
    gch.write_bytes(b"x")
    nrf52._prepare_pch(build_dir)
    assert not gch.exists()


def test_prepare_pch_disabled_strict_raises(
    monkeypatch: pytest.MonkeyPatch, build_dir: Path
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    monkeypatch.setenv("ESPHOME_PCH_STRICT", "1")
    with pytest.raises(EsphomeError, match="ESPHOME_PCH_STRICT"):
        nrf52._prepare_pch(build_dir)


def test_prepare_pch_missing_autoconf_degrades(
    build_dir: Path, caplog: pytest.LogCaptureFixture
) -> None:
    with patch.object(nrf52.pch, "prepare_pch") as prepare:
        nrf52._prepare_pch(build_dir)
    assert not prepare.called
    assert "No autoconf.h found" in caplog.text
    # The header is written first so OBJECT_DEPENDS stays satisfied
    assert (build_dir / "esphome_pch.h").is_file()


def test_prepare_pch_missing_autoconf_strict_raises(
    monkeypatch: pytest.MonkeyPatch, build_dir: Path
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_STRICT", "1")
    with pytest.raises(EsphomeError, match="autoconf.h missing"):
        nrf52._prepare_pch(build_dir)


def test_prepare_pch_unreadable_autoconf_fails_closed(
    build_dir: Path, caplog: pytest.LogCaptureFixture
) -> None:
    # A directory named autoconf.h: read_text raises OSError
    autoconf = build_dir / "zephyr" / "include" / "generated" / "autoconf.h"
    autoconf.mkdir(parents=True)
    with patch.object(nrf52.pch, "prepare_pch") as prepare:
        nrf52._prepare_pch(build_dir)
    assert not prepare.called
    assert "Could not read" in caplog.text


def test_app_build_dir_sysbuild_layout(build_dir: Path) -> None:
    app = build_dir / "zephyr"
    app.mkdir()
    (app / "CMakeCache.txt").write_text("")
    assert nrf52._app_build_dir(build_dir) == app


def test_app_build_dir_top_level_layout(build_dir: Path) -> None:
    # Non-sysbuild: build_dir/zephyr is the Zephyr output dir, no cache
    (build_dir / "zephyr").mkdir()
    assert nrf52._app_build_dir(build_dir) == build_dir


def test_app_build_dir_ignores_cache_directory(build_dir: Path) -> None:
    (build_dir / "zephyr" / "CMakeCache.txt").mkdir(parents=True)
    assert nrf52._app_build_dir(build_dir) == build_dir


def test_app_build_dir_propagates_stat_errors(build_dir: Path) -> None:
    # is_file() would swallow this and mislocate the pch
    with (
        patch.object(Path, "stat", side_effect=PermissionError("denied")),
        pytest.raises(PermissionError),
    ):
        nrf52._app_build_dir(build_dir)


def test_prepare_pch_extras_carry_build_identity(build_dir: Path) -> None:
    _write_autoconf(build_dir)
    with (
        patch.dict(CORE.data, {KEY_CORE: {KEY_FRAMEWORK_VERSION: "2.9.2"}}),
        patch.object(
            nrf52, "zephyr_data", return_value={KEY_BOARD: "adafruit_feather"}
        ),
        patch.object(nrf52, "get_project_compile_flags", return_value=["-Os"]),
        patch.object(nrf52.pch, "prepare_pch") as prepare,
    ):
        nrf52._prepare_pch(build_dir)
    assert (build_dir / "esphome_pch.h").is_file()
    (passed_dir, headers, extras) = prepare.call_args.args
    assert passed_dir == build_dir
    assert headers == nrf52.PCH_DEFAULT_HEADERS
    assert list(extras) == ["2.9.2", "adafruit_feather", _AUTOCONF_TEXT, "-Os"]


def _generate_cmake(tmp_path: Path) -> str:
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"
    CORE.name = "livingroom"
    with (
        patch(
            "esphome.components.zephyr.library.generate_zephyr_modules",
            return_value=[],
        ),
        patch.object(nrf52, "get_project_compile_flags", return_value=["-Os"]),
        patch.object(nrf52, "get_project_link_flags", return_value=[]),
    ):
        nrf52._generate_cmake_lists()
    return (tmp_path / "build" / "zephyr" / "CMakeLists.txt").read_text()


def test_cmake_lists_include_pch_consumer_block(tmp_path: Path) -> None:
    # Content contract is pinned by the shared pch_cmake_consumer tests;
    # here only that the block reaches the generated CMakeLists
    text = _generate_cmake(tmp_path)
    assert "target_compile_options(app PRIVATE" in text
    assert 'OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/esphome_pch.h"' in text


def test_cmake_lists_pch_block_disabled(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    assert "esphome_pch.h" not in _generate_cmake(tmp_path)


class TestRunCompilePhases:
    """The pch pre-build block in run_compile: header write, conditional
    cmake-only phase, and the never-abort-the-build exception contract."""

    @pytest.fixture
    def compile_ctx(self, tmp_path: Path):
        CORE.config_path = tmp_path / "test.yaml"
        CORE.build_path = tmp_path / "build"
        CORE.name = "livingroom"
        CORE.toolchain = Toolchain.SDK_NRF
        with (
            patch.object(nrf52, "check_and_install"),
            patch.object(nrf52, "_generate_cmake_lists", return_value=False),
            patch.object(
                nrf52,
                "get_build_paths",
                return_value={
                    "python_executable": "python3",
                    "framework_path": tmp_path,
                },
            ),
            patch.object(nrf52, "get_build_env", return_value={}),
            patch.object(nrf52, "zephyr_data", return_value={KEY_BOARD: "board"}),
            patch.object(nrf52, "run_command_ok") as run_cmd,
            patch.object(nrf52, "_prepare_pch") as prepare,
        ):
            yield run_cmd, prepare, CORE.relative_pioenvs_path(CORE.name)

    def _run(self) -> None:
        nrf52.run_compile(None, {})

    def test_missing_db_runs_cmake_phase(self, compile_ctx) -> None:
        run_cmd, prepare, build_dir = compile_ctx
        # cmake-only ok, generated headers ok, final build fails
        results = iter([True, True, False])

        def west(cmd, **kwargs):
            # Phase 1 configures the sysbuild app domain
            app = build_dir / "zephyr"
            app.mkdir(parents=True, exist_ok=True)
            (app / "CMakeCache.txt").write_text("")
            return next(results)

        run_cmd.side_effect = west
        with pytest.raises(EsphomeError, match="nRF52 native build failed"):
            self._run()
        assert "--cmake-only" in run_cmd.call_args_list[0].args[0]
        # Generated syscall headers are built in the app domain pre-pch
        headers_cmd = run_cmd.call_args_list[1].args[0]
        assert headers_cmd[:2] == ["cmake", "--build"]
        assert str(build_dir / "zephyr") in headers_cmd
        assert "zephyr_generated_headers" in headers_cmd
        assert "--cmake-only" not in run_cmd.call_args_list[2].args[0]
        # The pch is prepared in the app domain dir, not the sysbuild root
        assert prepare.call_args.args[0] == build_dir / "zephyr"

    def test_generated_headers_failure_degrades(
        self, compile_ctx, caplog: pytest.LogCaptureFixture
    ) -> None:
        run_cmd, prepare, _ = compile_ctx
        # headers target fails, the real build still runs (and fails here)
        run_cmd.side_effect = [True, False, False]
        with pytest.raises(EsphomeError, match="nRF52 native build failed"):
            self._run()
        assert "Zephyr header generation failed" in caplog.text
        assert prepare.called

    def test_generated_headers_failure_strict_raises(
        self, monkeypatch: pytest.MonkeyPatch, compile_ctx
    ) -> None:
        monkeypatch.setenv("ESPHOME_PCH_STRICT", "1")
        run_cmd, prepare, _ = compile_ctx
        run_cmd.side_effect = [True, False]
        with pytest.raises(EsphomeError, match="ESPHOME_PCH_STRICT"):
            self._run()
        assert not prepare.called

    def test_cmake_phase_failure_raises(self, compile_ctx) -> None:
        run_cmd, prepare, _ = compile_ctx
        run_cmd.side_effect = [False]
        with pytest.raises(EsphomeError, match="configure failed"):
            self._run()
        assert not prepare.called

    def test_ccache_pch_env_reaches_west(self, compile_ctx) -> None:
        run_cmd, _, _ = compile_ctx
        run_cmd.side_effect = [False]
        # clear=True also drops ambient CCACHE_*/ESPHOME_PCH_* overrides
        with (
            patch.dict("os.environ", {}, clear=True),
            pytest.raises(EsphomeError, match="configure failed"),
        ):
            self._run()
        env = run_cmd.call_args.kwargs["env"]
        assert env["CCACHE_PCH_EXTSUM"] == "true"
        assert env["CCACHE_SLOPPINESS"] == "pch_defines,time_macros"

    @pytest.mark.parametrize("sysbuild", [False, True])
    def test_settled_db_skips_cmake_phase(self, sysbuild: bool, compile_ctx) -> None:
        run_cmd, prepare, build_dir = compile_ctx
        app = build_dir / "zephyr" if sysbuild else build_dir
        app.mkdir(parents=True)
        # A present top-level cache keeps the pristine wipe from dropping
        # the DB; the app-dir cache is the sysbuild layout marker
        (build_dir / "CMakeCache.txt").write_text("")
        (app / "CMakeCache.txt").write_text("")
        (app / "compile_commands.json").write_text("[]")
        run_cmd.side_effect = [False]
        with pytest.raises(EsphomeError, match="nRF52 native build failed"):
            self._run()
        assert run_cmd.call_count == 1
        assert "--cmake-only" not in run_cmd.call_args.args[0]
        assert prepare.call_args.args[0] == app

    def test_disabled_skips_header_and_cmake_phase(
        self, monkeypatch: pytest.MonkeyPatch, compile_ctx
    ) -> None:
        monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
        run_cmd, prepare, build_dir = compile_ctx
        run_cmd.side_effect = [False]
        with pytest.raises(EsphomeError, match="nRF52 native build failed"):
            self._run()
        assert run_cmd.call_count == 1
        assert not (build_dir / "esphome_pch.h").exists()
        # The wrapper still runs: it discards stale sidecars and feeds strict
        assert prepare.called

    def test_prepare_failure_never_aborts_the_build(
        self, compile_ctx, caplog: pytest.LogCaptureFixture
    ) -> None:
        run_cmd, prepare, build_dir = compile_ctx
        build_dir.mkdir(parents=True)
        # Keep the pristine wipe from dropping the dir the fallback touches
        (build_dir / "CMakeCache.txt").write_text("")
        prepare.side_effect = RuntimeError("boom")
        run_cmd.side_effect = [True, True, False]
        with pytest.raises(EsphomeError, match="nRF52 native build failed"):
            self._run()
        assert run_cmd.call_count == 3
        assert "Precompiled header setup failed" in caplog.text
        # The fallback still satisfies OBJECT_DEPENDS
        assert (build_dir / "esphome_pch.h").is_file()

    def test_prepare_failure_strict_raises(
        self, monkeypatch: pytest.MonkeyPatch, compile_ctx
    ) -> None:
        monkeypatch.setenv("ESPHOME_PCH_STRICT", "1")
        run_cmd, prepare, _ = compile_ctx
        prepare.side_effect = RuntimeError("boom")
        run_cmd.side_effect = [True, True]
        with pytest.raises(RuntimeError, match="boom"):
            self._run()
