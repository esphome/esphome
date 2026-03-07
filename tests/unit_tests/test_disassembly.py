"""Tests for esphome.analyze_memory.disassembly normalization."""

from esphome.analyze_memory.disassembly import (
    _find_balanced_ref,
    _normalize_all_refs,
    _normalize_function_asm,
)


class TestFindBalancedRef:
    """Tests for balanced angle bracket matching."""

    def test_simple_symbol(self):
        text = "<foo>"
        assert _find_balanced_ref(text, 0) == ("foo", 5)

    def test_symbol_with_offset(self):
        text = "<foo+0x20>"
        assert _find_balanced_ref(text, 0) == ("foo+0x20", 10)

    def test_nested_template(self):
        text = "<std::vector<unsigned char> >"
        assert _find_balanced_ref(text, 0) == ("std::vector<unsigned char> ", 29)

    def test_deeply_nested_template(self):
        text = "<std::vector<unsigned char, std::allocator<unsigned char> >::resize(unsigned int)+0x20>"
        result = _find_balanced_ref(text, 0)
        assert result is not None
        content, end = result
        assert end == len(text)
        assert (
            content
            == "std::vector<unsigned char, std::allocator<unsigned char> >::resize(unsigned int)+0x20"
        )

    def test_offset_into_string(self):
        text = "call0     <some_func>"
        assert _find_balanced_ref(text, 10) == ("some_func", 21)

    def test_unbalanced_returns_none(self):
        assert _find_balanced_ref("<unclosed", 0) is None

    def test_out_of_bounds(self):
        assert _find_balanced_ref("abc", 5) is None

    def test_not_angle_bracket(self):
        assert _find_balanced_ref("abc", 0) is None


class TestNormalizeAllRefs:
    """Tests for instruction-level symbolic reference normalization."""

    def test_simple_cross_ref(self):
        insn = "call0     <other_func>"
        assert _normalize_all_refs(insn, "my_func", False) == "call0     <other_func>"

    def test_cross_ref_strips_offset(self):
        insn = "call0     <other_func+0x20>"
        assert _normalize_all_refs(insn, "my_func", False) == "call0     <other_func>"

    def test_self_ref(self):
        insn = "j     <my_func+0x40>"
        assert _normalize_all_refs(insn, "my_func", False) == "j     <self>"

    def test_self_ref_no_offset(self):
        insn = "j     <my_func>"
        assert _normalize_all_refs(insn, "my_func", False) == "j     <self>"

    def test_l32r_literal(self):
        insn = "l32r    a5,  <some_symbol+0x10>"
        assert _normalize_all_refs(insn, "my_func", True) == "l32r    a5,  <.literal>"

    def test_literal_pool_symbol(self):
        insn = "j     <_lit4_end+0x1234>"
        assert _normalize_all_refs(insn, "my_func", False) == "j     <.literal>"

    def test_l32r_template_symbol(self):
        """The bug: l32r with C++ template symbol was leaving trailing text."""
        insn = (
            "l32r    a5,  "
            "<std::vector<unsigned char, std::allocator<unsigned char> >"
            "::resize(unsigned int)+0x20>"
        )
        assert _normalize_all_refs(insn, "my_func", True) == "l32r    a5,  <.literal>"

    def test_template_cross_ref(self):
        insn = (
            "call0     "
            "<std::vector<unsigned char, std::allocator<unsigned char> >"
            "::reserve(unsigned int)>"
        )
        expected = (
            "call0     "
            "<std::vector<unsigned char, std::allocator<unsigned char> >"
            "::reserve(unsigned int)>"
        )
        assert _normalize_all_refs(insn, "my_func", False) == expected

    def test_template_cross_ref_strips_offset(self):
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

    def test_no_refs(self):
        insn = "mov.n    a2, a3"
        assert _normalize_all_refs(insn, "my_func", False) == "mov.n    a2, a3"

    def test_all_linker_symbols(self):
        for sym in ("_lit4_end", "_iram_end", "_data_end", "_bss_end", "_heap_end"):
            insn = f"j     <{sym}+0x10>"
            assert _normalize_all_refs(insn, "my_func", False) == "j     <.literal>"


class TestNormalizeFunctionAsm:
    """Tests for full function disassembly normalization."""

    def test_relative_offsets(self):
        lines = [
            "  40001000:\tmov.n    a2, a3",
            "  40001002:\tret.n",
        ]
        result = _normalize_function_asm(lines, 0x40001000, "test_func")
        assert result == "+0x0000: mov.n    a2, a3\n+0x0002: ret.n"

    def test_skips_non_instruction_lines(self):
        lines = [
            "",
            "  40001000:\tmov.n    a2, a3",
            "   ...",
            "  40001002:\tret.n",
        ]
        result = _normalize_function_asm(lines, 0x40001000, "test_func")
        assert result == "+0x0000: mov.n    a2, a3\n+0x0002: ret.n"

    def test_strips_absolute_addr_before_ref(self):
        lines = [
            "  40001000:\tcall0     40002000 <other_func>",
        ]
        result = _normalize_function_asm(lines, 0x40001000, "test_func")
        assert result == "+0x0000: call0      <other_func>"

    def test_self_branch_normalized(self):
        lines = [
            "  40001000:\tj     40001010 <test_func+0x10>",
        ]
        result = _normalize_function_asm(lines, 0x40001000, "test_func")
        assert result == "+0x0000: j      <self>"

    def test_l32r_normalized_to_literal(self):
        lines = [
            "  40001000:\tl32r    a5, 40000ffc <_lit4_end+0x1234>",
        ]
        result = _normalize_function_asm(lines, 0x40001000, "test_func")
        assert result == "+0x0000: l32r    a5,  <.literal>"

    def test_template_symbol_in_call(self):
        """Ensure C++ template symbols in call instructions are handled correctly."""
        lines = [
            "  40001000:\tcall0     40002000 <std::vector<unsigned char, std::allocator<unsigned char> >::reserve(unsigned int)>",
        ]
        result = _normalize_function_asm(lines, 0x40001000, "test_func")
        expected = (
            "+0x0000: call0      "
            "<std::vector<unsigned char, std::allocator<unsigned char> >"
            "::reserve(unsigned int)>"
        )
        assert result == expected

    def test_template_symbol_in_l32r(self):
        """The original bug: l32r with template symbol left trailing garbage."""
        lines = [
            "  40001000:\tl32r    a5, 40000ffc <std::vector<unsigned char, std::allocator<unsigned char> >::resize(unsigned int)+0x20>",
        ]
        result = _normalize_function_asm(lines, 0x40001000, "test_func")
        assert result == "+0x0000: l32r    a5,  <.literal>"

    def test_empty_input(self):
        assert _normalize_function_asm([], 0x40001000, "test_func") == ""
