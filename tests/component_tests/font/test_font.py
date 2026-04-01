"""Tests for the font component.

Focuses on verifying that long multi-byte (Chinese/CJK) glyph strings
are correctly processed through the font configuration pipeline.
"""

import functools
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.components.font import (
    CONF_BPP,
    CONF_EXTRAS,
    CONF_GLYPHSETS,
    CONF_IGNORE_MISSING_GLYPHS,
    CONF_RAW_GLYPH_ID,
    FONT_CACHE,
    flatten,
    glyph_comparator,
    to_code,
    validate_font_config,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_GLYPHS,
    CONF_ID,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_SIZE,
    CONF_TYPE,
)

FONT_DIR = Path(__file__).parent
FONT_PATH = FONT_DIR / "NotoSans-Regular.ttf"

# 200 unique CJK Unified Ideograph characters (U+4E00..U+4EC7)
CHINESE_200 = "".join(chr(cp) for cp in range(0x4E00, 0x4EC8))


def _file_conf() -> dict:
    return {CONF_PATH: str(FONT_PATH), CONF_TYPE: "local"}


def _make_config(
    glyphs: list[str],
    *,
    ignore_missing: bool = False,
    size: int = 20,
    bpp: int = 1,
    extras: list | None = None,
    glyphsets: list | None = None,
) -> dict:
    """Build a config dict matching what FONT_SCHEMA produces."""
    return {
        CONF_FILE: _file_conf(),
        CONF_GLYPHS: glyphs,
        CONF_GLYPHSETS: glyphsets or [],
        CONF_IGNORE_MISSING_GLYPHS: ignore_missing,
        CONF_SIZE: size,
        CONF_BPP: bpp,
        CONF_EXTRAS: extras or [],
    }


@pytest.fixture(autouse=True)
def _load_font():
    """Load the test font into FONT_CACHE and clean up afterwards."""
    fc = _file_conf()
    FONT_CACHE[fc] = FONT_PATH
    yield
    FONT_CACHE.store.clear()


# ---------- flatten / glyph_comparator helpers ----------


def test_flatten_splits_chinese_string_into_chars():
    """A single string of 200 Chinese characters must become 200 individual chars."""
    result = flatten([CHINESE_200])
    assert len(result) == 200
    assert all(len(c) == 1 for c in result)
    assert result[0] == "\u4e00"
    assert result[-1] == "\u4ec7"


def test_flatten_multiple_chinese_strings():
    """Multiple glyph strings are concatenated then split correctly."""
    s1 = CHINESE_200[:100]
    s2 = CHINESE_200[100:]
    result = flatten([list(s1), list(s2)])
    assert len(result) == 200


def test_glyph_comparator_orders_chinese_by_utf8():
    """glyph_comparator must order CJK characters by their UTF-8 byte sequence."""
    chars = list(CHINESE_200[:10])
    sorted_chars = sorted(chars, key=functools.cmp_to_key(glyph_comparator))
    # CJK block is contiguous and UTF-8 order matches codepoint order here
    assert sorted_chars == chars


def test_glyph_comparator_mixed_ascii_and_chinese():
    """ASCII characters sort before CJK characters (lower UTF-8 bytes)."""
    assert glyph_comparator("A", "\u4e00") == -1
    assert glyph_comparator("\u4e00", "A") == 1
    assert glyph_comparator("\u4e00", "\u4e00") == 0


# ---------- validate_font_config ----------


def test_long_chinese_glyphs_raises_missing_error():
    """200 Chinese chars not present in NotoSans must raise Invalid with the correct count."""
    config = _make_config([CHINESE_200])
    with pytest.raises(cv.Invalid, match=r"missing 200 glyphs"):
        validate_font_config(config)


def test_long_chinese_glyphs_error_mentions_overflow():
    """When more than 10 glyphs are missing the error should mention the remainder."""
    config = _make_config([CHINESE_200])
    with pytest.raises(cv.Invalid, match=r"and 190 more"):
        validate_font_config(config)


def test_duplicate_chinese_glyphs_detected():
    """Duplicate CJK characters within a single glyph string must be caught."""
    duped = "\u4e00\u4e01\u4e00"  # first char repeated
    config = _make_config([duped])
    with pytest.raises(cv.Invalid, match="duplicate"):
        validate_font_config(config)


def test_duplicate_chinese_across_strings():
    """Duplicates across separate glyph strings are also caught."""
    config = _make_config(["\u4e00\u4e01", "\u4e01\u4e02"])
    with pytest.raises(cv.Invalid, match="duplicate"):
        validate_font_config(config)


def test_no_false_duplicates_in_200_unique_chinese():
    """200 unique CJK characters must not trigger the duplicate check."""
    config = _make_config([CHINESE_200])
    # Should not raise duplicate error — it should reach the missing-glyph check instead
    with pytest.raises(cv.Invalid, match="missing"):
        validate_font_config(config)


def test_valid_latin_glyphs_pass_validation():
    """Latin characters present in NotoSans-Regular pass validation without error."""
    config = _make_config(["ABCabc123"])
    result = validate_font_config(config)
    assert result is not None
    assert result[CONF_SIZE] == 20


def test_long_latin_glyphs_pass_validation():
    """A long string of supported Latin glyphs passes validation."""
    # 95 printable ASCII characters that NotoSans supports
    latin = "".join(chr(cp) for cp in range(0x21, 0x7F))
    config = _make_config([latin])
    result = validate_font_config(config)
    assert result is not None


def test_mixed_latin_and_chinese_glyphs_error():
    """Mixing valid Latin and invalid Chinese chars reports missing Chinese glyphs."""
    chinese_10 = CHINESE_200[:10]
    config = _make_config(["ABC", chinese_10])
    with pytest.raises(cv.Invalid, match=r"missing 10 glyphs"):
        validate_font_config(config)


def test_single_chinese_char_glyph():
    """A single Chinese character is correctly handled as one glyph."""
    config = _make_config(["\u4e00"])
    with pytest.raises(cv.Invalid, match=r"missing 1 glyph[^s]"):
        validate_font_config(config)


def test_chinese_glyphs_as_individual_list_items():
    """Chinese chars provided as separate list items are handled the same as a single string."""
    chars_as_list = list(CHINESE_200[:50])
    config = _make_config(chars_as_list)
    with pytest.raises(cv.Invalid, match=r"missing 50 glyphs"):
        validate_font_config(config)


# ---------- YAML parsing ----------


def test_yaml_long_latin_glyphs_parsed_and_validated(tmp_path):
    """200 Latin Extended chars on a single YAML line are parsed intact and pass validation."""
    from esphome.yaml_util import load_yaml

    latin_long = "".join(chr(cp) for cp in range(0x100, 0x1C8))
    yaml_file = tmp_path / "font_test.yaml"
    yaml_file.write_text(
        f'font:\n  - file: "NotoSans-Regular.ttf"\n    glyphs: "{latin_long}"\n',
        encoding="utf-8",
    )

    parsed = load_yaml(yaml_file)
    raw_glyphs = parsed["font"][0]["glyphs"]

    # YAML must preserve every Unicode character on the single line
    assert raw_glyphs == latin_long
    assert len(raw_glyphs) == 200

    # Feed through validate_font_config to confirm all glyphs are accepted
    config = _make_config([raw_glyphs])
    result = validate_font_config(config)
    assert result is not None


@pytest.mark.parametrize(
    "glyphs_str",
    [
        " ABC",  # space at start
        "AB CD",  # space in middle
        "ABC ",  # space at end
    ],
    ids=["start", "middle", "end"],
)
def test_yaml_space_in_glyphs_preserved(tmp_path, glyphs_str):
    """A space character in a glyphs string must survive YAML round-trip and validation."""
    from esphome.yaml_util import load_yaml

    yaml_file = tmp_path / "font_test.yaml"
    yaml_file.write_text(
        f'font:\n  - file: "NotoSans-Regular.ttf"\n    glyphs: "{glyphs_str}"\n',
        encoding="utf-8",
    )

    parsed = load_yaml(yaml_file)
    raw_glyphs = parsed["font"][0]["glyphs"]

    assert raw_glyphs == glyphs_str
    assert " " in raw_glyphs

    # Space and ASCII letters are all in NotoSans — validation must pass
    config = _make_config([raw_glyphs])
    result = validate_font_config(config)
    assert result is not None


# ---------- to_code generation ----------


# 200 unique Latin Extended characters (U+0100..U+01C7), all present in NotoSans
LATIN_LONG = "".join(chr(cp) for cp in range(0x100, 0x1C8))


@pytest.fixture
def mock_cg():
    """Mock all cg codegen functions used by to_code."""
    with (
        patch("esphome.components.font.cg.add_define") as mock_define,
        patch("esphome.components.font.cg.progmem_array") as mock_progmem,
        patch("esphome.components.font.cg.static_const_array") as mock_static,
        patch("esphome.components.font.cg.new_Pvariable") as mock_new_pvar,
    ):
        mock_progmem.return_value = MagicMock()
        mock_static.return_value = MagicMock()
        yield {
            "add_define": mock_define,
            "progmem_array": mock_progmem,
            "static_const_array": mock_static,
            "new_Pvariable": mock_new_pvar,
        }


@pytest.mark.asyncio
async def test_to_code_long_latin_generates_all_glyphs(mock_cg):
    """to_code must generate glyph data for every character in a long Latin string."""
    glyph_count = len(LATIN_LONG)  # 200
    config = _make_config([LATIN_LONG])
    config[CONF_ID] = MagicMock()
    config[CONF_RAW_DATA_ID] = MagicMock()
    config[CONF_RAW_GLYPH_ID] = MagicMock()

    await to_code(config)

    # USE_FONT define must be emitted
    mock_cg["add_define"].assert_any_call("USE_FONT")

    # progmem_array receives the combined bitmap data (non-empty)
    mock_cg["progmem_array"].assert_called_once()
    bitmap_data = mock_cg["progmem_array"].call_args.args[1]
    assert len(bitmap_data) > 0

    # static_const_array receives one entry per unique glyph
    mock_cg["static_const_array"].assert_called_once()
    glyph_initializer = mock_cg["static_const_array"].call_args.args[1]
    assert len(glyph_initializer) == glyph_count

    # new_Pvariable is called with the correct glyph count
    mock_cg["new_Pvariable"].assert_called_once()
    pvar_args = mock_cg["new_Pvariable"].call_args.args
    assert pvar_args[2] == glyph_count  # len(glyph_initializer)
    assert pvar_args[8] == 1  # bpp


@pytest.mark.asyncio
async def test_to_code_glyph_entries_contain_expected_fields(mock_cg):
    """Each glyph initializer entry must have 7 fields: codepoint, data ptr, advance, offset_x, offset_y, w, h."""
    config = _make_config([LATIN_LONG])
    config[CONF_ID] = MagicMock()
    config[CONF_RAW_DATA_ID] = MagicMock()
    config[CONF_RAW_GLYPH_ID] = MagicMock()

    await to_code(config)

    glyph_initializer = mock_cg["static_const_array"].call_args.args[1]
    for entry in glyph_initializer:
        assert len(entry) == 7, f"Glyph entry should have 7 fields, got {len(entry)}"
        codepoint = entry[0]
        assert isinstance(codepoint, int)
        assert 0x100 <= codepoint <= 0x1C7


@pytest.mark.asyncio
async def test_to_code_glyphs_sorted_by_utf8(mock_cg):
    """Glyphs in the initializer must be sorted by UTF-8 byte order."""
    config = _make_config([LATIN_LONG])
    config[CONF_ID] = MagicMock()
    config[CONF_RAW_DATA_ID] = MagicMock()
    config[CONF_RAW_GLYPH_ID] = MagicMock()

    await to_code(config)

    glyph_initializer = mock_cg["static_const_array"].call_args.args[1]
    codepoints = [entry[0] for entry in glyph_initializer]
    assert codepoints == sorted(codepoints)
