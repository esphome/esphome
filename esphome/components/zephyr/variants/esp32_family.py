"""ESP32-family GPIO-matrix pinctrl signal decoding, shared by I2C/UART/SPI
pinctrl overlay generation in zephyr/__init__.py."""

from collections.abc import Callable

from esphome.types import ConfigType

from ..const import (
    ZEPHYR_VARIANT_ESP32,
    ZEPHYR_VARIANT_ESP32_C3,
    ZEPHYR_VARIANT_ESP32_C5,
    ZEPHYR_VARIANT_ESP32_C6,
    ZEPHYR_VARIANT_ESP32_H2,
)

# Devicetree property esp32-family pinctrl groups pack all their signal macros into.
PROPERTY_NAME = "pinmux"


def to_code(config: ConfigType) -> None:
    """REQUIRES_FULL_LIBCPP selects GLIBCXX_LIBCPP; without it Zephyr defaults
    to MINIMAL_LIBCPP, which has no STL and breaks ESPHome's C++ headers."""
    import esphome.codegen as cg  # noqa: PLC0415
    from esphome.const import CONF_LOG_LEVEL  # noqa: PLC0415

    from .. import (  # noqa: PLC0415 -- avoids circular import at module load
        zephyr_add_prj_conf,
        zephyr_variant,
    )

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    # Original ESP32 is Xtensa LX6, which has a hardware FPU; esp32_h2/c6/c5/c3 are
    # RV32IMAC/RV32IMC, none of which do.
    if zephyr_variant() == ZEPHYR_VARIANT_ESP32:
        zephyr_add_prj_conf("FPU", True)
    # random_bytes() uses sys_rand_get(), which requires the entropy subsystem.
    zephyr_add_prj_conf("ENTROPY_GENERATOR", True)
    # arch_stack_walk() isn't implemented for Xtensa (original ESP32); RISC-V
    # (esp32_h2/c6/c5/c3) does implement it and enables ARCH_HAS_STACKWALK
    # unconditionally, so no EXTRA_EXCEPTION_INFO is needed there either.
    log_level = config.get(CONF_LOG_LEVEL, "ERROR")
    if log_level != "NONE" and zephyr_variant() != ZEPHYR_VARIANT_ESP32:
        zephyr_add_prj_conf("EXCEPTION_STACK_TRACE", True)
    # Consumed by C++ code shared across every esp32-family variant (core.cpp, etc.).
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_FAMILY_ESP32")


# I2C pinctrl group/state to assume when DTS resolution fails.
I2C_FALLBACK_GROUP = "group1"
I2C_FALLBACK_STATE_SUFFIXES = ("default",)

# GPIO-matrix pinmux value layout (esp-pinctrl-common.h and per-chip variants):
# sig_i (controller input signal id) and sig_o (controller output signal id) are
# packed into the same 32-bit pinmux value at different bit offsets.
_ESP32_PINMUX_SIGI_SHIFT = 6
_ESP32_PINMUX_SIGO_SHIFT = 15
_ESP32_PINMUX_SIG_MASK = 0x1FF
_ESP32_PINMUX_NOSIG = 0x1FF  # ESP_SIG_INVAL in esp-pinctrl-common.h

# esp32-family I2C instance -> {"scl": id, "sda": id} GPIO-matrix signal IDs. I2C
# is bidirectional, so sig_i and sig_o are always equal -- either field identifies it.
_I2C_INSTANCE_SIGNAL_IDS = {
    ZEPHYR_VARIANT_ESP32: {0: {"scl": 29, "sda": 30}, 1: {"scl": 95, "sda": 96}},
    ZEPHYR_VARIANT_ESP32_C3: {0: {"scl": 53, "sda": 54}},
    ZEPHYR_VARIANT_ESP32_C5: {0: {"scl": 46, "sda": 47}},
    ZEPHYR_VARIANT_ESP32_C6: {0: {"scl": 45, "sda": 46}},
    ZEPHYR_VARIANT_ESP32_H2: {0: {"scl": 45, "sda": 46}, 1: {"scl": 55, "sda": 56}},
}


def pin_macro(prefix: str, signal: str, pin: int | None, variant: str) -> str | None:
    """`variant` is unused -- accepted only for signature parity with the
    other families' pin_macro(). SPI passes an instance macro prefix
    ("SPIM0") instead of a bus label, and "SCLK"/"MISO"/"MOSI" instead of a
    bus signal name -- same shape otherwise."""
    if pin is None:
        return None
    return f"<{prefix}_{signal}_GPIO{pin}>"


def spi_quad_macro(pin: int, instance_prefix: str, signal: str) -> str:
    """Build a raw `ESP32_PINMUX()` macro for SPI quad mode's WP/HD signals --
    no `SPIM{n}_{WP,HD}_GPIO*` convenience macros exist for these two (verified
    absent from every in-scope chip's pinctrl header)."""
    return f"<ESP32_PINMUX({pin}, ESP_NOSIG, ESP_{instance_prefix}{signal}_OUT)>"


def _i2c_value_role(value: int, signal_ids: dict[str, int]) -> str | None:
    sig_o = (value >> _ESP32_PINMUX_SIGO_SHIFT) & _ESP32_PINMUX_SIG_MASK
    for role, sig_id in signal_ids.items():
        if sig_o == sig_id:
            return role
    return None


def i2c_value_role_decoder(bus_label: str) -> Callable[[int], str | None] | None:
    """None if the instance/variant isn't in _I2C_INSTANCE_SIGNAL_IDS."""
    from .. import (
        zephyr_variant,  # noqa: PLC0415 -- avoids circular import at module load
    )

    instance = int(bus_label.removeprefix("i2c")) if bus_label[3:].isdigit() else None
    signal_ids = _I2C_INSTANCE_SIGNAL_IDS.get(zephyr_variant(), {}).get(instance)
    if signal_ids is None:
        return None

    def decoder(value: int) -> str | None:
        return _i2c_value_role(value, signal_ids)

    return decoder


# UART instance -> GPIO matrix signal ID shared by RXD_IN/TXD_OUT (CTS_IN/RTS_OUT
# always use that ID + 1) -- verified against every esp32-family chip's own
# <chip>-gpio-sigmap.h in scope. Direction (sig_i vs sig_o) alone can't tell TX
# from RTS or RX from CTS (RXD_IN/TXD_OUT and CTS_IN/RTS_OUT are each a shared ID
# used in different direction slots) -- this ID is what actually distinguishes them.
_UART_INSTANCE_SIGNAL_BASE = {
    ZEPHYR_VARIANT_ESP32: {0: 14, 1: 17, 2: 198},
    ZEPHYR_VARIANT_ESP32_C3: {0: 6, 1: 9},
    ZEPHYR_VARIANT_ESP32_C5: {0: 6, 1: 9},
    ZEPHYR_VARIANT_ESP32_C6: {0: 6, 1: 9},
    ZEPHYR_VARIANT_ESP32_H2: {0: 6, 1: 9},
}


def _uart_value_role(value: int, signal_base: int) -> str | None:
    """Decode a single real pinmux value against this UART instance's signal IDs --
    RXD_IN/TXD_OUT share `signal_base`, CTS_IN/RTS_OUT share `signal_base + 1`."""
    sig_i = (value >> _ESP32_PINMUX_SIGI_SHIFT) & _ESP32_PINMUX_SIG_MASK
    sig_o = (value >> _ESP32_PINMUX_SIGO_SHIFT) & _ESP32_PINMUX_SIG_MASK
    if sig_o == signal_base:
        return "tx"
    if sig_o == signal_base + 1:
        return "rts"
    if sig_i == signal_base:
        return "rx"
    if sig_i == signal_base + 1:
        return "cts"
    return None


def _uart_signal_groups(
    board: str, label: str, groups: list[str], signal_base: int
) -> dict[str, str]:
    """Reads every value in each group's pinmux property, not just the first
    entry, since a board may combine e.g. TX and RTS in one group's array."""
    from ..dts_lookup import get_pinctrl_group_property  # noqa: PLC0415

    signals: dict[str, str] = {}
    for group in groups:
        for value in get_pinctrl_group_property(board, label, group, "pinmux") or []:
            role = _uart_value_role(value, signal_base)
            if role is not None:
                signals[role] = group
    return signals


def uart_pinctrl(
    board: str, port_label: str
) -> (
    tuple[
        Callable[[str, str, list[str]], dict[str, str] | None],
        Callable[[int], str | None],
    ]
    | None
):
    """Return (group_role_resolver, value_role_decoder) bound to port_label's real
    UART instance signal IDs, or None if the instance/variant isn't in
    _UART_INSTANCE_SIGNAL_BASE (falls back to the positional guess)."""
    from .. import (
        zephyr_variant,  # noqa: PLC0415 -- avoids circular import at module load
    )

    instance = (
        int(port_label.removeprefix("uart")) if port_label[4:].isdigit() else None
    )
    signal_base = _UART_INSTANCE_SIGNAL_BASE.get(zephyr_variant(), {}).get(instance)
    if signal_base is None:
        return None

    def resolver(board: str, label: str, groups: list[str]) -> dict[str, str] | None:
        signals = _uart_signal_groups(board, label, groups, signal_base)
        if "tx" not in signals or "rx" not in signals:
            return None
        return signals

    def value_role_decoder(value: int) -> str | None:
        return _uart_value_role(value, signal_base)

    return resolver, value_role_decoder


# esp32-family SPI instance -> GPIO-matrix signal prefix, verified against real
# per-chip devicetree source (v4.4.1): base ESP32 has two general-purpose SPI
# controllers, spi2=HSPI/spi3=VSPI (esp32_common.dtsi's
# `clocks = <&clock ESP32_HSPI_MODULE>`/`ESP32_VSPI_MODULE`). C3/C5/C6/H2 each have
# only a single one, spi2=FSPI (confirmed via each chip's own -gpio-sigmap.h; no
# spi3 node exists on any of them). S2/S3/P4 are not supported variants in this repo
# yet and are deliberately not included here.
SPI_INSTANCE_SIGNAL_PREFIX = {
    ZEPHYR_VARIANT_ESP32: {2: "HSPI", 3: "VSPI"},
    ZEPHYR_VARIANT_ESP32_C3: {2: "FSPI"},
    ZEPHYR_VARIANT_ESP32_C5: {2: "FSPI"},
    ZEPHYR_VARIANT_ESP32_C6: {2: "FSPI"},
    ZEPHYR_VARIANT_ESP32_H2: {2: "FSPI"},
}

# esp32-family SPI instance -> {"clk": id, "miso": id, "mosi": id, "wp": id, "hd":
# id} GPIO-matrix signal IDs, from each chip's own <chip>-gpio-sigmap.h (verified
# against zephyr main, 2026-09-09). "miso" is the only sig_i (controller input);
# clk/mosi/wp/hd are all sig_o -- used to decode which real board pinctrl group
# already carries each signal.
_SPI_INSTANCE_SIGNAL_IDS = {
    ZEPHYR_VARIANT_ESP32: {
        2: {"clk": 8, "miso": 9, "mosi": 10, "hd": 12, "wp": 13},
        3: {"clk": 63, "miso": 64, "mosi": 65, "hd": 66, "wp": 67},
    },
    ZEPHYR_VARIANT_ESP32_C3: {
        2: {"clk": 63, "miso": 64, "mosi": 65, "hd": 66, "wp": 67}
    },
    ZEPHYR_VARIANT_ESP32_C5: {
        2: {"clk": 56, "miso": 57, "mosi": 58, "hd": 59, "wp": 60}
    },
    ZEPHYR_VARIANT_ESP32_C6: {
        2: {"clk": 63, "miso": 64, "mosi": 65, "hd": 66, "wp": 67}
    },
    ZEPHYR_VARIANT_ESP32_H2: {
        2: {"clk": 63, "miso": 64, "mosi": 65, "hd": 66, "wp": 67}
    },
}


def _spi_value_role(value: int, signal_ids: dict[str, int]) -> str | None:
    sig_i = (value >> _ESP32_PINMUX_SIGI_SHIFT) & _ESP32_PINMUX_SIG_MASK
    sig_o = (value >> _ESP32_PINMUX_SIGO_SHIFT) & _ESP32_PINMUX_SIG_MASK
    for name, sig_id in signal_ids.items():
        if (sig_i if name == "miso" else sig_o) == sig_id:
            return name
    return None


def _spi_signal_groups(
    board: str, label: str, groups: list[str], signal_ids: dict[str, int]
) -> dict[str, str]:
    """Return {"clk": group, ...} for signals found, decoding real pinmux values
    against this instance's signal IDs -- mirrors _uart_signal_groups()."""
    from ..dts_lookup import get_pinctrl_group_property  # noqa: PLC0415

    signals: dict[str, str] = {}
    for group in groups:
        for value in get_pinctrl_group_property(board, label, group, "pinmux") or []:
            role = _spi_value_role(value, signal_ids)
            if role is not None:
                signals[role] = group
    return signals


def upload_program(host: str, args) -> bool:
    """Every esp32-family variant flashes the same way: Zephyr's generic esp32
    runner (runners/esp32.py, wrapping esptool) via `west flash`. Only serial
    is implemented so far -- other host types return False so the caller falls
    through to (or rejects via) its own handling."""
    from esphome.upload_targets import PortType, get_port_type  # noqa: PLC0415

    if get_port_type(host) != PortType.SERIAL:
        return False

    from esphome.__main__ import check_permissions  # noqa: PLC0415
    from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION  # noqa: PLC0415
    from esphome.core import CORE, EsphomeError  # noqa: PLC0415

    from .. import resolve_zephyr_modules, zephyr_data, zephyr_variant  # noqa: PLC0415
    from ..build_zephyr import run_west_flash  # noqa: PLC0415
    from ..const import KEY_FRAMEWORK_TYPE, KEY_RUNNER  # noqa: PLC0415
    from ..framework_west import check_and_install as west_install  # noqa: PLC0415
    from . import VARIANTS, resolve_sdk  # noqa: PLC0415

    check_permissions(host)

    version = str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
    variant_data = VARIANTS[zephyr_variant()]
    _, sdk = resolve_sdk(variant_data, zephyr_data().get(KEY_FRAMEWORK_TYPE))
    python_bin, framework_path, west_env = west_install(
        sdk,
        version,
        zephyr_data()["west_version"],
        zephyr_data()["ninja_version"],
        zephyr_data()["sdk_source"],
        modules=resolve_zephyr_modules(),
    )

    build_dir = CORE.relative_build_path(".west_build")
    speed = getattr(args, "upload_speed", None)

    if not run_west_flash(
        python_bin,
        framework_path,
        west_env,
        build_dir,
        host,
        speed,
        runner=zephyr_data().get(KEY_RUNNER),
    ):
        raise EsphomeError("Zephyr west flash failed")
    return True


def spi_pinctrl(
    board: str, bus_label: str
) -> (
    tuple[
        Callable[[str, str, list[str]], dict[str, str] | None],
        Callable[[int], str | None],
    ]
    | None
):
    """Return (group_role_resolver, value_role_decoder) bound to bus_label's
    signal IDs, or None if unknown (falls back to the single-shared-group guess
    in _resolve_spi_pinctrl_states())."""
    from .. import (
        zephyr_variant,  # noqa: PLC0415 -- avoids circular import at module load
    )

    instance = int(bus_label.removeprefix("spi")) if bus_label[3:].isdigit() else None
    signal_ids = _SPI_INSTANCE_SIGNAL_IDS.get(zephyr_variant(), {}).get(instance)
    if signal_ids is None:
        return None

    def resolver(board: str, label: str, groups: list[str]) -> dict[str, str] | None:
        signals = _spi_signal_groups(board, label, groups, signal_ids)
        return signals or None

    def value_role_decoder(value: int) -> str | None:
        return _spi_value_role(value, signal_ids)

    return resolver, value_role_decoder
