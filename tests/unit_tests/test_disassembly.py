"""Tests for esphome.analyze_memory.disassembly normalization."""

import pytest

from esphome.analyze_memory.disassembly import (
    _find_balanced_ref,
    _is_safe_path,
    _normalize_all_refs,
    _normalize_encoding,
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


def test_normalize_function_asm_instructions() -> None:
    lines = [
        "  40001000:\tmov.n    a2, a3",
        "  40001002:\tret.n",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "mov    a2, a3\nret"


def test_normalize_function_asm_skips_non_instruction_lines() -> None:
    lines = [
        "",
        "  40001000:\tmov.n    a2, a3",
        "   ...",
        "  40001002:\tret.n",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "mov    a2, a3\nret"


def test_normalize_function_asm_strips_absolute_addr_before_ref() -> None:
    lines = [
        "  40001000:\tcall0     40002000 <other_func>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "call0      <other_func>"


def test_normalize_function_asm_self_branch() -> None:
    lines = [
        "  40001000:\tj     40001010 <test_func+0x10>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "j      <self>"


def test_normalize_function_asm_l32r_literal() -> None:
    lines = [
        "  40001000:\tl32r    a5, 40000ffc <_lit4_end+0x1234>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "l32r    a5,  <.literal>"


def test_normalize_function_asm_l32r_with_data_ref() -> None:
    """l32r with parenthesized data reference should be trimmed."""
    lines = [
        "  40001000:\tl32r    a11, 400d0080 <_stext+0x60>"
        " (3f400194 <_flash_rodata_start+0x74>)",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "l32r    a11,  <.literal>"


def test_normalize_function_asm_template_in_call() -> None:
    lines = [
        "  40001000:\tcall0     40002000"
        " <std::vector<unsigned char, std::allocator<unsigned char> >"
        "::reserve(unsigned int)>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    expected = (
        "call0      "
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
    assert result == "l32r    a5,  <.literal>"


def test_normalize_function_asm_empty_input() -> None:
    assert _normalize_function_asm([], 0x40001000, "test_func") == ""


def test_normalize_function_asm_source_annotations() -> None:
    """Source line annotations from objdump -l are included as comments.

    When source files aren't readable, just filename:line is shown.
    """
    lines = [
        "/nonexistent/path/file.cpp:42",
        "  40001000:\tmov.n    a2, a3",
        "/nonexistent/path/file.cpp:43",
        "  40001002:\tret.n",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "# file.cpp:42\nmov    a2, a3\n# file.cpp:43\nret"


def test_normalize_function_asm_source_annotations_deduplicated() -> None:
    """Consecutive references to the same source line are deduplicated."""
    lines = [
        "/nonexistent/path/file.cpp:42",
        "  40001000:\tmov.n    a2, a3",
        "/nonexistent/path/file.cpp:42",
        "  40001002:\tret.n",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "# file.cpp:42\nmov    a2, a3\nret"


def test_normalize_function_asm_source_annotations_with_discriminator() -> None:
    """Source annotations with discriminator suffix are parsed correctly."""
    lines = [
        "/nonexistent/path/file.cpp:42 (discriminator 3)",
        "  40001000:\tmov.n    a2, a3",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert result == "# file.cpp:42\nmov    a2, a3"


def test_normalize_function_asm_max_insns() -> None:
    """max_insns limits instruction count but not source annotations."""
    lines = [
        "/path/to/file.cpp:1",
        "  40001000:\tmov.n    a2, a3",
        "/path/to/file.cpp:2",
        "  40001002:\tmov.n    a4, a5",
        "/path/to/file.cpp:3",
        "  40001004:\tret.n",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func", max_insns=2)
    assert "mov    a2, a3" in result
    assert "mov    a4, a5" in result
    assert "ret" not in result


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
    assert result == "mov    a2, a3\nret"


def test_normalize_function_asm_func_size_zero_includes_all() -> None:
    """When func_size is 0, all instructions are included (no limit).

    Note: dead-code detection still applies — code after ret.n is skipped
    unless it's a branch target.
    """
    lines = [
        "  40001000:\tmov.n    a2, a3",
        "  40001002:\tbeqz    a2, 40001006 <test_func+0x6>",
        "  40001004:\tret.n",
        # Branch target at +0x6 — reachable despite being after ret.n
        "  40001006:\tj     40003000 <other_func>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func", func_size=0)
    assert "j      <other_func>" in result


def test_normalize_function_asm_skips_dead_code_after_unconditional_jump() -> None:
    """Data after unconditional jumps (switch tables, padding) is skipped.

    objdump's linear sweep decodes these data words as instructions,
    producing spurious references to unrelated symbols.
    """
    lines = [
        "  40001000:\tmov.n    a2, a3",
        "  40001003:\tjx    a9",
        # Switch table data decoded as instructions — unreachable
        "  40001006:\ts32i.n    a15, a2, 52",
        "  40001008:\tl16ui    a2, a4, 80",
        "  4000100b:\tj     40003000 <esphome::wifi::WiFiComponent::loop()>",
        # Real code at a branch target
        "  4000100e:\tmovi.n    a14, 5",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert "WiFi" not in result
    assert "s32i" not in result
    assert "mov    a2, a3" in result
    assert "jx    a9" in result
    # +0x0e is not a branch target, so it's also skipped
    assert "movi" not in result


def test_normalize_function_asm_dead_code_resumes_at_branch_target() -> None:
    """Code resumes after dead region when a branch target is reached."""
    lines = [
        "  40001000:\tbeqz    a2, 40001010 <test_func+0x10>",
        "  40001003:\tmov.n    a2, a3",
        "  40001005:\tretw.n",
        # Dead code (not a branch target)
        "  40001007:\tj     40003000 <esphome::wifi::WiFiComponent::loop()>",
        # Branch target from beqz at +0x00 — code is live again
        "  40001010:\tmovi.n    a5, 1",
        "  40001012:\tretw.n",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert "WiFi" not in result
    assert "movi    a5, 1" in result
    assert "retw" in result


def test_normalize_function_asm_dead_code_after_ret() -> None:
    """Data after ret.n is skipped unless it's a branch target."""
    lines = [
        "  40001000:\tmov.n    a2, a3",
        "  40001002:\tret.n",
        # Padding/data after return
        "  40001004:\tcall4     40005000 <esp8266::MDNSResponder::foo()>",
        "  40001007:\tl32i    a13, a0, 136",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert "MDNSResponder" not in result
    assert result == "mov    a2, a3\nret"


def test_normalize_function_asm_dead_code_arm_thumb() -> None:
    """Dead code detection works for ARM Thumb (bx lr, pop {pc})."""
    lines = [
        "  00001000:\tmov    r0, r1",
        "  00001002:\tbx    lr",
        # Dead code after return
        "  00001004:\tbl    00002000 <unrelated_func>",
        # Branch target from elsewhere
        "  00001008:\tmov    r2, r3",
    ]
    # Add a branch to +0x08 so it's a known target
    lines.insert(0, "  00000ffe:\tbeq.n    00001008 <test_func+0x0a>")
    result = _normalize_function_asm(lines, 0x00000FFE, "test_func")
    assert "unrelated_func" not in result
    assert "mov    r2, r3" in result


@pytest.mark.parametrize(
    "jump_insn",
    [
        "j     40001020 <test_func+0x20>",
        "jx    a9",
        "ret.n",
        "retw.n",
        "ret",
        "retw",
    ],
    ids=["j", "jx", "ret.n", "retw.n", "ret", "retw"],
)
def test_normalize_function_asm_unconditional_starts_dead_region(
    jump_insn: str,
) -> None:
    """Various unconditional flow instructions start dead-code regions."""
    lines = [
        "  40001000:\tmov.n    a2, a3",
        f"  40001003:\t{jump_insn}",
        "  40001006:\tj     40003000 <esphome::wifi::WiFiComponent::loop()>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert "WiFi" not in result


@pytest.mark.parametrize(
    "insn",
    [
        "excw",
        "ill",
        ".byte\t0x4f",
        ".short\t0x1234",
        ".word\t0x12345678",
        "{ excw; excw }",
        "orb\tb0, b0, b0",
        "orbc\tb0, b0, b10",
    ],
    ids=[
        "excw",
        "ill",
        "byte",
        "short",
        "word",
        "flix_excw",
        "orb_b0",
        "orbc_b0",
    ],
)
def test_normalize_function_asm_skips_data_as_code(insn: str) -> None:
    """Data misinterpreted as code is filtered out."""
    lines = [
        "  40001000:\tmov.n    a2, a3",
        f"  40001002:\t{insn}",
        "  40001005:\tret.n",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert insn not in result
    assert "mov" in result
    assert result.endswith("ret")


# --- Tests for _normalize_encoding ---


@pytest.mark.parametrize(
    ("insn", "expected"),
    [
        ("mov.n    a2, a3", "mov    a2, a3"),
        ("s32i.n    a10, a1, 0", "s32i    a10, a1, 0"),
        ("ret.n", "ret"),
        ("retw.n", "retw"),
        ("movi.n    a5, 1", "movi    a5, 1"),
        ("add.n    a2, a3, a4", "add    a2, a3, a4"),
        ("l32i.n    a2, a1, 0", "l32i    a2, a1, 0"),
        ("beqz.n    a2, 40001010 <foo>", "beqz    a2, 40001010 <foo>"),
        # ARM Thumb wide/narrow
        ("b.w    00001000 <foo>", "b    00001000 <foo>"),
        ("b.n    00001000 <foo>", "b    00001000 <foo>"),
        # No suffix — unchanged
        ("call0     40002000 <foo>", "call0     40002000 <foo>"),
        ("mov    a2, a3", "mov    a2, a3"),
    ],
    ids=[
        "mov.n",
        "s32i.n",
        "ret.n",
        "retw.n",
        "movi.n",
        "add.n",
        "l32i.n",
        "beqz.n",
        "b.w",
        "b.n",
        "no_suffix_call",
        "no_suffix_mov",
    ],
)
def test_normalize_encoding_suffixes(insn: str, expected: str) -> None:
    """Narrow/wide encoding suffixes are stripped."""
    assert _normalize_encoding(insn) == expected


@pytest.mark.parametrize(
    ("insn", "expected"),
    [
        ("or    a10, a2, a2", "mov    a10, a2"),
        ("or    a3, a15, a15", "mov    a3, a15"),
        # Different registers — genuine OR, not a move
        ("or    a8, a8, a9", "or    a8, a8, a9"),
        # Three different registers
        ("or    a2, a3, a4", "or    a2, a3, a4"),
    ],
    ids=["or_move_a2", "or_move_a15", "genuine_or", "three_diff_regs"],
)
def test_normalize_encoding_or_move_idiom(insn: str, expected: str) -> None:
    """Xtensa 'or rX, rY, rY' move idiom is normalized to 'mov rX, rY'."""
    assert _normalize_encoding(insn) == expected


def test_normalize_function_asm_or_move_normalized() -> None:
    """Xtensa or aX, aY, aY in function output is normalized to mov."""
    lines = [
        "  40001000:\tor    a10, a2, a2",
        "  40001003:\tcall8     40002000 <other_func>",
    ]
    result = _normalize_function_asm(lines, 0x40001000, "test_func")
    assert "mov    a10, a2" in result
    assert "or " not in result


# --- Tests for _is_safe_path ---


def test_is_safe_path_under_root(tmp_path: str) -> None:
    """Paths under source_root are allowed."""
    import os

    root = str(tmp_path)
    assert _is_safe_path(os.path.join(root, "src", "file.cpp"), root)


def test_is_safe_path_outside_root(tmp_path: str) -> None:
    """Paths outside source_root are rejected."""
    assert not _is_safe_path("/etc/passwd", str(tmp_path))


def test_is_safe_path_proc(tmp_path: str) -> None:
    """Proc filesystem paths are rejected."""
    assert not _is_safe_path("/proc/self/environ", str(tmp_path))


def test_normalize_function_asm_source_root_restricts_reads(tmp_path) -> None:
    """Source annotations only inline code from files under source_root."""
    # Create a source file inside the allowed root
    src_dir = tmp_path / "src"
    src_dir.mkdir()
    src_file = src_dir / "allowed.cpp"
    src_file.write_text("line1\nallowed_code_here\nline3\n")

    # Use POSIX-style paths in annotations (as objdump outputs)
    src_file_posix = src_file.as_posix()
    source_root_posix = tmp_path.as_posix()

    lines = [
        f"{src_file_posix}:2",
        "  40001000:\tmov.n    a2, a3",
        "/etc/passwd:1",
        "  40001002:\tret.n",
    ]
    result = _normalize_function_asm(
        lines, 0x40001000, "test_func", source_root=source_root_posix
    )
    # Allowed file content is inlined
    assert "allowed_code_here" in result
    # Disallowed path shows only basename:line, no content
    assert "# passwd:1" in result
    assert "root:" not in result
