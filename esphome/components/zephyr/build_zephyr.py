import logging
from pathlib import Path

from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import (
    get_project_compile_flags,
    get_project_link_flags,
    run_command_ok,
)
from esphome.helpers import write_file_if_changed

from .const import (
    ZEPHYR_VARIANT_ESP32,
    ZEPHYR_VARIANT_ESP32_C6,
    ZEPHYR_VARIANT_ESP32_H2,
    ZEPHYR_VARIANT_NATIVE_SIM,
)

_LOGGER = logging.getLogger(__name__)

# Maps variant → Zephyr SDK toolchain name (arg to setup.sh -t / ZEPHYR_TOOLCHAIN_VARIANT).
# Variants absent from this map install host tools only and use the host GCC.
_VARIANT_TOOLCHAIN: dict[str, str] = {
    ZEPHYR_VARIANT_ESP32_H2: "riscv64-zephyr-elf",
    ZEPHYR_VARIANT_ESP32_C6: "riscv64-zephyr-elf",
    # Original ESP32 is Xtensa (dual-core PRO_CPU/APP_CPU), unlike H2/C6's RISC-V --
    # a completely separate Zephyr SDK toolchain.
    ZEPHYR_VARIANT_ESP32: "xtensa-espressif_esp32_zephyr-elf",
}


_NATIVE_SIM_LIBSTDCXX_WORKAROUND = """\

# EXTERNAL_LIBC switches posix arch to hosted mode (no -nostdinc) and
# EXTERNAL_LIBCPP links host libstdc++.  No manual header injection needed.
target_link_libraries(app PRIVATE stdc++)
"""


def generate_cmake_lists(mode: str) -> bool:
    """Generate the Zephyr CMakeLists.txt for a native West build.

    Returns True if the file changed.
    """
    compile_flags = get_project_compile_flags()
    link_flags = get_project_link_flags()

    lines = [
        "cmake_minimum_required(VERSION 3.20.0)",
        "",
        'set(Zephyr_DIR "$ENV{ZEPHYR_BASE}/share/zephyr-package/cmake/")',
        "",
        "find_package(Zephyr REQUIRED)",
        "",
        f"project({CORE.name})",
        "",
        'file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/../src/*.c")',
        "",
        "target_sources(app PRIVATE ${APP_SOURCES})",
        'target_include_directories(app PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../src")',
    ]

    if compile_flags:
        lines += [
            "",
            "target_compile_options(app PRIVATE",
            *[f'  "{flag}"' for flag in compile_flags],
            ")",
        ]

    if link_flags:
        lines += [
            "",
            "zephyr_ld_options(",
            *[f'  "{flag}"' for flag in link_flags],
            ")",
        ]

    content = "\n".join(lines) + "\n"
    if mode == ZEPHYR_VARIANT_NATIVE_SIM:
        content += _NATIVE_SIM_LIBSTDCXX_WORKAROUND

    return write_file_if_changed(
        CORE.relative_build_path("zephyr", "CMakeLists.txt"),
        content,
    )


def run_west_blobs_fetch(
    python_executable: Path,
    framework_path: Path,
    env: dict,
    module: str,
    allow_regex: str,
    sentinel_name: str,
) -> None:
    """Fetch west blobs for a HAL module, gated on a per-SDK sentinel file.

    Uses --allow-regex to limit the download to the relevant chip's blobs
    and --auto-accept to skip interactive license prompts.
    The sentinel lives next to the framework's .ready file so the fetch
    only runs once per SDK install, not on every compile.
    """
    sentinel = framework_path / sentinel_name
    if sentinel.exists():
        return
    _LOGGER.info("Fetching %s binary blobs matching '%s' ...", module, allow_regex)
    cmd = [
        str(python_executable),
        "-m",
        "west",
        "blobs",
        "fetch",
        module,
        "--allow-regex",
        allow_regex,
        "--auto-accept",
    ]
    if not run_command_ok(cmd, env=env, cwd=str(framework_path)):
        raise EsphomeError(f"Failed to fetch {module} binary blobs")
    sentinel.touch()


def run_west_build(
    python_executable: Path,
    framework_path: Path,
    board: str,
    env: dict,
    sdk_install_dir: Path | None = None,
    zephyr_toolchain_variant: str = "host",
    extra_modules: list[Path] | None = None,
    board_root: Path | None = None,
) -> None:
    """Run west build for a Zephyr native build.

    When sdk_install_dir is provided, sets ZEPHYR_SDK_INSTALL_DIR and
    ZEPHYR_TOOLCHAIN_VARIANT and outputs to .west_build/.
    """
    if sdk_install_dir is not None:
        build_dir = CORE.relative_build_path(".west_build")
        run_env = {
            **env,
            "ZEPHYR_SDK_INSTALL_DIR": str(sdk_install_dir),
            "ZEPHYR_TOOLCHAIN_VARIANT": zephyr_toolchain_variant,
        }
    else:
        build_dir = CORE.relative_pioenvs_path(CORE.name)
        run_env = env
    source_dir = CORE.relative_build_path("zephyr")

    west_cmd = [
        str(python_executable),
        "-m",
        "west",
        "build",
        "--pristine=auto",
        # west build defaults to sysbuild off -- without this, sysbuild.conf is
        # silently ignored and no bootloader child image gets built.
        "--sysbuild",
        "-b",
        board,
        "-d",
        str(build_dir),
        str(source_dir),
    ]
    if extra_modules:
        # Matches nrf52's own _generate_cmake_lists() variable name; Zephyr's
        # zephyr_get() accepts both this and ZEPHYR_EXTRA_MODULES as aliases.
        modules_list = ";".join(str(p) for p in extra_modules)
        west_cmd.append(f"--cmake-opt=-DEXTRA_ZEPHYR_MODULES={modules_list}")
    if board_root is not None:
        west_cmd.append(f"--cmake-opt=-DBOARD_ROOT={board_root}")

    if not run_command_ok(
        west_cmd,
        env=run_env,
        stream_output=True,
        cwd=str(framework_path),
    ):
        raise EsphomeError("Zephyr native build failed")


def run_west_flash(
    python_executable: Path,
    framework_path: Path,
    env: dict,
    build_dir: Path,
    device: str,
    baud_rate: str | int | None = None,
) -> bool:
    """Flash a real Zephyr embedded target (e.g. esp32_h2) via `west flash`.

    Delegates bootloader/partition-table/app image selection and offsets to
    Zephyr's own runner (runners/esp32.py, wrapping esptool) and its sysbuild
    multi-domain flashing -- both already know every image from the CMake
    configure step, so there is no need to replicate that logic here.
    """
    west_cmd = [
        str(python_executable),
        "-m",
        "west",
        "flash",
        "--skip-rebuild",
        "-d",
        str(build_dir),
        "--esp-device",
        device,
    ]
    if baud_rate:
        west_cmd += ["--esp-baud-rate", str(baud_rate)]

    return run_command_ok(
        west_cmd,
        env=env,
        stream_output=True,
        cwd=str(framework_path),
    )
