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
    _find_revision_overlay,
    _find_shield_dir,
    _find_snippet_dir,
    _format_size,
    _get_edt,
    _read_dts_with_includes,
    _shield_overlay_files,
    _snippet_overlay_files,
    get_board_partitions,
    get_board_yaml_supported,
    get_can_controller_labels,
    get_i2c_controller_labels,
    get_i2c_pinctrl_esp32,
    get_spi_controller_labels,
    get_uart_controller_labels,
    log_board_capabilities,
    resolve_zephyr_bus,
    validate_board_revision,
)
from esphome.core import CORE, EsphomeError


def _empty_zd(**overrides) -> dict:
    return {
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
        buses: list[str] | None = None,
    ) -> None:
        self.labels = labels or []
        self.status = status
        self.compats = compats or []
        self.label = label
        self.regs = regs or []
        self.parent = parent
        self.buses = buses or []


class _FakeEdt:
    def __init__(self, nodes: list[_FakeNode]) -> None:
        self.scc_order = list(nodes)


# ---------------------------------------------------------------------------
# resolve_zephyr_bus -- explicit override parameter
# ---------------------------------------------------------------------------


def test_resolve_zephyr_bus_returns_explicit_override() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    assert (
        resolve_zephyr_bus(
            "i2c", "some_board", override_key="dts_node_override", override="i2c99"
        )
        == "i2c99"
    )


def test_resolve_zephyr_bus_raises_when_nothing_resolves() -> None:
    # resolve_zephyr_bus() raises EsphomeError, not cv.Invalid -- it runs from
    # to_code(), not CONFIG_SCHEMA, where only cv.Invalid is caught/formatted.
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    with pytest.raises(EsphomeError, match="Cannot determine I2C bus label"):
        resolve_zephyr_bus("i2c", "unknown_board", override_key="dts_node_override")


def test_resolve_zephyr_bus_error_hint_uses_caller_supplied_override_key() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    with pytest.raises(EsphomeError, match="interface: spi0"):
        resolve_zephyr_bus("spi", "unknown_board", override_key="interface")


# ---------------------------------------------------------------------------
# _find_board_dir / _find_dts_file -- real fixture board tree
# ---------------------------------------------------------------------------


@pytest.fixture
def fake_zephyr_base(tmp_path: Path) -> Path:
    """Build a minimal boards/ tree mimicking HWMv2 layout, including a board
    with two qualifier-suffixed .dts files (like esp32c6_devkitc's hpcore/lpcore)
    to exercise the qualifier-disambiguation fix in _find_dts_file, and a board
    whose qualified filenames repeat the <soc> segment (like rpi_pico's plain/
    w/mcuboot family and nRF54L15's multi-die DK) to exercise
    _find_qualified_file's <board>_<soc>_<qualifier> match.
    """
    boards = tmp_path / "boards"
    single = boards / "espressif" / "esp32h2_devkitm"
    single.mkdir(parents=True)
    (single / "esp32h2_devkitm.dts").write_text("/ { };")

    dual = boards / "espressif" / "esp32c6_devkitc"
    dual.mkdir(parents=True)
    (dual / "esp32c6_devkitc_lpcore.dts").write_text("/* lpcore */")
    (dual / "esp32c6_devkitc_hpcore.dts").write_text("/* hpcore */")

    # Real upstream filenames (raspberrypi/rpi_pico) -- soc "rp2040" repeats in the
    # qualified variants' filenames, but not the plain board's.
    rpi_pico = boards / "raspberrypi" / "rpi_pico"
    rpi_pico.mkdir(parents=True)
    (rpi_pico / "rpi_pico.dts").write_text("/* plain */")
    (rpi_pico / "rpi_pico_rp2040_mcuboot.dts").write_text("/* mcuboot */")
    (rpi_pico / "rpi_pico_rp2040_w.dts").write_text("/* w */")
    (rpi_pico / "rpi_pico_rp2040_w_mcuboot.dts").write_text("/* w mcuboot */")

    # Real upstream filenames (nordic/nrf54l15dk) -- many adjacent qualified
    # variants sharing a prefix, to make sure disambiguation doesn't false-match
    # a neighbor (e.g. nrf54l15dk_nrf54l15_cpuapp vs. _cpuapp_ns/_cpuflpr).
    nrf54l15dk = boards / "nordic" / "nrf54l15dk"
    nrf54l15dk.mkdir(parents=True)
    for suffix in (
        "nrf54l05_cpuapp",
        "nrf54l05_cpuflpr",
        "nrf54l10_cpuapp",
        "nrf54l10_cpuapp_ns",
        "nrf54l10_cpuflpr",
        "nrf54l15_cpuapp",
        "nrf54l15_cpuapp_ns",
        "nrf54l15_cpuflpr",
        "nrf54l15_cpuflpr_xip",
    ):
        (nrf54l15dk / f"nrf54l15dk_{suffix}.dts").write_text(f"/* {suffix} */")

    # Real upstream filenames (adafruit/feather_nrf52840) -- board.yml nests a
    # "uf2" variant under the "sense" variant, giving a 4-segment identifier
    # (board/soc/sense/uf2) whose filename doubles the soc AND both variant levels.
    feather = boards / "adafruit" / "feather_nrf52840"
    feather.mkdir(parents=True)
    (feather / "adafruit_feather_nrf52840.dts").write_text("/* plain */")
    (feather / "adafruit_feather_nrf52840_nrf52840_uf2.dts").write_text("/* uf2 */")
    (feather / "adafruit_feather_nrf52840_nrf52840_sense.dts").write_text("/* sense */")
    (feather / "adafruit_feather_nrf52840_nrf52840_sense_uf2.dts").write_text(
        "/* sense uf2 */"
    )

    # Real upstream filenames (raspberrypi/rpi_pico2) -- board.yml's `cpucluster:`
    # attribute inserts its own path segment ("m33"), on top of the variant
    # nesting, giving a 5-segment identifier (board/soc/cpucluster/variant/
    # subvariant) for rp2350a/m33/w/mcuboot.
    rpi_pico2 = boards / "raspberrypi" / "rpi_pico2"
    rpi_pico2.mkdir(parents=True)
    (rpi_pico2 / "rpi_pico2_rp2350a_m33.dts").write_text("/* m33 */")
    (rpi_pico2 / "rpi_pico2_rp2350a_m33_mcuboot.dts").write_text("/* m33 mcuboot */")
    (rpi_pico2 / "rpi_pico2_rp2350a_m33_w.dts").write_text("/* m33 w */")
    (rpi_pico2 / "rpi_pico2_rp2350a_m33_w_mcuboot.dts").write_text(
        "/* m33 w mcuboot */"
    )

    return tmp_path


def test_find_board_dir_locates_vendor_subfolder(fake_zephyr_base: Path) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    result = _find_board_dir(fake_zephyr_base, "esp32h2_devkitm/esp32h2")
    assert result == fake_zephyr_base / "boards" / "espressif" / "esp32h2_devkitm"


def test_find_board_dir_returns_none_for_unknown_board(fake_zephyr_base: Path) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    assert _find_board_dir(fake_zephyr_base, "no_such_board/soc") is None


def test_find_board_dir_strips_revision_suffix(fake_zephyr_base: Path) -> None:
    """A "@<revision>" suffix must not leak into the vendor-folder scan -- otherwise
    a revisioned board string would never resolve to its directory."""
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    result = _find_board_dir(fake_zephyr_base, "esp32h2_devkitm@1.0.0/esp32h2")
    assert result == fake_zephyr_base / "boards" / "espressif" / "esp32h2_devkitm"


def test_find_dts_file_two_part_board_string(fake_zephyr_base: Path) -> None:
    board_dir = fake_zephyr_base / "boards" / "espressif" / "esp32h2_devkitm"
    result = _find_dts_file(board_dir, "esp32h2_devkitm/esp32h2")
    assert result == board_dir / "esp32h2_devkitm.dts"


def test_find_dts_file_strips_revision_suffix(fake_zephyr_base: Path) -> None:
    """The base .dts file is never revision-suffixed -- a "@<revision>" on the bare
    name must be stripped before building the candidate filename."""
    board_dir = fake_zephyr_base / "boards" / "espressif" / "esp32h2_devkitm"
    result = _find_dts_file(board_dir, "esp32h2_devkitm@1.0.0/esp32h2")
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


def test_find_dts_file_doubled_soc_segment_picks_qualified_variant(
    fake_zephyr_base: Path,
) -> None:
    """Regression test for the rpi_pico/rp2040/mcuboot bug: when the qualified
    filename repeats the <soc> segment (rpi_pico_rp2040_mcuboot.dts, not
    rpi_pico_mcuboot.dts), it must still be found -- not silently fall through to
    the plain board's rpi_pico.dts."""
    board_dir = fake_zephyr_base / "boards" / "raspberrypi" / "rpi_pico"
    result = _find_dts_file(board_dir, "rpi_pico/rp2040/mcuboot")
    assert result == board_dir / "rpi_pico_rp2040_mcuboot.dts"

    result_w = _find_dts_file(board_dir, "rpi_pico_rp2040_w/rp2040/mcuboot")
    assert result_w == board_dir / "rpi_pico_rp2040_w_mcuboot.dts"


def test_find_dts_file_doubled_soc_segment_bare_board_picks_plain(
    fake_zephyr_base: Path,
) -> None:
    """The default (no qualifier) board string must still resolve to the plain
    board file, not accidentally match a qualified variant."""
    board_dir = fake_zephyr_base / "boards" / "raspberrypi" / "rpi_pico"
    result = _find_dts_file(board_dir, "rpi_pico/rp2040")
    assert result == board_dir / "rpi_pico.dts"


def test_find_dts_file_doubled_soc_segment_disambiguates_among_many_neighbors(
    fake_zephyr_base: Path,
) -> None:
    """nrf54l15dk-shaped case: several qualified variants share a long common
    prefix (nrf54l15dk_nrf54l15_cpuapp vs. _cpuapp_ns vs. _cpuflpr vs. _cpuflpr_xip,
    plus sibling die revisions nrf54l05/nrf54l10) -- must match exactly, not a
    neighbor with the same prefix."""
    board_dir = fake_zephyr_base / "boards" / "nordic" / "nrf54l15dk"
    result = _find_dts_file(board_dir, "nrf54l15dk/nrf54l15/cpuapp")
    assert result == board_dir / "nrf54l15dk_nrf54l15_cpuapp.dts"

    result_ns = _find_dts_file(board_dir, "nrf54l15dk/nrf54l15/cpuapp_ns")
    assert result_ns == board_dir / "nrf54l15dk_nrf54l15_cpuapp_ns.dts"

    result_l05 = _find_dts_file(board_dir, "nrf54l15dk/nrf54l05/cpuapp")
    assert result_l05 == board_dir / "nrf54l15dk_nrf54l05_cpuapp.dts"


def test_find_dts_file_four_segment_nested_variant(fake_zephyr_base: Path) -> None:
    """adafruit_feather_nrf52840/nrf52840/sense/uf2 -- a 4-segment identifier from
    board.yml's nested `variants:` (uf2 nested under sense). Must match the fully
    qualified file, not the shorter "sense" or "uf2"-only siblings."""
    board_dir = fake_zephyr_base / "boards" / "adafruit" / "feather_nrf52840"

    result = _find_dts_file(board_dir, "adafruit_feather_nrf52840/nrf52840/sense/uf2")
    assert result == board_dir / "adafruit_feather_nrf52840_nrf52840_sense_uf2.dts"

    result_sense = _find_dts_file(board_dir, "adafruit_feather_nrf52840/nrf52840/sense")
    assert result_sense == board_dir / "adafruit_feather_nrf52840_nrf52840_sense.dts"

    result_uf2 = _find_dts_file(board_dir, "adafruit_feather_nrf52840/nrf52840/uf2")
    assert result_uf2 == board_dir / "adafruit_feather_nrf52840_nrf52840_uf2.dts"


def test_find_dts_file_five_segment_cpucluster_and_nested_variant(
    fake_zephyr_base: Path,
) -> None:
    """rpi_pico2/rp2350a/m33/w/mcuboot -- a 5-segment identifier where board.yml's
    `cpucluster:` attribute (m33) inserts its own path segment on top of two
    nested variant levels (w, then mcuboot). Must match the fully qualified file."""
    board_dir = fake_zephyr_base / "boards" / "raspberrypi" / "rpi_pico2"

    result = _find_dts_file(board_dir, "rpi_pico2/rp2350a/m33/w/mcuboot")
    assert result == board_dir / "rpi_pico2_rp2350a_m33_w_mcuboot.dts"

    result_plain_mcuboot = _find_dts_file(board_dir, "rpi_pico2/rp2350a/m33/mcuboot")
    assert result_plain_mcuboot == board_dir / "rpi_pico2_rp2350a_m33_mcuboot.dts"

    result_cluster_only = _find_dts_file(board_dir, "rpi_pico2/rp2350a/m33")
    assert result_cluster_only == board_dir / "rpi_pico2_rp2350a_m33.dts"


# Real-world qualifier depths, 1 through 5 total board-string segments (0 through 4
# "/"-qualifiers after the bare name) -- see the fixtures built above for each
# shape's real upstream precedent (esp32h2_devkitm: 1 qualifier, esp32c6_devkitc: 2,
# adafruit_feather_nrf52840: 3, rpi_pico2: 4, the deepest real case). A purpose-built
# board (rather than reusing those DTS-shape fixtures directly) keeps the qualified
# filename unambiguous at every depth, isolating "does revision handling regress at
# this depth" from the soc-dropping/doubled-segment concerns those fixtures target.
@pytest.mark.parametrize(
    "qualifiers",
    [
        pytest.param([], id="1_segment_no_qualifiers"),
        pytest.param(["soc"], id="2_segments_1_qualifier"),
        pytest.param(["soc", "cpu"], id="3_segments_2_qualifiers"),
        pytest.param(["soc", "sense", "uf2"], id="4_segments_3_qualifiers"),
        pytest.param(["soc", "m33", "w", "mcuboot"], id="5_segments_4_qualifiers"),
    ],
)
def test_find_board_dir_dts_file_and_revision_overlay_at_each_depth(
    tmp_path: Path, qualifiers: list[str]
) -> None:
    board_dir = tmp_path / "boards" / "acme" / "depth_board"
    board_dir.mkdir(parents=True)
    suffix = ("_" + "_".join(qualifiers)) if qualifiers else ""
    (board_dir / f"depth_board{suffix}.dts").write_text("/* base */")
    (board_dir / f"depth_board{suffix}_1_0_0.overlay").write_text("/* rev 1.0.0 */")

    board = "depth_board@1.0.0" + "".join(f"/{q}" for q in qualifiers)

    CORE.data[KEY_ZEPHYR] = _empty_zd()
    found_board_dir = _find_board_dir(tmp_path, board)
    assert found_board_dir == board_dir

    dts_file = _find_dts_file(found_board_dir, board)
    assert dts_file == board_dir / f"depth_board{suffix}.dts"

    overlay = _find_revision_overlay(found_board_dir, board, "1.0.0")
    assert overlay == board_dir / f"depth_board{suffix}_1_0_0.overlay"


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


def test_find_board_yaml_doubled_soc_segment_picks_qualified_variant(
    fake_zephyr_base: Path,
) -> None:
    """Same regression as the .dts case, for the .yaml sibling file."""
    board_dir = fake_zephyr_base / "boards" / "raspberrypi" / "rpi_pico"
    (board_dir / "rpi_pico.yaml").write_text("supported:\n  - uart\n")
    (board_dir / "rpi_pico_rp2040_mcuboot.yaml").write_text("supported:\n  - flash\n")
    result = _find_board_yaml(board_dir, "rpi_pico/rp2040/mcuboot")
    assert result == board_dir / "rpi_pico_rp2040_mcuboot.yaml"


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
# get_i2c_controller_labels / get_spi_controller_labels /
# get_uart_controller_labels / get_board_partitions -- via a fake EDT (real
# edtlib/cpp integration is exercised manually; these functions only touch
# _get_edt()/_iter_nodes(), so a duck-typed fake node is sufficient and much
# cheaper than a full DTS+bindings+cpp fixture).
# ---------------------------------------------------------------------------


def test_get_i2c_controller_labels_returns_enabled_then_disabled(monkeypatch) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    nodes = [
        _FakeNode(labels=["i2c0"], buses=["i2c"], status="disabled"),
        _FakeNode(labels=["i2c1"], buses=["i2c"], status="okay"),
    ]
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: _FakeEdt(nodes))
    assert get_i2c_controller_labels("some_board") == ["i2c1", "i2c0"]


def test_get_spi_controller_labels_returns_enabled_then_disabled(monkeypatch) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    nodes = [
        _FakeNode(labels=["spi0"], buses=["spi"], status="disabled"),
        _FakeNode(labels=["spi1"], buses=["spi"], status="okay"),
    ]
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: _FakeEdt(nodes))
    assert get_spi_controller_labels("some_board") == ["spi1", "spi0"]


def test_get_uart_controller_labels_returns_disabled_when_none_enabled(
    monkeypatch,
) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    nodes = [
        _FakeNode(labels=["uart0"], buses=["uart"], status="disabled"),
        _FakeNode(labels=["uart1"], buses=["uart"], status="disabled"),
    ]
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: _FakeEdt(nodes))
    assert get_uart_controller_labels("some_board") == ["uart0", "uart1"]


def test_get_can_controller_labels_returns_disabled_nodes(monkeypatch) -> None:
    """Every STM32 SoC dtsi ships its CAN node disabled, so a disabled-only board
    must still report it -- zephyr_can is what enables the node."""
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    nodes = [_FakeNode(labels=["fdcan1"], status="disabled")]
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: _FakeEdt(nodes))
    assert get_can_controller_labels("some_board") == ["fdcan1"]


def test_get_can_controller_labels_matches_bxcan_and_fdcan_only(monkeypatch) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    nodes = [
        _FakeNode(labels=["can1"]),
        _FakeNode(labels=["fdcan2"]),
        _FakeNode(labels=["can"]),
        _FakeNode(labels=["scan1"]),
    ]
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: _FakeEdt(nodes))
    assert get_can_controller_labels("some_board") == ["can1", "fdcan2"]


def test_get_spi_controller_labels_returns_none_when_dts_unavailable(
    monkeypatch,
) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd()
    monkeypatch.setattr(dts_lookup, "_get_edt", lambda board: None)
    assert get_spi_controller_labels("some_board") is None


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
    monkeypatch.setattr(dts_lookup, "get_i2c_controller_labels", lambda board: ["i2c0"])
    monkeypatch.setattr(dts_lookup, "get_spi_controller_labels", lambda board: None)
    monkeypatch.setattr(dts_lookup, "get_uart_controller_labels", lambda board: [])
    monkeypatch.setattr(dts_lookup, "get_can_controller_labels", lambda board: ["can1"])
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
    assert "CAN buses present: can1" in message
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
        "get_i2c_controller_labels",
        "get_spi_controller_labels",
        "get_uart_controller_labels",
        "get_can_controller_labels",
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
    monkeypatch.setattr(dts_lookup, "get_i2c_controller_labels", lambda board: None)
    monkeypatch.setattr(dts_lookup, "get_spi_controller_labels", lambda board: None)
    monkeypatch.setattr(dts_lookup, "get_uart_controller_labels", lambda board: None)
    monkeypatch.setattr(dts_lookup, "get_can_controller_labels", lambda board: None)
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


def test_log_board_capabilities_lists_shields(monkeypatch, caplog) -> None:
    from esphome.components.zephyr.variants import ZephyrSDK, ZephyrVariant

    _fake_sdk = ZephyrSDK(manifest_url="https://example.invalid/zephyr")
    variant = ZephyrVariant(sdk=_fake_sdk)
    for name in (
        "get_board_yaml_supported",
        "get_board_features",
        "get_i2c_controller_labels",
        "get_spi_controller_labels",
        "get_uart_controller_labels",
        "get_can_controller_labels",
        "get_board_partitions",
    ):
        monkeypatch.setattr(dts_lookup, name, lambda board: None)

    with caplog.at_level("INFO"):
        log_board_capabilities(
            "my_board", "esp32_c6", variant, "4.4.1", None, ["nrf7002ek"]
        )

    message = caplog.text
    assert "Shields: nrf7002ek" in message
    assert "hardware, as defined by the board itself and its shield(s)" in message


# ---------------------------------------------------------------------------
# _find_shield_dir / _shield_overlay_files / _snippet_overlay_files -- pure
# filesystem lookups, no cpp/edtlib dependency
# ---------------------------------------------------------------------------


def test_find_shield_dir_locates_first_matching_root(tmp_path: Path) -> None:
    root_a = tmp_path / "root_a"
    root_b = tmp_path / "root_b"
    (root_b / "boards" / "shields" / "nrf7002ek").mkdir(parents=True)

    result = _find_shield_dir([root_a, root_b], "nrf7002ek")
    assert result == root_b / "boards" / "shields" / "nrf7002ek"


def test_find_shield_dir_returns_none_when_absent(tmp_path: Path) -> None:
    assert _find_shield_dir([tmp_path], "no_such_shield") is None


def test_shield_overlay_files_includes_base_and_board_specific(
    tmp_path: Path,
) -> None:
    shield_dir = tmp_path / "boards" / "shields" / "nrf7002ek"
    (shield_dir / "boards").mkdir(parents=True)
    (shield_dir / "nrf7002ek.overlay").write_text("/* base */")
    (shield_dir / "boards" / "nrf52840dk.overlay").write_text("/* board */")

    result = _shield_overlay_files(shield_dir, "nrf52840dk")
    assert result == [
        shield_dir / "nrf7002ek.overlay",
        shield_dir / "boards" / "nrf52840dk.overlay",
    ]


def test_shield_overlay_files_board_specific_keeps_full_qualified_segments(
    tmp_path: Path,
) -> None:
    """Regression test: real upstream shields name a board-specific overlay with
    every board-string segment joined by '_' (e.g. nrf7002ek's actual
    boards/nrf5340dk_nrf5340_cpuapp.overlay for board "nrf5340dk/nrf5340/cpuapp"),
    not just the bare board dirname -- must not be truncated to "nrf5340dk.overlay"."""
    shield_dir = tmp_path / "boards" / "shields" / "nrf7002ek"
    (shield_dir / "boards").mkdir(parents=True)
    (shield_dir / "boards" / "nrf5340dk_nrf5340_cpuapp.overlay").write_text(
        "/* board */"
    )

    result = _shield_overlay_files(shield_dir, "nrf5340dk/nrf5340/cpuapp")
    assert result == [shield_dir / "boards" / "nrf5340dk_nrf5340_cpuapp.overlay"]


def test_shield_overlay_files_board_specific_optional(tmp_path: Path) -> None:
    shield_dir = tmp_path / "boards" / "shields" / "nrf7002ek"
    shield_dir.mkdir(parents=True)
    (shield_dir / "nrf7002ek.overlay").write_text("/* base */")

    result = _shield_overlay_files(shield_dir, "some_other_board")
    assert result == [shield_dir / "nrf7002ek.overlay"]


def test_find_snippet_dir_locates_first_matching_root(tmp_path: Path) -> None:
    root_a = tmp_path / "root_a"
    root_b = tmp_path / "root_b"
    (root_b / "snippets" / "slot1-partition").mkdir(parents=True)

    result = _find_snippet_dir([root_a, root_b], "slot1-partition")
    assert result == root_b / "snippets" / "slot1-partition"


def test_find_snippet_dir_returns_none_when_absent(tmp_path: Path) -> None:
    assert _find_snippet_dir([tmp_path], "no_such_snippet") is None


def test_snippet_overlay_files_top_level_append_applies_to_any_board(
    tmp_path: Path,
) -> None:
    """slot1-partition-shaped case: a flat append: with no boards: section applies
    unconditionally."""
    snippet_dir = tmp_path / "snippets" / "slot1-partition"
    snippet_dir.mkdir(parents=True)
    (snippet_dir / "slot1-partition.overlay").write_text("/* snippet */")
    (snippet_dir / "snippet.yml").write_text(
        "name: slot1-partition\n"
        "append:\n"
        "  EXTRA_DTC_OVERLAY_FILE: slot1-partition.overlay\n"
    )

    result = _snippet_overlay_files(snippet_dir, "any_board/soc")
    assert result == [snippet_dir / "slot1-partition.overlay"]


def test_snippet_overlay_files_board_regex_match_espressif_flash_shape(
    tmp_path: Path,
) -> None:
    """espressif-flash-4M-shaped case (cited in zephyr:'s snippets: schema comment):
    no top-level append:, only per-SoC overlays selected by a boards: <regex>:
    match against the full board string."""
    snippet_dir = tmp_path / "snippets" / "espressif-flash-4M"
    soc_dir = snippet_dir / "soc"
    soc_dir.mkdir(parents=True)
    (soc_dir / "flash_0x0_default_4M.overlay").write_text("/* c3 */")
    (soc_dir / "flash_0x1000_amp_4M.overlay").write_text("/* esp32 */")
    (snippet_dir / "snippet.yml").write_text(
        "name: espressif-flash-4M\n"
        "boards:\n"
        "  /.*/esp32/.*/:\n"
        "    append:\n"
        "      EXTRA_DTC_OVERLAY_FILE: soc/flash_0x1000_amp_4M.overlay\n"
        "  /.*/esp32c3/.*/:\n"
        "    append:\n"
        "      EXTRA_DTC_OVERLAY_FILE: soc/flash_0x0_default_4M.overlay\n"
    )

    result_c3 = _snippet_overlay_files(snippet_dir, "esp32c3_devkitm/esp32c3/procpu")
    assert result_c3 == [soc_dir / "flash_0x0_default_4M.overlay"]

    result_none = _snippet_overlay_files(snippet_dir, "rpi_pico/rp2040")
    assert result_none == []


def test_snippet_overlay_files_combines_top_level_and_board_specific(
    tmp_path: Path,
) -> None:
    """nordic-flpr-shaped case: a matching board gets both the generic top-level
    overlay AND its own board-specific one, stacked -- CMake's zephyr_get()
    accumulates rather than replaces."""
    snippet_dir = tmp_path / "snippets" / "nordic-flpr"
    soc_dir = snippet_dir / "soc"
    soc_dir.mkdir(parents=True)
    (snippet_dir / "nordic-flpr.overlay").write_text("/* generic */")
    (soc_dir / "nrf54l15_cpuapp.overlay").write_text("/* l15 */")
    (snippet_dir / "snippet.yml").write_text(
        "name: nordic-flpr\n"
        "append:\n"
        "  EXTRA_DTC_OVERLAY_FILE: nordic-flpr.overlay\n"
        "boards:\n"
        "  /.*/nrf54l15/cpuapp/:\n"
        "    append:\n"
        "      EXTRA_DTC_OVERLAY_FILE: soc/nrf54l15_cpuapp.overlay\n"
    )

    result = _snippet_overlay_files(snippet_dir, "nrf54l15dk/nrf54l15/cpuapp")
    assert result == [
        snippet_dir / "nordic-flpr.overlay",
        soc_dir / "nrf54l15_cpuapp.overlay",
    ]


def test_snippet_overlay_files_returns_empty_when_snippet_yml_absent(
    tmp_path: Path,
) -> None:
    missing_dir = tmp_path / "snippets" / "no_such_snippet"
    assert _snippet_overlay_files(missing_dir, "any_board") == []


def test_snippet_overlay_files_handles_list_valued_overlay_file(
    tmp_path: Path,
) -> None:
    """Regression test: EXTRA_DTC_OVERLAY_FILE is a list-typed CMake variable, so a
    snippet.yml declaring it as a YAML list (not just a single string) must not
    crash -- both entries should be picked up."""
    snippet_dir = tmp_path / "snippets" / "multi-overlay"
    snippet_dir.mkdir(parents=True)
    (snippet_dir / "a.overlay").write_text("/* a */")
    (snippet_dir / "b.overlay").write_text("/* b */")
    (snippet_dir / "snippet.yml").write_text(
        "name: multi-overlay\n"
        "append:\n"
        "  EXTRA_DTC_OVERLAY_FILE:\n"
        "    - a.overlay\n"
        "    - b.overlay\n"
    )

    result = _snippet_overlay_files(snippet_dir, "any_board")
    assert result == [snippet_dir / "a.overlay", snippet_dir / "b.overlay"]


# ---------------------------------------------------------------------------
# _get_edt -- cache key must vary with shields/snippets, and shield/snippet
# overlays must be merged in alongside the base board dts
# ---------------------------------------------------------------------------


def test_get_edt_cache_key_varies_with_shields(monkeypatch, tmp_path: Path) -> None:
    """Same board, different shields: selection must not share a cached EDT --
    otherwise a config change (adding/removing a shield) would silently keep
    validating against the wrong (stale) merged devicetree."""
    (tmp_path / "boards" / "espressif" / "my_board").mkdir(parents=True)
    (tmp_path / "boards" / "espressif" / "my_board" / "my_board.dts").write_text(
        "/ { };"
    )

    def _fake_preprocess_dts(dts_file, zephyr_base, board_dir):
        return "/* base */"

    def _fake_edt_ctor(path, bindings_dirs, **kwargs):
        return object()  # a fresh, distinct object each call

    monkeypatch.setattr(dts_lookup, "_preprocess_dts", _fake_preprocess_dts)
    monkeypatch.setattr(
        dts_lookup,
        "_load_edtlib",
        lambda base: type("_FakeEdtlib", (), {"EDT": staticmethod(_fake_edt_ctor)}),
    )

    zd = _empty_zd(dts_base_path=str(tmp_path), shields=[])
    CORE.data[KEY_ZEPHYR] = zd
    edt_no_shield = _get_edt("my_board")

    zd["shields"] = ["nrf7002ek"]
    edt_with_shield = _get_edt("my_board")

    assert edt_no_shield is not edt_with_shield
    cache = zd["board_edt_cache"]
    assert ("my_board", (), ()) in cache
    assert ("my_board", ("nrf7002ek",), ()) in cache


def test_get_edt_merges_shield_overlay_text(monkeypatch, tmp_path: Path) -> None:
    (tmp_path / "boards" / "espressif" / "my_board").mkdir(parents=True)
    (tmp_path / "boards" / "espressif" / "my_board" / "my_board.dts").write_text(
        "/* base */"
    )
    shield_dir = tmp_path / "boards" / "shields" / "nrf7002ek"
    shield_dir.mkdir(parents=True)
    (shield_dir / "nrf7002ek.overlay").write_text('&spi0 { status = "okay"; };')

    def _fake_preprocess_dts_file(src_file, zephyr_base, extra_include_dirs):
        return src_file.read_text()

    seen_texts: list[str] = []

    def _fake_edt_ctor(path, bindings_dirs, **kwargs):
        seen_texts.append(Path(path).read_text(encoding="utf-8"))
        return object()

    monkeypatch.setattr(dts_lookup, "_preprocess_dts_file", _fake_preprocess_dts_file)
    monkeypatch.setattr(
        dts_lookup,
        "_load_edtlib",
        lambda base: type("_FakeEdtlib", (), {"EDT": staticmethod(_fake_edt_ctor)}),
    )

    CORE.data[KEY_ZEPHYR] = _empty_zd(
        dts_base_path=str(tmp_path), shields=["nrf7002ek"]
    )
    _get_edt("my_board")

    assert len(seen_texts) == 1
    assert "/* base */" in seen_texts[0]
    assert '&spi0 { status = "okay"; };' in seen_texts[0]


# ---------------------------------------------------------------------------
# _find_revision_overlay / get_board_edt revision merge / validate_board_revision
# ---------------------------------------------------------------------------


def _revisioned_board_dir(tmp_path: Path) -> Path:
    """boards/actinius/icarus-shaped fixture: a board.yml declaring revisions
    1.4.0/2.0.0 (major.minor.patch, closest-lower matching), a base .dts, and one
    overlay per declared revision -- mirrors the real upstream layout exactly."""
    board_dir = tmp_path / "boards" / "actinius" / "icarus"
    board_dir.mkdir(parents=True)
    (board_dir / "board.yml").write_text(
        "board:\n"
        "  name: actinius_icarus\n"
        "  vendor: actinius\n"
        "  revision:\n"
        "    format: major.minor.patch\n"
        "    default: '2.0.0'\n"
        "    revisions:\n"
        "      - name: '1.4.0'\n"
        "      - name: '2.0.0'\n"
    )
    (board_dir / "actinius_icarus_nrf9160_ns.dts").write_text("/* base */")
    (board_dir / "actinius_icarus_nrf9160_ns_1_4_0.overlay").write_text(
        "/* rev 1.4.0 */"
    )
    (board_dir / "actinius_icarus_nrf9160_ns_2_0_0.overlay").write_text(
        "/* rev 2.0.0 */"
    )
    return board_dir


def test_find_revision_overlay_exact_match(tmp_path: Path) -> None:
    board_dir = _revisioned_board_dir(tmp_path)
    result = _find_revision_overlay(board_dir, "actinius_icarus/nrf9160/ns", "2.0.0")
    assert result == board_dir / "actinius_icarus_nrf9160_ns_2_0_0.overlay"


def test_find_revision_overlay_returns_none_when_absent(tmp_path: Path) -> None:
    board_dir = _revisioned_board_dir(tmp_path)
    result = _find_revision_overlay(board_dir, "actinius_icarus/nrf9160/ns", "9.9.9")
    assert result is None


def test_find_revision_overlay_no_qualifiers(tmp_path: Path) -> None:
    """A board with no "/" qualifiers still finds its revision overlay -- the
    revision is the only trailing segment in that case."""
    board_dir = tmp_path / "boards" / "acme" / "my_board"
    board_dir.mkdir(parents=True)
    (board_dir / "my_board_1_0_0.overlay").write_text("/* rev */")
    result = _find_revision_overlay(board_dir, "my_board", "1.0.0")
    assert result == board_dir / "my_board_1_0_0.overlay"


def test_get_edt_merges_revision_overlay_text(monkeypatch, tmp_path: Path) -> None:
    _revisioned_board_dir(tmp_path)

    def _fake_preprocess_dts(dts_file, zephyr_base, board_dir_arg):
        return dts_file.read_text()

    def _fake_preprocess_dts_file(src_file, zephyr_base, extra_include_dirs):
        return src_file.read_text()

    seen_texts: list[str] = []

    def _fake_edt_ctor(path, bindings_dirs, **kwargs):
        seen_texts.append(Path(path).read_text(encoding="utf-8"))
        return object()

    monkeypatch.setattr(dts_lookup, "_preprocess_dts", _fake_preprocess_dts)
    monkeypatch.setattr(dts_lookup, "_preprocess_dts_file", _fake_preprocess_dts_file)
    monkeypatch.setattr(
        dts_lookup,
        "_load_edtlib",
        lambda base: type("_FakeEdtlib", (), {"EDT": staticmethod(_fake_edt_ctor)}),
    )

    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    # Requesting 1.9.0 must resolve to the closest lower declared revision, 1.4.0,
    # and merge in *that* revision's overlay -- not 2.0.0's.
    _get_edt("actinius_icarus@1.9.0/nrf9160/ns")

    assert len(seen_texts) == 1
    assert "/* base */" in seen_texts[0]
    assert "/* rev 1.4.0 */" in seen_texts[0]
    assert "/* rev 2.0.0 */" not in seen_texts[0]


def test_get_edt_ignores_revision_when_board_has_none(
    monkeypatch, tmp_path: Path
) -> None:
    (tmp_path / "boards" / "espressif" / "my_board").mkdir(parents=True)
    (tmp_path / "boards" / "espressif" / "my_board" / "my_board.dts").write_text(
        "/* base */"
    )

    def _fake_preprocess_dts(dts_file, zephyr_base, board_dir_arg):
        return "/* base */"

    seen_texts: list[str] = []

    def _fake_edt_ctor(path, bindings_dirs, **kwargs):
        seen_texts.append(Path(path).read_text(encoding="utf-8"))
        return object()

    monkeypatch.setattr(dts_lookup, "_preprocess_dts", _fake_preprocess_dts)
    monkeypatch.setattr(
        dts_lookup,
        "_load_edtlib",
        lambda base: type("_FakeEdtlib", (), {"EDT": staticmethod(_fake_edt_ctor)}),
    )

    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    # "@1.0.0" on a board that declares no revisions at all -- silently ignored,
    # same as Zephyr's own build would do for a stray, meaningless revision.
    _get_edt("my_board@1.0.0")

    assert seen_texts == ["/* base */"]


def test_validate_board_revision_returns_none_without_revision_suffix(
    tmp_path: Path,
) -> None:
    _revisioned_board_dir(tmp_path)
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    assert validate_board_revision("actinius_icarus/nrf9160/ns") is None


def test_validate_board_revision_returns_none_without_dts_base_path() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=None)
    assert validate_board_revision("actinius_icarus@1.4.0/nrf9160/ns") is None


def test_validate_board_revision_true_for_resolvable_revision(tmp_path: Path) -> None:
    _revisioned_board_dir(tmp_path)
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    assert validate_board_revision("actinius_icarus@1.9.0/nrf9160/ns") is True


def test_validate_board_revision_false_for_unresolvable_revision(
    tmp_path: Path,
) -> None:
    _revisioned_board_dir(tmp_path)
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    assert validate_board_revision("actinius_icarus@0.1.0/nrf9160/ns") is False


def test_validate_board_revision_returns_none_for_board_without_revisions(
    tmp_path: Path,
) -> None:
    (tmp_path / "boards" / "espressif" / "my_board").mkdir(parents=True)
    (tmp_path / "boards" / "espressif" / "my_board" / "my_board.dts").write_text(
        "/* base */"
    )
    CORE.data[KEY_ZEPHYR] = _empty_zd(dts_base_path=str(tmp_path))
    assert validate_board_revision("my_board@1.0.0") is None
