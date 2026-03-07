"""Tests for esphome.analyze_memory.disassembly normalization."""

import pytest

from esphome.analyze_memory.disassembly import (
    _find_balanced_ref,
    _normalize_all_refs,
    _normalize_function_asm,
)


@pytest.mark.parametrize(
    ("text", "start", "expected"),
    [
        ("<foo>", 0, ("foo", 5)),
        ("<foo+0x20>", 0, ("foo+0x20", 10)),
        ("<std::vector<unsigned char> >", 0, ("std::vector<unsigned char> ", 29)),
        (
            "<std::vector<unsigned char, std::allocator<unsigned char> >"
            "::resize(unsigned int)+0x20>",
            0,
            (
                "std::vector<unsigned char, std::allocator<unsigned char> >"
                "::resize(unsigned int)+0x20",
                87,
            ),
        ),
        ("call0     <some_func>", 10, ("some_func", 21)),
    ],
)
def test_find_balanced_ref(text: str, start: int, expected: tuple[str, int]) -> None:
    assert _find_balanced_ref(text, start) == expected


@pytest.mark.parametrize(
    ("text", "start"),
    [
        ("<unclosed", 0),
        ("abc", 5),
        ("abc", 0),
    ],
    ids=["unbalanced", "out_of_bounds", "not_angle_bracket"],
)
def test_find_balanced_ref_returns_none(text: str, start: int) -> None:
    assert _find_balanced_ref(text, start) is None


@pytest.mark.parametrize(
    ("insn", "func_name", "is_literal", "expected"),
    [
        (
            "call0     <other_func>",
            "my_func",
            False,
            "call0     <other_func>",
        ),
        (
            "call0     <other_func+0x20>",
            "my_func",
            False,
            "call0     <other_func>",
        ),
        (
            "j     <my_func+0x40>",
            "my_func",
            False,
            "j     <self>",
        ),
        (
            "j     <my_func>",
            "my_func",
            False,
            "j     <self>",
        ),
        (
            "l32r    a5,  <some_symbol+0x10>",
            "my_func",
            True,
            "l32r    a5,  <.literal>",
        ),
        (
            "j     <_lit4_end+0x1234>",
            "my_func",
            False,
            "j     <.literal>",
        ),
        (
            "mov.n    a2, a3",
            "my_func",
            False,
            "mov.n    a2, a3",
        ),
    ],
    ids=[
        "cross_ref",
        "cross_ref_strips_offset",
        "self_ref",
        "self_ref_no_offset",
        "l32r_literal",
        "literal_pool_symbol",
        "no_refs",
    ],
)
def test_normalize_all_refs(
    insn: str, func_name: str, is_literal: bool, expected: str
) -> None:
    assert _normalize_all_refs(insn, func_name, is_literal) == expected


def test_normalize_all_refs_l32r_template_symbol() -> None:
    """The bug: l32r with C++ template symbol was leaving trailing text."""
    insn = (
        "l32r    a5,  "
        "<std::vector<unsigned char, std::allocator<unsigned char> >"
        "::resize(unsigned int)+0x20>"
    )
    assert _normalize_all_refs(insn, "my_func", True) == "l32r    a5,  <.literal>"


def test_normalize_all_refs_template_cross_ref() -> None:
    insn = (
        "call0     "
        "<std::vector<unsigned char, std::allocator<unsigned char> >"
        "::reserve(unsigned int)>"
    )
    assert _normalize_all_refs(insn, "my_func", False) == insn


def test_normalize_all_refs_template_cross_ref_strips_offset() -> None:
    insn = (
        "call0     "
        "<std::vector<unsigned char, std::allocator<unsigned char> >"
        "::reserve(unsigned int)+0x20>"
    )
    expected = (
        "call0     "
        "<std::vector<unsigned char, std::allocator<unsigned char> >"
        "::reserve(unsigned int)>"
    )
    assert _normalize_all_refs(insn, "my_func", False) == expected


@pytest.mark.parametrize(
    "symbol",
    ["_lit4_end", "_iram_end", "_data_end", "_bss_end", "_heap_end"],
)
def test_normalize_all_refs_linker_symbols(symbol: str) -> None:
    insn = f"j     <{symbol}+0x10>"
    assert _normalize_all_refs(insn, "my_func", False) == "j     <.literal>"


def test_normalize_function_asm_relative_offsets() -> None:
    lines = [
        "  40001000:\tmov.n    a2, a3",
        "  40001002:\tret.n",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "+0x0000: mov.n    a2, a3\n+0x0002: ret.n"


def test_normalize_function_asm_skips_non_instruction_lines() -> None:
    lines = [
        "",
        "  40001000:\tmov.n    a2, a3",
        "   ...",
        "  40001002:\tret.n",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "+0x0000: mov.n    a2, a3\n+0x0002: ret.n"


def test_normalize_function_asm_strips_absolute_addr_before_ref() -> None:
    lines = [
        "  40001000:\tcall0     40002000 <other_func>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "+0x0000: call0      <other_func>"


def test_normalize_function_asm_self_branch() -> None:
    lines = [
        "  40001000:\tj     40001010 <test_func+0x10>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "+0x0000: j      <self>"


def test_normalize_function_asm_l32r_literal() -> None:
    lines = [
        "  40001000:\tl32r    a5, 40000ffc <_lit4_end+0x1234>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "+0x0000: l32r    a5,  <.literal>"


def test_normalize_function_asm_template_in_call() -> None:
    lines = [
        "  40001000:\tcall0     40002000"
        " <std::vector<unsigned char, std::allocator<unsigned char> >"
        "::reserve(unsigned int)>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    expected = (
        "+0x0000: call0      "
        "<std::vector<unsigned char, std::allocator<unsigned char> >"
        "::reserve(unsigned int)>"
    )
    assert result == expected


def test_normalize_function_asm_template_in_l32r() -> None:
    """The original bug: l32r with template symbol left trailing garbage."""
    lines = [
        "  40001000:\tl32r    a5, 40000ffc"
        " <std::vector<unsigned char, std::allocator<unsigned char> >"
        "::resize(unsigned int)+0x20>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "+0x0000: l32r    a5,  <.literal>"


def test_normalize_function_asm_empty_input() -> None:
    assert _normalize_function_asm([], 0x40001000, "test_func") == ""


def test_normalize_function_asm_func_size_excludes_padding() -> None:
    """Instructions past the known function size are excluded.

    objdump disassembles padding/data between functions as code,
    producing spurious references to unrelated symbols.
    """
    lines = [
        "  40001000:\tmov.n    a2, a3",
        "  40001002:\tret.n",
        # Padding bytes past function end, misinterpreted as instructions
        "  40001004:\tj     40003000 <esphome::wifi::WiFiComponent::loop()>",
        "  40001007:\tcall8     40004000 <chip_v6_set_chan>",
    ]
    # Function is 4 bytes (0x000-0x003), so instructions at +0x0004 onward are padding
    result = _normalize_function_asm(lines, 0x40001000, "test_func", func_size=4)
    assert result == "+0x0000: mov.n    a2, a3\n+0x0002: ret.n"


def test_normalize_function_asm_func_size_zero_includes_all() -> None:
    """When func_size is 0, all instructions are included (no limit)."""
    lines = [
        "  40001000:\tmov.n    a2, a3",
        "  40001002:\tret.n",
        "  40001004:\tj     40003000 <other_func>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func", func_size=0)
    assert "+0x0004:" in result
