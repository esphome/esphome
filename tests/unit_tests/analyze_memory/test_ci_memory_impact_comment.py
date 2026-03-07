"""Tests for script/ci_memory_impact_comment.py block-level diffing."""

from pathlib import Path
import sys

# Add script directory to path so we can import the module
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / "script"))

from ci_memory_impact_comment import (  # noqa: E402
    _count_instructions,
    _parse_source_blocks,
    _source_block_diff,
    prepare_disassembly_diffs,
)


def test_parse_source_blocks_simple() -> None:
    """Instructions without annotations form a single block."""
    asm = "mov    a2, a3\nret"
    blocks = _parse_source_blocks(asm)
    assert blocks == [("", "mov    a2, a3\nret")]


def test_parse_source_blocks_with_annotations() -> None:
    """Instructions are grouped by their source annotations."""
    asm = (
        "# file.cpp:10  foo()\nmov    a2, a3\ncall8 <bar>\n# file.cpp:11  return;\nret"
    )
    blocks = _parse_source_blocks(asm)
    assert len(blocks) == 2
    assert blocks[0] == ("# file.cpp:10  foo()", "mov    a2, a3\ncall8 <bar>")
    assert blocks[1] == ("# file.cpp:11  return;", "ret")


def test_parse_source_blocks_annotation_at_start() -> None:
    """First annotation with no preceding instructions."""
    asm = "# file.cpp:1  void f() {\nmov    a2, a3"
    blocks = _parse_source_blocks(asm)
    assert blocks == [("# file.cpp:1  void f() {", "mov    a2, a3")]


def test_source_block_diff_identical() -> None:
    """Identical asm produces no diff."""
    asm = "# file.cpp:10  foo()\nmov    a2, a3\nret"
    assert _source_block_diff(asm, asm) is None


def test_source_block_diff_changed_block() -> None:
    """Same-header block with changed instructions shows line-level diff."""
    target = "# file.cpp:10  foo()\nmov    a2, a3\ncall8 <baz>\n# file.cpp:11  bar()\ncall8 <bar>"
    pr = "# file.cpp:10  foo()\nmov    a2, a3\ncall8 <qux>\n# file.cpp:11  bar()\ncall8 <bar>"
    diff = _source_block_diff(target, pr)
    assert diff is not None
    # Source annotation shown as context (space prefix)
    assert " # file.cpp:10  foo()" in diff
    # Instruction-level diff within the block
    assert "-call8 <baz>" in diff
    assert "+call8 <qux>" in diff
    # Unchanged block should NOT appear
    assert any("bar" in line for line in diff) is False


def test_source_block_diff_register_only_skipped() -> None:
    """Register-only changes are skipped as noise."""
    target = "# file.cpp:10  foo()\nmov    a2, a3\n# file.cpp:11  bar()\ncall8 <bar>"
    pr = "# file.cpp:10  foo()\nmov    a4, a5\n# file.cpp:11  bar()\ncall8 <bar>"
    diff = _source_block_diff(target, pr)
    assert diff is None


def test_source_block_diff_new_block() -> None:
    """New block in PR shows as inserted."""
    target = "# file.cpp:10  foo()\nmov    a2, a3"
    pr = "# file.cpp:10  foo()\nmov    a2, a3\n# file.cpp:11  bar()\ncall8 <bar>"
    diff = _source_block_diff(target, pr)
    assert diff is not None
    assert "+# file.cpp:11  bar()" in diff
    assert "+call8 <bar>" in diff


def test_source_block_diff_removed_block() -> None:
    """Removed block shows as deleted."""
    target = "# file.cpp:10  foo()\nmov    a2, a3\n# file.cpp:11  bar()\ncall8 <bar>"
    pr = "# file.cpp:10  foo()\nmov    a2, a3"
    diff = _source_block_diff(target, pr)
    assert diff is not None
    assert "-# file.cpp:11  bar()" in diff
    assert "-call8 <bar>" in diff


def test_source_block_diff_separator_between_hunks() -> None:
    """Non-adjacent changes are separated by '...'."""
    target = (
        "# a.cpp:1  a()\ncall8 <foo>\n"
        "# b.cpp:2  b()\nmov a4, a5\n"
        "# c.cpp:3  c()\ncall8 <bar>"
    )
    pr = (
        "# a.cpp:1  a()\ncall8 <baz>\n"  # changed
        "# b.cpp:2  b()\nmov a4, a5\n"  # unchanged
        "# c.cpp:3  c()\ncall8 <qux>"  # changed
    )
    diff = _source_block_diff(target, pr)
    assert diff is not None
    assert "..." in diff


def test_source_block_diff_different_headers_full_replacement() -> None:
    """Blocks with different source annotations show as full replacement."""
    target = "# old.cpp:1  old_code()\nmov a2, a3\ncall8 <old>"
    pr = "# new.cpp:1  new_code()\nmov a4, a5\ncall8 <new>"
    diff = _source_block_diff(target, pr)
    assert diff is not None
    # Full blocks shown with +/-
    assert "-# old.cpp:1  old_code()" in diff
    assert "-call8 <old>" in diff
    assert "+# new.cpp:1  new_code()" in diff
    assert "+call8 <new>" in diff


def test_source_block_diff_line_number_shift_skipped() -> None:
    """Line number changes with identical instructions produce no diff."""
    target = (
        "# mipi_rgb.cpp:305  void fill(Color c) {\nentry a1, 48\n"
        "# mipi_rgb.cpp:306  if (!check())\nmov a10, a2\ncall8 <check>"
    )
    pr = (
        "# mipi_rgb.cpp:307  void fill(Color c) {\nentry a1, 48\n"
        "# mipi_rgb.cpp:308  if (!check())\nmov a10, a2\ncall8 <check>"
    )
    diff = _source_block_diff(target, pr)
    assert diff is None


def test_source_block_diff_line_number_shift_with_change() -> None:
    """Line number shift + real instruction change shows only the change."""
    target = (
        "# mipi_rgb.cpp:305  void fill(Color c) {\nentry a1, 48\n"
        "# mipi_rgb.cpp:306  if (!check())\nmov a10, a2\ncall8 <check>"
    )
    pr = (
        "# mipi_rgb.cpp:307  void fill(Color c) {\nentry a1, 48\n"
        "# mipi_rgb.cpp:308  if (!check())\nmov a10, a2\ncall8 <verify>"
    )
    diff = _source_block_diff(target, pr)
    assert diff is not None
    assert "-call8 <check>" in diff
    assert "+call8 <verify>" in diff
    # Unchanged block should not appear
    assert "entry" not in " ".join(diff)


def test_count_instructions_skips_comments() -> None:
    """Comment lines (source annotations) are not counted as instructions."""
    asm = (
        "# file.cpp:10  foo()\nmov    a2, a3\ncall8 <bar>\n# file.cpp:11  return;\nret"
    )
    assert _count_instructions(asm) == 3


def test_count_instructions_none() -> None:
    assert _count_instructions(None) == 0


def test_count_instructions_empty() -> None:
    assert _count_instructions("") == 0


def test_prepare_disassembly_diffs_uses_block_diff() -> None:
    """prepare_disassembly_diffs shows only changed blocks, not full unified diff."""
    target_disasm = {
        "my_func": (
            "# file.cpp:10  foo()\nmov    a2, a3\n"
            "# file.cpp:11  bar()\ncall8 <bar>\n"
            "# file.cpp:12  return;\nret"
        ),
    }
    pr_disasm = {
        "my_func": (
            "# file.cpp:10  foo()\nmov    a2, a3\n"
            "# file.cpp:11  baz()\ncall8 <baz>\n"  # changed
            "# file.cpp:12  return;\nret"
        ),
    }
    symbol_changes = {
        "changed_symbols": [("my_func", 100, 104, 4)],
        "new_symbols": [],
        "removed_symbols": [],
    }
    result = prepare_disassembly_diffs(target_disasm, pr_disasm, symbol_changes)
    assert result is not None
    assert len(result) == 1
    symbol, change_type, diff_text, target_insns, pr_insns = result[0]
    assert symbol == "my_func"
    assert change_type == "changed"
    # Changed block should be in diff
    assert "-call8 <bar>" in diff_text
    assert "+call8 <baz>" in diff_text
    # Unchanged blocks should NOT be in diff
    assert "mov    a2, a3" not in diff_text
    assert "ret" not in diff_text


def test_prepare_disassembly_diffs_identical_asm_skipped() -> None:
    """Symbols with identical disassembly are not included."""
    asm = "mov    a2, a3\nret"
    symbol_changes = {
        "changed_symbols": [("my_func", 100, 104, 4)],
        "new_symbols": [],
        "removed_symbols": [],
    }
    result = prepare_disassembly_diffs(
        {"my_func": asm}, {"my_func": asm}, symbol_changes
    )
    assert result is None
