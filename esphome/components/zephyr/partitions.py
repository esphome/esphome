"""Flash layout requirements, checked by sysbuild against the merged devicetree.

The checks themselves (partition_checks.cmake) run after every image is configured, so
they see board, shield, snippet and overlay changes alike, and MCUboot's image as well
as the app's. This module only records what a build needs and renders the sysbuild
CMakeLists.txt that runs them.
"""

from dataclasses import dataclass
from pathlib import Path

PARTITION_CHECKS_CMAKE = Path(__file__).parent / "partition_checks.cmake"


@dataclass(frozen=True)
class BootLayout:
    """Where the chip, or a resident vendor bootloader on the board, starts the first
    image (MCUboot when it is built, else the app), and the flash it keeps for itself.
    Offsets are within the flash device holding the first image."""

    start: int
    reserved: tuple[tuple[int, int], ...] = ()


def zephyr_require_partition(label: str, reason: str) -> None:
    """Fail the build unless the app's devicetree has a partition labeled `label`;
    `reason` names what needs it in the error."""
    from . import zephyr_data  # noqa: PLC0415
    from .const import KEY_PARTITION_REQUIREMENTS  # noqa: PLC0415

    requirements = zephyr_data()[KEY_PARTITION_REQUIREMENTS]
    if (label, reason) not in requirements:
        requirements.append((label, reason))


def zephyr_set_boot_layout(layout: BootLayout | None) -> None:
    """Replace the variant's boot layout, for a bootloader choice that moves it."""
    from . import zephyr_data  # noqa: PLC0415
    from .const import KEY_BOOT_LAYOUT  # noqa: PLC0415

    zephyr_data()[KEY_BOOT_LAYOUT] = layout


def _cmake_string(value: str) -> str:
    # ';' would split the argument into a CMake list.
    escaped = value.replace("\\", "\\\\").replace('"', '\\"').replace("$", "\\$")
    return f'"{escaped.replace(";", ",")}"'


def render_check_call(boot: BootLayout | None, required: list[tuple[str, str]]) -> str:
    """The esphome_check_partitions() call for this build."""
    lines = ["esphome_check_partitions("]
    if boot is not None:
        lines.append(f"  BOOT_START {boot.start:#x}")
        if boot.reserved:
            ranges = " ".join(f"{start:#x} {end:#x}" for start, end in boot.reserved)
            lines.append(f"  RESERVED {ranges}")
    if required:
        lines.append("  REQUIRE")
        lines += [
            f"    {_cmake_string(label)} {_cmake_string(reason)}"
            for label, reason in required
        ]
    lines.append(")")
    return "\n".join(lines)


def render_sysbuild_cmake(
    boot: BootLayout | None, required: list[tuple[str, str]]
) -> str:
    """Zephyr's sysbuild template followed by the flash layout checks."""
    return "\n".join(
        [
            "find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})",
            "",
            "project(sysbuild LANGUAGES)",
            "",
            PARTITION_CHECKS_CMAKE.read_text(encoding="utf-8"),
            render_check_call(boot, required),
            "",
        ]
    )
