"""RP2040/RP2350 RP2XXX_PINMUX pinctrl signal decoding, shared by I2C/UART
pinctrl overlay generation in zephyr/__init__.py."""

from collections.abc import Callable
import logging
from pathlib import Path
import subprocess

from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

# Devicetree property rpi_pico pinctrl groups pack all their signal macros into.
PROPERTY_NAME = "pinmux"

# I2C pinctrl group/state to assume when DTS resolution fails.
I2C_FALLBACK_GROUP = "group1"
I2C_FALLBACK_STATE_SUFFIXES = ("default",)

# RP2XXX_PINMUX(pin_num, alt_func) only encodes the pin and which peripheral owns
# it, not which signal within it -- role is implied by which fixed GPIO it's on.
_PIN_NUM_POS = 5
_PIN_NUM_MASK = 0x3F


def pin_macro(prefix: str, signal: str, pin: int | None, variant: str) -> str | None:
    """`variant` is unused -- accepted only for signature parity with the
    other families' pin_macro()."""
    if pin is None:
        return None
    return f"<{prefix}_{signal}_P{pin}>"


def i2c_value_role_decoder(bus_label: str) -> Callable[[int], str | None] | None:
    """Always None -- rp2040/rp2350's I2C pin validation is a fixed
    instance+role table check (zephyr_setup_i2c_pinctrl's rpi_pico branch),
    not a decodable pinmux value, so there's nothing to bind here."""
    return None


def _value_role(value: int, instance_pins: dict[str, frozenset[int]]) -> str | None:
    """Decode a value's embedded pin number against the per-instance valid-pins
    table (uart/i2c_valid_pins_by_instance) already used to validate it."""
    pin_num = (value >> _PIN_NUM_POS) & _PIN_NUM_MASK
    for role, pins in instance_pins.items():
        if pin_num in pins:
            return role
    return None


def value_role_decoder(
    instance_pins: dict[str, frozenset[int]] | None,
) -> Callable[[int], str | None] | None:
    if instance_pins is None:
        return None

    def decoder(value: int) -> str | None:
        return _value_role(value, instance_pins)

    return decoder


def uart_pinctrl(
    board: str, port_label: str
) -> tuple[None, Callable[[int], str | None]] | None:
    """Return (group_role_resolver, value_role_decoder) bound to port_label's
    real UART instance valid-pins table, or None if unresolvable -- rp2040/
    rp2350 have no group-role resolver (group_role_resolver is always None,
    falling back to the generic positional guess)."""
    from .. import zephyr_data  # noqa: PLC0415 -- avoids circular import at module load
    from . import VARIANTS  # noqa: PLC0415

    variant_info = VARIANTS.get(zephyr_data().get("variant") or "")
    instance_pins = (
        variant_info.uart_valid_pins_by_instance.get(port_label.upper())
        if variant_info is not None
        else None
    )
    decoder = value_role_decoder(instance_pins)
    if decoder is None:
        return None
    return None, decoder


def to_code(config: ConfigType) -> None:
    """rpi_pico-family zephyr_to_code hook: mainline Zephyr's MINIMAL_LIBCPP
    has no STL, which ESPHome's C++ core requires regardless of chip vendor --
    same reasoning as every other family. Also enables BOOTSEL-touch: lets
    logger_zephyr.cpp's USB_CDC poll loop detect a host opening the port at
    1200 baud (the cross-ecosystem "magic baud rate" convention) and reboot
    into the ROM USB bootloader without needing the physical button -- backed
    entirely by already-merged Zephyr infrastructure (subsys/retention's
    bootmode API + the RP2 SoC's own PRE_KERNEL_2 hook that acts on it).

    Applied directly rather than via the upstream `rp2-boot-mode-retention`
    snippet: that snippet's board-matching regex (`.*/rp2350b?/.*`) doesn't
    account for the "A package" qualifier real RP2350 boards use
    (`xiao_rp2350/rp2350a/m33`), so its devicetree overlay silently never
    applies there, and CONFIG_RETENTION_BOOT_MODE gets silently dropped for
    lack of the "zephyr,boot-mode" chosen node it depends on."""
    import esphome.codegen as cg  # noqa: PLC0415
    from esphome.const import CONF_LOG_LEVEL  # noqa: PLC0415

    from .. import (  # noqa: PLC0415 -- avoids circular import at module load
        zephyr_add_overlay,
        zephyr_add_prj_conf,
        zephyr_data,
        zephyr_variant,
    )
    from ..const import (  # noqa: PLC0415
        BOOTLOADER_MCUBOOT,
        KEY_BOOTLOADER,
        ZEPHYR_VARIANT_RP2040,
    )

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    # rp2040 is Cortex-M0+, no hardware FPU; rp2350 is Cortex-M33, which has one.
    if zephyr_variant() != ZEPHYR_VARIANT_RP2040:
        zephyr_add_prj_conf("FPU", True)
    # No ENTROPY_GENERATOR here: rp2040 has no hardware RNG at all; rp2350's does
    # exist but Zephyr's driver for it hangs the whole boot sequence (unbounded
    # busy-wait, no timeout -- see rp2350.py's TEST_RANDOM_GENERATOR).
    # ARM Cortex-M's ARCH_HAS_STACKWALK only defaults on when EXTRA_EXCEPTION_INFO
    # is also set (arch/arm/core/Kconfig selects the dependency it needs).
    log_level = config.get(CONF_LOG_LEVEL, "ERROR")
    if log_level != "NONE":
        zephyr_add_prj_conf("EXTRA_EXCEPTION_INFO", True)
        zephyr_add_prj_conf("EXCEPTION_STACK_TRACE", True)
    # Consumed by C++ code shared across every rpi_pico-family variant (core.cpp, etc.).
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_RPI_PICO")

    boot_mode_dtsi = (
        "rp2040-boot-mode-retention"
        if zephyr_variant() == ZEPHYR_VARIANT_RP2040
        else "rp2350-boot-mode-retention"
    )
    boot_mode_overlay = f"#include <vendor/raspberrypi/{boot_mode_dtsi}.dtsi>"
    # When mcuboot is enabled, it -- not the app -- is what runs first on the next
    # boot after the app calls bootmode_set()+sys_reboot(). The RP2 SoC's own
    # PRE_KERNEL_2 hook that reads the retained flag and jumps into the ROM USB
    # bootloader has to run inside mcuboot's own boot sequence, since mcuboot
    # decides whether to chain-load the app at all -- so mcuboot's own build needs
    # this Kconfig/overlay applied too (image="mcuboot"), or nothing ever reads the
    # flag and mcuboot just boots the app slot as normal.
    images = ("",)
    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        images = ("", "mcuboot")
    for image in images:
        zephyr_add_prj_conf("RETAINED_MEM", True, image=image)
        zephyr_add_prj_conf("RETENTION", True, image=image)
        zephyr_add_prj_conf("RETENTION_BOOT_MODE", True, image=image)
        zephyr_add_overlay(boot_mode_overlay, image)
    cg.add_build_flag("-DUSE_ZEPHYR_BOOTSEL_TOUCH")


def signed_image_flash_address(signed_hex: Path) -> int | None:
    """Return the absolute flash address imgtool wrote the signed image at, by
    reading the Intel HEX records at the top of the file -- signed.bin (raw binary)
    carries no address of its own, and this is the build's own authoritative value
    rather than one derived/guessed locally.
    """
    with Path(signed_hex).open(encoding="ascii") as f:
        extended_line = f.readline().strip()
        data_line = f.readline().strip()
    # ":02" byte count, "0000" record address, "04" = extended linear address,
    # followed by the upper 16 bits of the 32-bit address.
    if not extended_line.startswith(":02000004"):
        return None
    upper16 = int(extended_line[9:13], 16)
    # Data record: ":<len><addr16><00><data...><checksum>" -- addr16 is the low 16
    # bits of this record's address, not necessarily 0 (e.g. non-64KB-aligned slots).
    if len(data_line) < 9 or data_line[7:9] != "00":
        return None
    lower16 = int(data_line[3:7], 16)
    return (upper16 << 16) | lower16


def find_picotool() -> Path | None:
    import shutil  # noqa: PLC0415
    import sys  # noqa: PLC0415

    from esphome.util import PICOTOOL_PACKAGE  # noqa: PLC0415

    binary_name = "picotool.exe" if sys.platform == "win32" else "picotool"
    pio_packages = Path.home() / ".platformio" / "packages"
    picotool = pio_packages / PICOTOOL_PACKAGE / binary_name
    if not picotool.is_file():
        picotool = Path(shutil.which(binary_name) or "")
    return picotool if picotool and picotool.is_file() else None


def touch_1200_baud_reboot(port: str, timeout: float = 10.0) -> bool:
    """Trigger an RP2040/RP2350 running ESPHome's own firmware (with
    USE_ZEPHYR_BOOTSEL_TOUCH's poll loop active) to reboot into BOOTSEL, by briefly
    opening its USB CDC serial port at 1200 baud -- the cross-ecosystem "magic baud
    rate" convention -- then waiting for it to re-enumerate in BOOTSEL mode.
    """
    import time  # noqa: PLC0415

    import serial  # noqa: PLC0415

    from esphome.util import get_serial_ports  # noqa: PLC0415

    picotool = find_picotool()
    if picotool is None:
        return False

    # Once triggered, BOOTSEL mode is anonymous -- picotool can't tell devices apart
    # (no serial number/bus-address selection is used anywhere in this upload path), so
    # there's no way to prove after the fact that whatever appears in BOOTSEL is really
    # the board we touched, versus some other RP-family device on the system. The best
    # available guard is refusing up front whenever more than one serial-capable
    # candidate is present -- a device already sitting in raw BOOTSEL can't be counted
    # here too, since detecting it requires `picotool info -d`, which is not reliable
    # enough to depend on for a safety check (see upload_using_picotool()).
    if len(get_serial_ports()) > 1:
        _LOGGER.error(
            "More than one RP2040/RP2350-capable device is connected. Disconnect all "
            "but the target device before uploading, or put the target into BOOTSEL "
            "mode manually and select it explicitly."
        )
        return False

    try:
        with serial.Serial(port, baudrate=1200):
            pass
    except serial.SerialException as err:
        _LOGGER.warning("Could not open %s at 1200 baud: %s", port, err)
        return False

    # `picotool info -d` isn't reliable enough to poll for BOOTSEL re-enumeration (see
    # upload_using_picotool()) -- instead, wait for the touched port itself to
    # disappear, which is what actually happens when the device reboots out of CDC-ACM.
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if port not in (p.path for p in get_serial_ports()):
            return True
        time.sleep(0.5)
    return False


def upload_using_picotool() -> bool:
    """Upload Zephyr firmware to an RP2040/RP2350 device in BOOTSEL mode using picotool."""
    import sys  # noqa: PLC0415

    from esphome.util import is_picotool_usb_permission_error  # noqa: PLC0415

    from .. import zephyr_data  # noqa: PLC0415 -- avoids circular import at module load
    from ..const import BOOTLOADER_MCUBOOT, KEY_BOOTLOADER  # noqa: PLC0415

    picotool = find_picotool()
    if picotool is None:
        _LOGGER.error(
            "picotool not found. Install it via PlatformIO (rp2040 platform) "
            "or your system package manager."
        )
        return False

    # sysbuild produces two separate images when mcuboot is enabled: the bootloader
    # itself (linked to run from the start of flash) and the app, signed and linked
    # to run from slot0_partition instead. Unlike a direct-boot image, the app alone
    # has no valid image at the boot ROM's fixed entry address (flash offset 0), so
    # both must be written -- Zephyr's own "uf2" runner doesn't handle this correctly
    # either (its zephyr.uf2 output is always built from the unsigned binary).
    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        from esphome.core import CORE  # noqa: PLC0415

        mcuboot_elf = CORE.relative_build_path(".west_build/mcuboot/zephyr/zephyr.elf")
        signed_bin = CORE.relative_build_path(
            ".west_build/zephyr/zephyr/zephyr.signed.bin"
        )
        signed_hex = CORE.relative_build_path(
            ".west_build/zephyr/zephyr/zephyr.signed.hex"
        )
        if (
            not mcuboot_elf.is_file()
            or not signed_bin.is_file()
            or not signed_hex.is_file()
        ):
            _LOGGER.error("MCUboot firmware not found. Compile first.")
            return False
        address = signed_image_flash_address(signed_hex)
        if address is None:
            _LOGGER.error(
                "Could not determine the signed app image's flash address from %s.",
                signed_hex,
            )
            return False
        # (file, offset, execute-after-load) -- mcuboot first (no execute, so the
        # device stays in BOOTSEL for the second write), then the signed app.
        loads: list[tuple[Path, int | None, bool]] = [
            (mcuboot_elf, None, False),
            (signed_bin, address, True),
        ]
    else:
        from esphome.core import CORE  # noqa: PLC0415

        elf = CORE.relative_build_path(".west_build/zephyr/zephyr/zephyr.elf")
        if not elf.is_file():
            _LOGGER.error("Zephyr firmware ELF not found. Compile first.")
            return False
        loads = [(elf, None, True)]

    for file_path, offset, execute in loads:
        _LOGGER.info("Uploading %s via picotool...", file_path.name)
        cmd = [str(picotool), "load"]
        if offset is not None:
            cmd += ["-o", hex(offset)]
        if execute:
            cmd.append("-x")
        cmd.append(str(file_path))
        try:
            result = subprocess.run(
                cmd,
                stderr=subprocess.PIPE,
                timeout=60,
                check=False,
            )
        except subprocess.TimeoutExpired:
            _LOGGER.error("picotool upload timed out after 60 seconds.")
            return False
        except OSError as err:
            _LOGGER.error("Failed to run picotool: %s", err)
            return False

        if result.returncode != 0:
            stderr = result.stderr.decode("utf-8", errors="replace").strip()
            if stderr:
                for line in stderr.splitlines():
                    _LOGGER.error("picotool: %s", line)
            if is_picotool_usb_permission_error(stderr):
                msg = "Permission denied accessing USB device."
                if sys.platform.startswith("linux"):
                    from esphome.__main__ import _RP2040_UDEV_HINT  # noqa: PLC0415

                    msg += f" {_RP2040_UDEV_HINT}"
                _LOGGER.error(msg)
            else:
                _LOGGER.error(
                    "picotool upload failed (exit code %d).", result.returncode
                )
            return False
    return True


def upload_program(host: str) -> bool:
    """rpi_pico-family upload flow for a normal serial port (not already in
    BOOTSEL): the board's default west runner (uf2) only works once already in
    BOOTSEL, so trigger that ourselves first via the 1200-baud touch (see
    to_code()'s USE_ZEPHYR_BOOTSEL_TOUCH), then flash the same way a
    BOOTSEL-mode device would."""
    from esphome.upload_targets import PortType, get_port_type  # noqa: PLC0415

    if get_port_type(host) != PortType.SERIAL:
        return False

    from esphome.core import EsphomeError  # noqa: PLC0415

    from .. import zephyr_data  # noqa: PLC0415 -- avoids circular import at module load
    from ..const import KEY_RUNNER  # noqa: PLC0415

    if zephyr_data().get(KEY_RUNNER):
        _LOGGER.info(
            "Configured advanced.runner has no effect on BOOTSEL/picotool flashing"
        )
    if not touch_1200_baud_reboot(host):
        raise EsphomeError(
            f"Could not trigger a BOOTSEL reboot on {host}. Manually put the "
            "device into BOOTSEL mode (hold BOOTSEL while plugging in) and retry."
        )
    return upload_using_picotool()
