"""nrf52 sdk-nrf pch wiring: the CMake consumer block, the prepare step,
and the two-phase west split in run_compile."""

from collections.abc import Generator
from pathlib import Path
from unittest.mock import Mock, patch

import pytest

from esphome.components import nrf52
from esphome.components.zephyr.const import KEY_BOARD
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION, Toolchain
from esphome.core import CORE, EsphomeError


@pytest.fixture
def build_dir(tmp_path: Path) -> Path:
    d = tmp_path / "build" / ".pioenvs" / "livingroom"
    d.mkdir(parents=True)
    return d


def _prepare(build_dir: Path, generate_headers: bool = False) -> tuple[Mock, Mock]:
    with (
        patch.dict(CORE.data, {KEY_CORE: {KEY_FRAMEWORK_VERSION: "2.9.2"}}),
        patch.object(
            nrf52, "zephyr_data", return_value={KEY_BOARD: "adafruit_feather"}
        ),
        patch.object(nrf52, "get_project_compile_flags", return_value=["-Os"]),
        patch.object(nrf52, "run_command_ok", return_value=True) as run_cmd,
        patch.object(nrf52.pch, "prepare_pch") as prepare,
    ):
        nrf52._prepare_pch(build_dir, generate_headers, {"A": "b"}, "/sdk")
    return run_cmd, prepare


@pytest.mark.parametrize("layout", ["zephyr/autoconf.h", "autoconf.h"])
def test_prepare_pch_passes_the_build_identity(build_dir: Path, layout: str) -> None:
    """Zephyr 3.4 moved autoconf.h under zephyr/; both layouts are found."""
    autoconf = build_dir / "zephyr" / "include" / "generated" / layout
    autoconf.parent.mkdir(parents=True)
    autoconf.write_text("#define CONFIG_GPIO 1\n")

    run_cmd, prepare = _prepare(build_dir)

    assert not run_cmd.called
    passed_dir, identity_file, extras = prepare.call_args.args
    assert passed_dir == build_dir
    assert identity_file == autoconf
    assert list(extras) == ["2.9.2", "adafruit_feather", "-Os"]


def test_prepare_pch_generates_the_zephyr_headers(build_dir: Path) -> None:
    """kernel.h needs the syscall headers, which only a build step makes."""
    run_cmd, prepare = _prepare(build_dir, generate_headers=True)

    assert run_cmd.call_args.args[0] == [
        "cmake",
        "--build",
        str(build_dir),
        "--target",
        "zephyr_generated_headers",
    ]
    assert run_cmd.call_args.kwargs["env"] == {"A": "b"}
    assert prepare.called


def test_prepare_pch_header_generation_failure_stops_the_build(
    build_dir: Path,
) -> None:
    with (
        patch.object(nrf52, "run_command_ok", return_value=False),
        patch.object(nrf52.pch, "prepare_pch") as prepare,
        pytest.raises(EsphomeError, match="header generation failed"),
    ):
        nrf52._prepare_pch(build_dir, True, {}, "/sdk")
    assert not prepare.called


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


def test_west_configure_command_keeps_one_separator() -> None:
    """The options of west end at the "--"; a second one reaches CMake as an
    unknown argument."""
    cmd = nrf52._west_build_command(
        Path("/penv/python"), "board", Path("/b"), Path("/s"), cmake_only=True
    )
    assert cmd.count("--") == 1
    split = cmd.index("--")
    assert "--cmake-only" in cmd[:split]
    assert cmd[split + 1 :] == [
        "-DCMAKE_BUILD_TYPE=MinSizeRel",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]


def test_west_build_after_a_configure_passes_no_cmake_arguments() -> None:
    """West configures again whenever it is handed CMake arguments."""
    cmd = nrf52._west_build_command(
        Path("/penv/python"), "board", Path("/b"), Path("/s"), configured=True
    )
    assert "--" not in cmd
    assert not [arg for arg in cmd if arg.startswith("-D")]


CompileCtx = tuple[Mock, Mock, Path]


class TestRunCompilePhases:
    """The pch block in run_compile: the configure-only phase when there is
    no compile database yet, then the prepare step, then the build."""

    @pytest.fixture
    def compile_ctx(self, tmp_path: Path) -> Generator[CompileCtx, None, None]:
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

    def test_missing_db_runs_cmake_phase(self, compile_ctx: CompileCtx) -> None:
        run_cmd, prepare, build_dir = compile_ctx
        # configure ok, final build fails
        results = iter([True, False])

        def west(cmd, **kwargs):
            # The configure phase creates the sysbuild app domain
            app = build_dir / "zephyr"
            app.mkdir(parents=True, exist_ok=True)
            (app / "CMakeCache.txt").write_text("")
            return next(results)

        run_cmd.side_effect = west
        with pytest.raises(EsphomeError, match="nRF52 native build failed"):
            self._run()
        assert "--cmake-only" in run_cmd.call_args_list[0].args[0]
        # One configure: the build does not hand west the arguments again
        assert "--" not in run_cmd.call_args_list[1].args[0]
        # Prepared in the app domain dir, with the headers still to generate
        assert prepare.call_args.args[:2] == (build_dir / "zephyr", True)

    def test_cmake_phase_failure_raises(self, compile_ctx: CompileCtx) -> None:
        run_cmd, prepare, _ = compile_ctx
        run_cmd.side_effect = [False]
        with pytest.raises(EsphomeError, match="configure failed"):
            self._run()
        assert not prepare.called

    def test_ccache_pch_env_reaches_west(self, compile_ctx: CompileCtx) -> None:
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
    def test_settled_db_skips_cmake_phase(
        self, sysbuild: bool, compile_ctx: CompileCtx
    ) -> None:
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
        assert "-DCMAKE_BUILD_TYPE=MinSizeRel" in run_cmd.call_args.args[0]
        assert prepare.call_args.args[:2] == (app, False)

    def test_disabled_skips_the_pch(
        self, monkeypatch: pytest.MonkeyPatch, compile_ctx: CompileCtx
    ) -> None:
        monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
        run_cmd, prepare, _ = compile_ctx
        run_cmd.side_effect = [False]
        with pytest.raises(EsphomeError, match="nRF52 native build failed"):
            self._run()
        assert run_cmd.call_count == 1
        assert not prepare.called

    def test_prepare_failure_stops_the_build(self, compile_ctx: CompileCtx) -> None:
        run_cmd, prepare, build_dir = compile_ctx
        build_dir.mkdir(parents=True)
        (build_dir / "CMakeCache.txt").write_text("")
        (build_dir / "compile_commands.json").write_text("[]")
        prepare.side_effect = EsphomeError("boom")
        with pytest.raises(EsphomeError, match="boom"):
            self._run()
        assert not run_cmd.called
