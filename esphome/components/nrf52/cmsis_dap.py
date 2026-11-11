"""Flashing with pyOCD through a CMSIS-DAP probe, such as the one built into Seeed XIAO nRF54 boards."""

from pathlib import Path

from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import run_command_ok

from .const import (
    VARIANT_NRF54L15,
    VARIANT_NRF54LM20,
    VARIANT_NRF52832,
    VARIANT_NRF52840,
)
from .framework import (
    check_and_install,
    get_build_env,
    get_build_paths,
    get_nrf52_variant,
)

# RRAM controller base of the nRF54L chips, whose RRAM is written directly instead of with
# pyOCD's flash algorithm, which faults on the nRF54LM20A
_RRAMC_BASES = {
    VARIANT_NRF54L15: 0x5004B000,
    VARIANT_NRF54LM20: 0x5004E000,
}
_RRAM_LOADER = Path(__file__).with_name("pyocd_rram_load.py")

# pyOCD's built-in target names
_PYOCD_TARGETS = {
    VARIANT_NRF52832: "nrf52832",
    VARIANT_NRF52840: "nrf52840",
    VARIANT_NRF54L15: "nrf54l",
    VARIANT_NRF54LM20: "nrf54lm20a",
}


def cmsis_dap_unique_id(port: str) -> str | None:
    """The unique ID of the CMSIS-DAP probe that provides ``port``, or None for other ports."""
    import serial.tools.list_ports as _list_ports

    for info in _list_ports.comports(include_links=True):
        if info.device != port:
            continue
        if any("CMSIS-DAP" in (text or "") for text in (info.product, info.interface)):
            return info.serial_number or ""
        return None
    return None


def _images(build_dir: Path) -> list[Path]:
    """The hex files that make up the firmware: MCUboot and the signed app, or the app alone."""
    if (merged := build_dir / "merged.hex").is_file():  # SDK before 3.4.0
        return [merged]
    images = []
    if (mcuboot := build_dir / "mcuboot" / "zephyr" / "zephyr.hex").is_file():
        images.append(mcuboot)
    app_dir = build_dir / "zephyr" / "zephyr"
    for name in ("zephyr.signed.hex", "zephyr.hex"):
        if (app := app_dir / name).is_file():
            images.append(app)
            break
    else:
        raise EsphomeError("Firmware not found. Please compile first.")
    return images


def upload_using_cmsis_dap(unique_id: str | None = None) -> None:
    """Flash with pyOCD; unique_id picks the probe when more than one is connected."""
    variant = get_nrf52_variant()
    if (target := _PYOCD_TARGETS.get(variant)) is None:
        raise EsphomeError(f"Flashing {variant} with pyOCD is not supported")
    images = _images(CORE.relative_pioenvs_path(CORE.name))
    check_and_install()  # the Zephyr requirements install pyOCD
    paths = get_build_paths()
    python = str(paths["python_executable"])
    if (rramc := _RRAMC_BASES.get(variant)) is not None:
        cmd = [python, "-I", str(_RRAM_LOADER), target, hex(rramc), unique_id or ""]
    else:
        cmd = [python, "-m", "pyocd", "flash", "-t", target, "-e", "sector"]
        if unique_id:
            cmd += ["-u", unique_id]
    cmd += [str(image) for image in images]
    if not run_command_ok(cmd, env=get_build_env(None), stream_output=True):
        raise EsphomeError(
            "pyOCD could not flash the device; its output above says why. If it says "
            "the chip is locked or write-protected, erase it once with "
            f"'pyocd erase -t {target} --chip' and upload again."
        )
