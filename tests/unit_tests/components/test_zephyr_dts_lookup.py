"""Unit tests for esphome.components.zephyr.dts_lookup."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.components.zephyr import dts_lookup
from esphome.components.zephyr.const import KEY_ZEPHYR
from esphome.components.zephyr.dts_lookup import (
    _extract_esp32_i2c_pins,
    _find_board_dir,
    _find_board_yaml,
    _find_dts_file,
    _format_size,
    _read_dts_with_includes,
    get_board_partitions,
    get_board_yaml_supported,
    get_enabled_spi_buses,
    get_enabled_uart_buses,
    get_i2c_pinctrl_esp32,
    log_board_capabilities,
    resolve_zephyr_bus,
)
import esphome.config_validation as cv
from esphome.core import CORE


def _empty_zd(**overrides) -> dict:
    return {
        "i2c_bus_cache": {},
        "cpp_path": "",
        "board_dir_cache": {},
        "dts_include_paths": None,
        "board_edt_cache": {},
        "board_yaml_cache": {},
        "dts_base_path": None,
        **overrides,
    }


class _FakeReg:
    def __init__(self, addr: int, size: int) -> None:
        self.addr = addr
        self.size = size


class _FakeNode:
    def __init__(
        self,
        labels: list[str] | None = None,
        status: str = "okay",
        compats: list[str] | None = None,
        label: str | None = None,
        regs: list[_FakeReg] | None = None,
        parent: _FakeNode | None = None,
    ) -> None:
        self.labels = labels or []
        self.status = status
        self.compats = compats or []
        self.label = label
        self.regs = regs or []
        self.parent = parent


class _FakeEdt:
    def __init__(self, nodes: list[_FakeNode]) -> None:
        self.scc_order = list(nodes)


# ---------------------------------------------------------------------------
# resolve_zephyr_bus -- explicit override parameter
# ---------------------------------------------------------------------------


def test_resolve_zephyr_bus_returns_explicit_override() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    assert resolve_zephyr_bus("i2c", "some_board", override="i2c99") == "i2c99"


def test_resolve_zephyr_bus_raises_when_nothing_resolves() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    with pytest.raises(cv.Invalid, match="Cannot determine I2C bus label"):
        resolve_zephyr_bus("i2c", "unknown_board")


# ---------------------------------------------------------------------------
# _find_board_dir / _find_dts_file -- real fixture board tree
# ---------------------------------------------------------------------------


@pytest.fixture
def fake_zephyr_base(tmp_path: Path) -> Path:
    """Build a minimal boards/ tree mimicking HWMv2 layout, including a board
    with two qualifier-suffixed .dts files (like esp32c6_devkitc's hpcore/lpcore)
    to exercise the qualifier-disambiguation fix in _find_dts_file.
    """
    boards = tmp_path / "boards"
    single = boards / "espressif" / "esp32h2_devkitm"
    single.mkdir(parents=True)
    (single / "esp32h2_devkitm.dts").write_text("/ { };")

    dual = boards / "espressif" / "esp32c6_devkitc"
    dual.mkdir(parents=True)
    (dual / "esp32c6_devkitc_lpcore.dts").write_text("/* lpcore */")
    (dual / "esp32c6_devkitc_hpcore.dts").write_text("/* hpcore */")

    return tmp_path


def test_find_board_dir_locates_vendor_subfolder(fake_zephyr_base: Path) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    result = _find_board_dir(fake_zephyr_base, "esp32h2_devkitm/esp32h2")
    assert result == fake_zephyr_base / "boards" / "espressif" / "esp32h2_devkitm"


def test_find_board_dir_returns_none_for_unknown_board(fake_zephyr_base: Path) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    assert _find_board_dir(fake_zephyr_base, "no_such_board/soc") is None


def test_find_dts_file_two_part_board_string(fake_zephyr_base: Path) -> None:
    board_dir = fake_zephyr_base / "boards" / "espressif" / "esp32h2_devkitm"
    result = _find_dts_file(board_dir, "esp32h2_devkitm/esp32h2")
    assert result == board_dir / "esp32h2_devkitm.dts"


def test_find_dts_file_picks_correct_qualifier_not_first_glob_match(
    fake_zephyr_base: Path,
) -> None:
    """Regression test: a board directory with two .dts files (hpcore/lpcore) must
    resolve to the one matching the board string's qualifier segment, not whichever
    file glob() happens to list first (filesystem order is not alphabetical/stable).
    """
    board_dir = fake_zephyr_base / "boards" / "espressif" / "esp32c6_devkitc"
    result = _find_dts_file(board_dir, "esp32c6_devkitc/esp32c6/hpcore")
    assert result == board_dir / "esp32c6_devkitc_hpcore.dts"

    result_lp = _find_dts_file(board_dir, "esp32c6_devkitc/esp32c6/lpcore")
    assert result_lp == board_dir / "esp32c6_devkitc_lpcore.dts"


def test_find_dts_file_falls_back_to_glob_when_unambiguous(tmp_path: Path) -> None:
    board_dir = tmp_path / "only_board"
    board_dir.mkdir()
    (board_dir / "only_board.dts").write_text("/* only */")
    result = _find_dts_file(board_dir, "only_board/soc")
    assert result == board_dir / "only_board.dts"


# ---------------------------------------------------------------------------
# _read_dts_with_includes
# ---------------------------------------------------------------------------


def test_read_dts_with_includes_inlines_quoted_include(tmp_path: Path) -> None:
    (tmp_path / "pins.dtsi").write_text("PINCTRL_CONTENT")
    main = tmp_path / "board.dts"
    main.write_text('before\n#include "pins.dtsi"\nafter')

    text = _read_dts_with_includes(main, tmp_path)
    assert "before" in text
    assert "PINCTRL_CONTENT" in text
    assert "after" in text


def test_read_dts_with_includes_missing_include_is_dropped_silently(
    tmp_path: Path,
) -> None:
    main = tmp_path / "board.dts"
    main.write_text('before\n#include "missing.dtsi"\nafter')
    text = _read_dts_with_includes(main, tmp_path)
    assert "before" in text
    assert "after" in text


def test_read_dts_with_includes_guards_against_circular_includes(
    tmp_path: Path,
) -> None:
    a = tmp_path / "a.dtsi"
    b = tmp_path / "b.dtsi"
    a.write_text('a-content\n#include "b.dtsi"')
    b.write_text('b-content\n#include "a.dtsi"')
    # Must terminate, not infinitely recurse.
    text = _read_dts_with_includes(a, tmp_path)
    assert "a-content" in text
    assert "b-content" in text


# ---------------------------------------------------------------------------
# _extract_esp32_i2c_pins / get_i2c_pinctrl_esp32
# ---------------------------------------------------------------------------

_H2_PINCTRL_DTS = """
&pinctrl {
    i2c0_default: i2c0_default {
        group1 {
            pinmux = <I2C0_SDA_GPIO0>,
                     <I2C0_SCL_GPIO1>;
            bias-pull-up;
            drive-open-drain;
            output-high;
        };
    };
};
"""

_C6_PINCTRL_DTS = """
&pinctrl {
    i2c0_default: i2c0_default {
        group1 {
            pinmux = <I2C0_SDA_GPIO6>,
                     <I2C0_SCL_GPIO7>;
            bias-pull-up;
            drive-open-drain;
            output-high;
        };
    };
};
"""


def test_extract_esp32_i2c_pins_h2() -> None:
    assert _extract_esp32_i2c_pins(_H2_PINCTRL_DTS, "i2c0") == {"sda": 0, "scl": 1}


def test_extract_esp32_i2c_pins_c6() -> None:
    assert _extract_esp32_i2c_pins(_C6_PINCTRL_DTS, "i2c0") == {"sda": 6, "scl": 7}


def test_extract_esp32_i2c_pins_returns_none_when_node_absent() -> None:
    assert _extract_esp32_i2c_pins("/ { totally-unrelated; };", "i2c0") is None


def test_extract_esp32_i2c_pins_ignores_macros_outside_the_node() -> None:
    """A board file can mention another bus label's macro (or a comment) elsewhere
    in the file -- the extractor must not pick that up for the wrong bus_label."""
    text = "// see I2C1_SDA_GPIO99 for the alternate bus\n" + _H2_PINCTRL_DTS
    assert _extract_esp32_i2c_pins(text, "i2c0") == {"sda": 0, "scl": 1}
    assert _extract_esp32_i2c_pins(text, "i2c1") is None


def test_get_i2c_pinctrl_esp32_end_to_end(tmp_path: Path) -> None:
    boards = tmp_path / "boards" / "espressif" / "esp32h2_devkitm"
    boards.mkdir(parents=True)
    (boards / "esp32h2_devkitm.dts").write_text(_H2_PINCTRL_DTS)

    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    result = get_i2c_pinctrl_esp32("esp32h2_devkitm/esp32h2", "i2c0")
    assert result == {"sda": 0, "scl": 1}


def test_get_i2c_pinctrl_esp32_returns_none_without_dts_base_path() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=None)
    assert get_i2c_pinctrl_esp32("esp32h2_devkitm/esp32h2", "i2c0") is None


def test_get_i2c_pinctrl_esp32_returns_none_for_unknown_board(tmp_path: Path) -> None:
    (tmp_path / "boards").mkdir()
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    assert get_i2c_pinctrl_esp32("no_such_board/soc", "i2c0") is None


# ---------------------------------------------------------------------------
# _find_board_yaml / get_board_yaml_supported
# ---------------------------------------------------------------------------


def test_find_board_yaml_two_part_board_string(fake_zephyr_base: Path) -> None:
    board_dir = fake_zephyr_base / "boards" / "espressif" / "esp32h2_devkitm"
    (board_dir / "esp32h2_devkitm.yaml").write_text("supported:\n  - uart\n")
    result = _find_board_yaml(board_dir, "esp32h2_devkitm/esp32h2")
    assert result == board_dir / "esp32h2_devkitm.yaml"


def test_find_board_yaml_picks_correct_qualifier(fake_zephyr_base: Path) -> None:
    board_dir = fake_zephyr_base / "boards" / "espressif" / "esp32c6_devkitc"
    (board_dir / "esp32c6_devkitc_hpcore.yaml").write_text("supported:\n  - spi\n")
    (board_dir / "esp32c6_devkitc_lpcore.yaml").write_text("supported:\n  - uart\n")
    result = _find_board_yaml(board_dir, "esp32c6_devkitc/esp32c6/hpcore")
    assert result == board_dir / "esp32c6_devkitc_hpcore.yaml"


def test_get_board_yaml_supported_end_to_end(tmp_path: Path) -> None:
    boards = tmp_path / "boards" / "espressif" / "esp32h2_devkitm"
    boards.mkdir(parents=True)
    (boards / "esp32h2_devkitm.yaml").write_text(
        "identifier: esp32h2_devkitm/esp32h2\nsupported:\n  - uart\n  - i2c\n  - adc\n"
    )
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    assert get_board_yaml_supported("esp32h2_devkitm/esp32h2") == ["adc", "i2c", "uart"]


def test_get_board_yaml_supported_returns_none_without_dts_base_path() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=None)
    assert get_board_yaml_supported("esp32h2_devkitm/esp32h2") is None


def test_get_board_yaml_supported_returns_none_for_malformed_yaml(
    tmp_path: Path,
) -> None:
    boards = tmp_path / "boards" / "espressif" / "esp32h2_devkitm"
    boards.mkdir(parents=True)
    (boards / "esp32h2_devkitm.yaml").write_text("not: valid: yaml: at: all:")
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    assert get_board_yaml_supported("esp32h2_devkitm/esp32h2") is None


def test_get_board_yaml_supported_returns_none_missing_supported_key(
    tmp_path: Path,
) -> None:
    boards = tmp_path / "boards" / "espressif" / "esp32h2_devkitm"
    boards.mkdir(parents=True)
    (boards / "esp32h2_devkitm.yaml").write_text("identifier: esp32h2_devkitm\n")
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    assert get_board_yaml_supported("esp32h2_devkitm/esp32h2") == []


def test_get_board_yaml_supported_is_cached(tmp_path: Path, monkeypatch) -> None:
    boards = tmp_path / "boards" / "espressif" / "esp32h2_devkitm"
    boards.mkdir(parents=True)
    yaml_file = boards / "esp32h2_devkitm.yaml"
    yaml_file.write_text("supported:\n  - uart\n")
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))

    assert get_board_yaml_supported("esp32h2_devkitm/esp32h2") == ["uart"]
    yaml_file.unlink()
    # Cached, so removing the file afterward must not change the (cached) result.
    assert get_board_yaml_supported("esp32h2_devkitm/esp32h2") == ["uart"]


# ---------------------------------------------------------------------------
# get_enabled_spi_buses / get_enabled_uart_buses / get_board_partitions
# -- via a fake EDT (real edtlib/cpp integration is exercised manually; these
# functions only touch _get_edt()/_iter_nodes(), so a duck-typed fake node is
# sufficient and much cheaper than a full DTS+bindings+cpp fixture).
# ---------------------------------------------------------------------------


def test_get_enabled_spi_buses_prefers_enabled_over_disabled(monkeypatch) -> None:
    nodes = [
        _FakeNode(labels=["spi0"], status="disabled"),
        _FakeNode(labels=["spi1"], status="okay"),
    ]
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: _FakeEdt(nodes))
    assert get_enabled_spi_buses("some_board") == ["spi1"]


def test_get_enabled_uart_buses_falls_back_to_disabled_when_none_enabled(
    monkeypatch,
) -> None:
    nodes = [
        _FakeNode(labels=["uart0"], status="disabled"),
        _FakeNode(labels=["uart1"], status="disabled"),
    ]
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: _FakeEdt(nodes))
    assert get_enabled_uart_buses("some_board") == ["uart0", "uart1"]


def test_get_enabled_spi_buses_returns_none_when_dts_unavailable(monkeypatch) -> None:
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: None)
    assert get_enabled_spi_buses("some_board") is None


def test_get_board_partitions_matches_parent_fixed_partitions_compat(
    monkeypatch,
) -> None:
    parent = _FakeNode(compats=["fixed-partitions"])
    nodes = [
        _FakeNode(label="image-1", regs=[_FakeReg(0x1E0000, 0x1C0000)], parent=parent),
        _FakeNode(label="mcuboot", regs=[_FakeReg(0x1000, 0xF000)], parent=parent),
    ]
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: _FakeEdt(nodes))
    assert get_board_partitions("some_board") == [
        ("mcuboot", 0x1000, 0xF000),
        ("image-1", 0x1E0000, 0x1C0000),
    ]


def test_get_board_partitions_matches_own_mapped_partition_compat(monkeypatch) -> None:
    """Covers this project's custom boards, which set compatible directly on each
    partition child instead of only on the parent (see esp32_devkit_procpu_only)."""
    nodes = [
        _FakeNode(
            label="storage",
            compats=["zephyr,mapped-partition"],
            regs=[_FakeReg(0x3A0000, 0x40000)],
        ),
    ]
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: _FakeEdt(nodes))
    assert get_board_partitions("some_board") == [("storage", 0x3A0000, 0x40000)]


def test_get_board_partitions_skips_nodes_without_label_or_regs(monkeypatch) -> None:
    parent = _FakeNode(compats=["fixed-partitions"])
    nodes = [
        _FakeNode(label=None, regs=[_FakeReg(0, 0x1000)], parent=parent),
        _FakeNode(label="unlabeled-but-no-regs", regs=[], parent=parent),
        _FakeNode(label="storage", regs=[_FakeReg(0x10000, 0x1000)], parent=parent),
    ]
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: _FakeEdt(nodes))
    assert get_board_partitions("some_board") == [("storage", 0x10000, 0x1000)]


def test_get_board_partitions_returns_none_when_dts_unavailable(monkeypatch) -> None:
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: None)
    assert get_board_partitions("some_board") is None


# ---------------------------------------------------------------------------
# _format_size
# ---------------------------------------------------------------------------


def test_format_size_scales_to_k_when_aligned() -> None:
    assert _format_size(1835008) == "1792K"
    assert _format_size(4096) == "4K"


def test_format_size_falls_back_to_bytes_when_not_k_aligned() -> None:
    assert _format_size(1000) == "1000B"


# ---------------------------------------------------------------------------
# log_board_capabilities -- combined report composition
# ---------------------------------------------------------------------------


def test_log_board_capabilities_stock_board_full_report(monkeypatch, caplog) -> None:
    from esphome.components.zephyr.variants import ZephyrSDK, ZephyrVariant

    _fake_sdk = ZephyrSDK(manifest_url="https://example.invalid/zephyr")

    variant = ZephyrVariant(sdk=_fake_sdk, swap_methods=frozenset({"scratch", "move"}))
    monkeypatch.setattr(
        dts_lookup, "get_board_yaml_supported", lambda board: ["adc", "i2c"]
    )
    monkeypatch.setattr(dts_lookup, "get_board_features", lambda board: ["WiFi"])
    monkeypatch.setattr(dts_lookup, "get_enabled_i2c_buses", lambda board: ["i2c0"])
    monkeypatch.setattr(dts_lookup, "get_enabled_spi_buses", lambda board: None)
    monkeypatch.setattr(dts_lookup, "get_enabled_uart_buses", lambda board: [])
    monkeypatch.setattr(
        dts_lookup,
        "get_board_partitions",
        lambda board: [("mcuboot", 0x1000, 0xF000)],
    )

    with caplog.at_level("INFO"):
        log_board_capabilities("my_board", "esp32_c6", variant, "4.4.1", None)

    message = caplog.text
    assert "Board 'my_board' (stock Zephyr board), Zephyr 4.4.1" in message
    assert "Declared in board definition: adc, i2c" in message
    assert "Also detected via DTS scan" in message and "WiFi" in message
    assert "I2C buses present: i2c0" in message
    assert "SPI buses present: (none or unavailable)" in message
    assert "UART buses present: (none or unavailable)" in message
    assert "MCUboot swap methods this variant's port supports: move, scratch" in message
    assert "Flash partitions defined by the board:" in message
    assert "mcuboot" in message and "0x001000" in message and "60K" in message


def test_log_board_capabilities_custom_board_shows_board_root(
    monkeypatch, caplog, tmp_path: Path
) -> None:
    from esphome.components.zephyr.variants import ZephyrSDK, ZephyrVariant

    _fake_sdk = ZephyrSDK(manifest_url="https://example.invalid/zephyr")

    variant = ZephyrVariant(sdk=_fake_sdk)
    for name in (
        "get_board_yaml_supported",
        "get_board_features",
        "get_enabled_i2c_buses",
        "get_enabled_spi_buses",
        "get_enabled_uart_buses",
        "get_board_partitions",
    ):
        monkeypatch.setattr(dts_lookup, name, lambda board: None)

    with caplog.at_level("INFO"):
        log_board_capabilities("my_board", "esp32_c6", variant, "4.4.1", tmp_path)

    message = caplog.text
    assert f"custom board, from board_source: {tmp_path.resolve()}" in message


def test_log_board_capabilities_empty_swap_methods_omits_partitions(
    monkeypatch, caplog
) -> None:
    """native_sim-shaped case: no swap_methods (no real MCUboot), so the partition
    section must be omitted entirely -- even though the board's DTS still defines a
    mcuboot/image-* partition layout as boilerplate, showing it would misleadingly
    imply a real bootloader/OTA partition scheme on this variant."""
    from esphome.components.zephyr.variants import ZephyrSDK, ZephyrVariant

    _fake_sdk = ZephyrSDK(manifest_url="https://example.invalid/zephyr")

    variant = ZephyrVariant(
        sdk=_fake_sdk
    )  # swap_methods defaults to frozenset() (empty)
    monkeypatch.setattr(dts_lookup, "get_board_yaml_supported", lambda board: None)
    monkeypatch.setattr(dts_lookup, "get_board_features", lambda board: None)
    monkeypatch.setattr(dts_lookup, "get_enabled_i2c_buses", lambda board: None)
    monkeypatch.setattr(dts_lookup, "get_enabled_spi_buses", lambda board: None)
    monkeypatch.setattr(dts_lookup, "get_enabled_uart_buses", lambda board: None)
    partitions_called = False

    def _unexpected_partitions_call(board):
        nonlocal partitions_called
        partitions_called = True
        return [("mcuboot", 0, 0xC000)]

    monkeypatch.setattr(dts_lookup, "get_board_partitions", _unexpected_partitions_call)

    with caplog.at_level("INFO"):
        log_board_capabilities(
            "native_sim/native/64", "native_sim", variant, "4.4.1", None
        )

    message = caplog.text
    assert "MCUboot swap methods this variant's port supports: (none)" in message
    assert "Flash partitions" not in message
    assert not partitions_called
