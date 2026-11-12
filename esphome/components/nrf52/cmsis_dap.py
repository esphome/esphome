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

# pyOCD's built-in target names
_PYOCD_TARGETS = {
    VARIANT_NRF52832: "nrf52832",
    VARIANT_NRF52840: "nrf52840",
    VARIANT_NRF54L15: "nrf54l",
    VARIANT_NRF54LM20: "nrf54lm20a",
}


# Watchdog of the nRF54L chips that Zephyr uses, and its registers for stopping it
_NRF54L_WDT31 = 0x50109000
_WDT_TASKS_STOP = 0x004
_WDT_TSEN = 0x520
_WDT_TSEN_KEY = 0x6E524635


def _stop_watchdog_commands(variant: str) -> list[str]:
    """PyOCD commands that stop a running watchdog, which keeps counting while flashing halts
    the CPU in firmware that does not pause it then, and would reset the chip part way."""
    if variant not in (VARIANT_NRF54L15, VARIANT_NRF54LM20):
        return []
    tsen = _NRF54L_WDT31 + _WDT_TSEN
    return [
        f"write32 0x{tsen:08x} 0x{_WDT_TSEN_KEY:08x}",
        f"write32 0x{_NRF54L_WDT31 + _WDT_TASKS_STOP:08x} 1",
        f"write32 0x{tsen:08x} 0",
    ]


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
    check_and_install()  # the Zephyr requirements install pyOCD, and it is patched
    paths = get_build_paths()
    env = get_build_env(None)
    pyocd = [str(paths["python_executable"]), "-m", "pyocd"]
    probe = ["-t", target] + (["-u", unique_id] if unique_id else [])
    if commands := _stop_watchdog_commands(variant):
        # Best effort: a locked chip refuses it, and 'pyocd flash' then unlocks the chip
        cmd = [*pyocd, "cmd", *probe]
        for command in commands:
            cmd += ["-c", command]
        run_command_ok(cmd, env=env, stream_output=True)
    cmd = [*pyocd, "flash", *probe, "-e", "sector", *(str(image) for image in images)]
    if not run_command_ok(cmd, env=env, stream_output=True):
        raise EsphomeError(
            "pyOCD could not flash the device; its output above says why. If it says "
            "the chip is locked or reports a memory transfer fault, unlock and erase it "
            f"once with 'pyocd erase -t {target} --mass' and upload again. Not with "
            "'--chip': it leaves an nRF54L locked, as no firmware opens it at reset."
        )
