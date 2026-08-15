import logging
from pathlib import Path
import subprocess

import yaml

from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import (
    get_project_compile_flags,
    get_project_link_flags,
    run_command_ok,
)
from esphome.helpers import write_file_if_changed
from esphome.util import get_serial_number

from .const import ZEPHYR_VARIANT_NATIVE_SIM

_LOGGER = logging.getLogger(__name__)


def _find_domain_build_dir(build_dir: Path) -> Path:
    """Locate the default image domain's own build directory.

    run_west_build() always passes --sysbuild, which fans a single `west build`
    out into one build directory per image domain (mcuboot, app, ...), each with
    its own `zephyr/` subdir (runners.yaml, .config, edt.pickle, ...), plus a
    top-level `domains.yaml` naming which domain is the default. Every domain
    targets the same physical board, so the default domain's build dir is a
    faithful stand-in for the whole build.
    """
    domains_yaml_path = Path(build_dir) / "domains.yaml"
    try:
        with domains_yaml_path.open(encoding="utf-8") as f:
            domains_yaml = yaml.safe_load(f)
        default_domain = domains_yaml["default"]
        return Path(
            next(
                d["build_dir"]
                for d in domains_yaml["domains"]
                if d["name"] == default_domain
            )
        )
    except (OSError, yaml.YAMLError, KeyError, TypeError, StopIteration):
        # Not a sysbuild (multi-domain) build -- everything lives directly
        # under build_dir.
        return Path(build_dir)


def _find_runners_yaml(build_dir: Path) -> Path:
    """Locate the runners.yaml describing the board's default flash runner."""
    return _find_domain_build_dir(build_dir) / "zephyr" / "runners.yaml"


def _runner_supports_dev_id(
    python_executable: Path, framework_path: Path, runner: str
) -> bool:
    """Ask the pinned SDK's own west runner framework whether `runner` declares
    the `dev_id` capability (a `-i/--dev-id` option), instead of checking against
    a hand-maintained list -- a future SDK version adding/removing dev_id support
    on a runner is picked up automatically, with no list to keep in sync.

    Runs inside the west venv (python_executable) so this sees runners the exact
    same way `west flash` itself does -- importing `runners` standalone outside
    west's own venv can fail for modules with west-specific dependencies.
    Returns False (matching the pre-dynamic-lookup default) if the runner is
    unrecognized or the query fails for any reason -- callers should treat that
    as "don't forward -i", reproducing today's single-probe auto-detect behavior.
    """
    runners_dir = framework_path / "zephyr" / "scripts" / "west_commands"
    script = (
        "import sys\n"
        f"sys.path.insert(0, {str(runners_dir)!r})\n"
        "try:\n"
        "    import runners\n"
        f"    print(runners.get_runner_cls({runner!r}).capabilities().dev_id)\n"
        "except Exception:\n"
        "    print(False)\n"
    )
    try:
        result = subprocess.run(
            [str(python_executable), "-c", script],
            capture_output=True,
            timeout=10,
            check=False,
            text=True,
        )
    except (OSError, subprocess.TimeoutExpired):
        return False
    return result.stdout.strip() == "True"


def get_flash_runner(build_dir: Path) -> str | None:
    """Return the board's default west flash runner name (e.g. "dfu-util",
    "pyocd", "jlink"), read from runners.yaml (written by the CMake configure
    step, see _find_runners_yaml() for the sysbuild wrinkle). None if it can't
    be determined (build not yet configured, unreadable file, etc.).
    """
    runners_yaml_path = _find_runners_yaml(build_dir)
    try:
        with runners_yaml_path.open(encoding="utf-8") as f:
            runners_yaml = yaml.safe_load(f)
    except (OSError, yaml.YAMLError) as e:
        _LOGGER.debug("Could not read %s: %s", runners_yaml_path, e)
        return None
    return (runners_yaml or {}).get("flash-runner")


def count_dfu_devices_for(dfu_util_path: str, vid_pid: str, alt: str) -> int | None:
    """Count how many attached USB devices match a dfu-util VID:PID and
    alt-setting, by parsing `dfu-util -l` output.

    West's own dfu-util runner (scripts/west_commands/runners/dfu.py) only
    checks whether at least one matching device is present before flashing --
    it never counts matches, so two boards sharing the same VID:PID (e.g. two
    Arduino Nano R4s, or any other board using the same bootloader) both
    sitting in DFU mode at once is silently ambiguous: which one gets flashed
    is up to dfu-util's own handling of multiple matches, not something a
    runner controls.

    Returns None if dfu-util can't be run -- callers should treat that as
    "unknown, proceed" rather than block flashing on an unrelated failure.
    0 means no matching device is currently visible (e.g. not reset into DFU
    mode yet).
    """
    import re

    try:
        result = subprocess.run(
            [dfu_util_path, "-l", "-d", vid_pid],
            capture_output=True,
            timeout=10,
            check=False,
            text=True,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    # Mirrors dfu.py's own list_pattern: numeric alt matches by index, non-numeric
    # by name -- same distinction west's own runner makes when finding a device.
    if alt.isdigit():
        pattern = rf", alt={re.escape(alt)},"
    else:
        pattern = rf', name="{re.escape(alt)}",'
    return len(re.findall(pattern, result.stdout))


def count_dfu_devices(build_dir: Path) -> int | None:
    """count_dfu_devices_for(), sourcing the VID:PID/alt from this build's own
    runners.yaml (written by the CMake configure step) instead of taking them
    directly -- see count_dfu_devices_for() for what's actually being counted
    and why.

    Returns None if dfu-util isn't installed, runners.yaml can't be read, or
    the dfu-util args can't be parsed.
    """
    import shutil

    dfu_util = shutil.which("dfu-util")
    if dfu_util is None:
        return None

    runners_yaml_path = _find_runners_yaml(build_dir)
    try:
        with runners_yaml_path.open(encoding="utf-8") as f:
            runners_yaml = yaml.safe_load(f) or {}
    except (OSError, yaml.YAMLError) as e:
        _LOGGER.debug("Could not read %s: %s", runners_yaml_path, e)
        return None

    dfu_args = (runners_yaml.get("args") or {}).get("dfu-util") or []
    vid_pid = None
    alt = "0"
    for arg in dfu_args:
        if arg.startswith("--pid="):
            vid_pid = arg.removeprefix("--pid=")
        elif arg.startswith("--alt="):
            alt = arg.removeprefix("--alt=")
    if vid_pid is None:
        return None

    return count_dfu_devices_for(dfu_util, vid_pid, alt)


def run_arduino_dfu_flash(build_dir: Path, app_pid: str, dfu_pid: str) -> bool:
    """Flash a board using Arduino's own patched dfu-util fork instead of
    west's generic dfu-util runner.

    Arduino's fork (bundled with the Arduino IDE/arduino-cli Renesas core,
    reports itself as e.g. "dfu-util 0.11-arduinoN") adds a `-Q` quirks flag
    (confirmed via `strings`: quirks.c/get_quirks) that stock dfu-util has no
    equivalent for -- testing showed the board never leaves DFU mode with the
    generic west runner + stock dfu-util + address-padded image, but does
    with this exact tool and invocation shape (dual-VID:PID auto-detach, raw
    unpadded image, no manual DFU-mode entry required). Not something to
    silently fall back from: a plain-dfu-util attempt already reproducibly
    leaves the board looking bricked, so this requires the real tool rather
    than degrading to the known-broken path.
    """
    import shutil

    dfu_util = shutil.which("dfu-util")
    found = "no dfu-util found on PATH"
    version_ok = False
    if dfu_util is not None:
        try:
            result = subprocess.run(
                [dfu_util, "--version"],
                capture_output=True,
                timeout=10,
                check=False,
                text=True,
            )
            version_ok = "-arduino" in result.stdout
            stdout = result.stdout.strip()
            version_line = stdout.splitlines()[0] if stdout else ""
            found = f"found {version_line!r}"
        except (OSError, subprocess.TimeoutExpired):
            found = f"could not run {dfu_util!r}"
    if dfu_util is None or not version_ok:
        raise EsphomeError(
            "This board needs Arduino's patched dfu-util (adds device-specific "
            f"quirk handling stock dfu-util lacks) -- {found}. Install the "
            "Arduino Renesas core via the Arduino IDE or arduino-cli, which "
            "bundles a compatible build, and put its dfu-util on PATH instead."
        )

    device_count = count_dfu_devices_for(dfu_util, dfu_pid, "0")
    if device_count is not None and device_count > 1:
        raise EsphomeError(
            f"Found {device_count} devices in DFU mode matching this board's "
            "bootloader. Disconnect all but the target device before "
            "uploading, or dfu-util may flash the wrong one."
        )

    domain_build_dir = _find_domain_build_dir(build_dir)
    bin_path = domain_build_dir / "zephyr" / "zephyr.bin"
    # No -R/--reset: this fork's -R unexpectedly requires an argument (differs from
    # stock dfu-util's boolean -R), breaking the invocation -- matches the exact
    # command confirmed working by hand, manual reset still needed after flashing.
    return run_command_ok(
        [
            dfu_util,
            "--device",
            f"{app_pid},{dfu_pid}",
            "-D",
            str(bin_path),
            "-a0",
            "-Q",
        ],
        stream_output=True,
    )


def _flash_load_address(
    python_executable: Path, framework_path: Path, domain_build_dir: Path
) -> int | None:
    """Ask the pinned SDK's own west runner framework for the real flash
    address this build's image is linked to run from (normally the
    `zephyr,code-partition` devicetree node's address), the same way
    `--dfuse`/pyocd/jlink derive it via
    ZephyrBinaryRunner.flash_address_from_build_conf(). None if it can't be
    determined (build not configured, query fails, etc.).

    Needed for boards whose default flash runner writes a flat image starting
    at a fixed interface base address with no address-tagging support of its
    own (see pad_image_to_flash_address()) -- everywhere else, the runner
    resolves this internally and no separate lookup is needed.
    """
    runners_dir = framework_path / "zephyr" / "scripts" / "west_commands"
    script = (
        "import sys\n"
        f"sys.path.insert(0, {str(runners_dir)!r})\n"
        "try:\n"
        "    from runners.core import BuildConfiguration, ZephyrBinaryRunner\n"
        f"    build_conf = BuildConfiguration({str(domain_build_dir)!r})\n"
        "    print(ZephyrBinaryRunner.flash_address_from_build_conf(build_conf))\n"
        "except Exception:\n"
        "    print('')\n"
    )
    try:
        result = subprocess.run(
            [str(python_executable), "-c", script],
            capture_output=True,
            timeout=10,
            check=False,
            text=True,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    output = result.stdout.strip()
    return int(output) if output else None


def pad_image_to_flash_address(
    python_executable: Path, framework_path: Path, build_dir: Path
) -> Path | None:
    """Prepend 0xFF padding to the board's built .bin so that flashing it with
    a flat, address-oblivious tool (starting at device offset 0) lands the
    real image at its actual linked flash address instead of at 0.

    Only needed for a flash runner that can't be told the target address
    itself (see board.cmake comment on the Nano R4's dfu-util args for why
    --dfuse isn't usable there). Returns the padded file's path, or None if
    the real .bin/flash address can't be found -- callers should fall back to
    flashing the original image unpadded (index-0, matching pre-fix behavior)
    rather than fail outright.
    """
    domain_build_dir = _find_domain_build_dir(build_dir)
    runners_yaml_path = domain_build_dir / "zephyr" / "runners.yaml"
    try:
        with runners_yaml_path.open(encoding="utf-8") as f:
            runners_yaml = yaml.safe_load(f)
        bin_file = runners_yaml["config"]["bin_file"]
    except (OSError, yaml.YAMLError, KeyError, TypeError) as e:
        _LOGGER.debug("Could not read %s: %s", runners_yaml_path, e)
        return None

    bin_path = domain_build_dir / "zephyr" / bin_file
    address = _flash_load_address(python_executable, framework_path, domain_build_dir)
    if address is None or not bin_path.is_file():
        return None

    try:
        image = bin_path.read_bytes()
    except OSError as e:
        _LOGGER.debug("Could not read %s: %s", bin_path, e)
        return None

    padded_path = domain_build_dir / "zephyr" / "zephyr.dfu-util-padded.bin"
    padded_path.write_bytes(b"\xff" * address + image)
    return padded_path


def resolve_dev_id(
    python_executable: Path, framework_path: Path, build_dir: Path, port: str
) -> str | None:
    """Resolve a `-i/--dev-id` value to disambiguate which probe `west flash` uses.

    Reads the board's default flash runner (see get_flash_runner()) and, only
    when that runner is known to support device IDs (see
    _runner_supports_dev_id()), looks up the USB serial number backing the
    selected serial `port`. Returns None whenever the runner is unknown/unsupported
    or the serial number can't be determined -- callers should treat None as
    "don't pass -i", which reproduces today's single-probe auto-detect behavior
    exactly.
    """
    flash_runner = get_flash_runner(build_dir)
    if flash_runner is None or not _runner_supports_dev_id(
        python_executable, framework_path, flash_runner
    ):
        return None

    return get_serial_number(port)


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
    snippets: list[str] | None = None,
    shield_root: Path | None = None,
    shields: list[str] | None = None,
    snippet_root: Path | None = None,
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
    if shield_root is not None:
        west_cmd.append(f"--cmake-opt=-DSHIELD_ROOT={shield_root}")
    if shields:
        west_cmd.append(f"--cmake-opt=-DSHIELD={';'.join(shields)}")
    if snippet_root is not None:
        west_cmd.append(f"--cmake-opt=-DSNIPPET_ROOT={snippet_root}")
    for snippet in snippets or []:
        west_cmd += ["-S", snippet]

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
        # '--skip-rebuild' still works in west v1.5, but is deprecated.
        "--no-rebuild",
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


def run_west_flash_generic(
    python_executable: Path,
    framework_path: Path,
    env: dict,
    build_dir: Path,
    dev_id: str | None = None,
    img: Path | None = None,
) -> bool:
    """Flash a Zephyr target using the board's default west runner.

    This is used by non-ESP32 Zephyr variants where flashing is typically
    handled by probe-based runners configured by Zephyr (jlink, pyocd, etc.).

    `dev_id`, when given (see `resolve_dev_id`), is forwarded as `-i/--dev-id`
    to tell the runner which attached probe to use -- without it, the runner
    picks whichever probe it finds, which is ambiguous when more than one
    debug-probe board is attached at once.

    `img`, when given, is forwarded as `--img` (after `--`, so it reaches the
    runner's own argparse rather than west's) to flash a specific file instead
    of the runner's default build output -- see pad_image_to_flash_address().
    """
    west_cmd = [
        str(python_executable),
        "-m",
        "west",
        "flash",
        # '--skip-rebuild' still works in west v1.5, but is deprecated.
        "--no-rebuild",
        "-d",
        str(build_dir),
    ]
    if dev_id:
        west_cmd += ["-i", dev_id]
    if img:
        west_cmd += ["--", "--img", str(img)]
    return run_command_ok(
        west_cmd,
        env=env,
        stream_output=True,
        cwd=str(framework_path),
    )


def run_west_flash_pyocd(
    python_executable: Path,
    framework_path: Path,
    env: dict,
    build_dir: Path,
    dev_id: str | None = None,
) -> bool:
    """Flash a real Zephyr embedded target via a J-Link/CMSIS-DAP debug probe.

    Delegates to Zephyr's own pyocd runner -- same rationale as run_west_flash
    above, just a different runner for variants with no serial/esptool path
    (e.g. nrf52, which flashes over SWD instead).

    `dev_id`, when given, is forwarded as `-i/--dev-id` -- see
    `run_west_flash_generic` above.
    """
    west_cmd = [
        str(python_executable),
        "-m",
        "west",
        "flash",
        # '--skip-rebuild' still works in west v1.5, but is deprecated.
        "--no-rebuild",
        "-d",
        str(build_dir),
        "--runner",
        "pyocd",
    ]
    if dev_id:
        west_cmd += ["-i", dev_id]
    return run_command_ok(
        west_cmd,
        env=env,
        stream_output=True,
        cwd=str(framework_path),
    )
