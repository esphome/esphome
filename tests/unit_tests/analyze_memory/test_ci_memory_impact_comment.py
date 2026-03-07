"""Tests for script/ci_memory_impact_comment.py disassembly diffing."""

from pathlib import Path
import sys

# Add script directory to path so we can import the module
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / "script"))

from ci_memory_impact_comment import (  # noqa: E402
    _count_instructions,
    _source_block_diff,
    prepare_disassembly_diffs,
)


def test_source_block_diff_identical() -> None:
    """Identical asm produces no diff."""
    asm = "# file.cpp:10  foo()\nmov    a2, a3\nret"
    assert _source_block_diff(asm, asm) is None


def test_source_block_diff_changed_block() -> None:
    """Changed instructions show line-level diff."""
    target = "# file.cpp:10  foo()\nmov    a2, a3\ncall8 <baz>\n# file.cpp:11  bar()\ncall8 <bar>"
    pr = "# file.cpp:10  foo()\nmov    a2, a3\ncall8 <qux>\n# file.cpp:11  bar()\ncall8 <bar>"
    diff = _source_block_diff(target, pr)
    assert diff is not None
    assert "-call8 <baz>" in diff
    assert "+call8 <qux>" in diff
    # Unchanged lines should NOT appear as changes
    assert any("bar" in line for line in diff if line.startswith(("+", "-"))) is False


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
    # Need enough unchanged lines between changes to exceed unified_diff context (n=2)
    target = (
        "# a.cpp:1  a()\ncall8 <foo>\n"
        "# b.cpp:2  b()\nmov a4, a5\n"
        "# b.cpp:3  b2()\nmov a6, a7\n"
        "# b.cpp:4  b3()\nmov a8, a9\n"
        "# b.cpp:5  b4()\nmov a10, a11\n"
        "# c.cpp:6  c()\ncall8 <bar>"
    )
    pr = (
        "# a.cpp:1  a()\ncall8 <baz>\n"  # changed
        "# b.cpp:2  b()\nmov a4, a5\n"  # unchanged
        "# b.cpp:3  b2()\nmov a6, a7\n"  # unchanged
        "# b.cpp:4  b3()\nmov a8, a9\n"  # unchanged
        "# b.cpp:5  b4()\nmov a10, a11\n"  # unchanged
        "# c.cpp:6  c()\ncall8 <qux>"  # changed
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
    # Unchanged lines should not appear as changes
    assert "entry" not in " ".join(line for line in diff if line.startswith(("+", "-")))


def test_source_block_diff_source_text_mismatch() -> None:
    """Annotations with/without source text still match by filename."""
    target = "# file.cpp:123\nmov a2, a3\ncall8 <foo>"
    pr = "# file.cpp:125  some_func()\nmov a2, a3\ncall8 <foo>"
    diff = _source_block_diff(target, pr)
    assert diff is None


def test_source_block_diff_block_reorder_skipped() -> None:
    """Compiler block reordering (same instructions, different order) is skipped."""
    target = "# a.cpp:1  a()\nmov a2, a3\ncall8 <foo>\n# b.cpp:2  b()\ncall8 <bar>\nret"
    pr = "# b.cpp:2  b()\ncall8 <bar>\nret\n# a.cpp:1  a()\nmov a2, a3\ncall8 <foo>"
    diff = _source_block_diff(target, pr)
    assert diff is None


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
    # Unchanged lines should not appear as changes (may appear as context)
    changed_lines = [
        line for line in diff_text.splitlines() if line.startswith(("+", "-"))
    ]
    assert all("mov    a2, a3" not in line for line in changed_lines)


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
