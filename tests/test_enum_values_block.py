"""Tests for parse_enum_values_block in script/schema_doc.py.

Covers the structured <EnumValues> JSX form that replaces the fragile
bullet-list regex for enum descriptions. The bug being prevented: a
``(default)`` annotation followed by ``: description`` got mangled into
the description slot by the old REGEX_ENUM* patterns. With the JSX
form, value/default/description live in separate fields, so the bug is
structurally impossible.
"""

import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "script"))

import schema_doc  # noqa: E402
from schema_doc import (  # noqa: E402
    _apply_enum_entries,
    md_get_next_config,
    parse_enum_values_block,
)

# convert_links() reads args.deploy_url even though plain-text
# descriptions don't trigger link rewriting — set a dummy so tests don't
# need a CLI invocation.
schema_doc.args = SimpleNamespace(deploy_url="https://esphome.io")


def parse(text):
    """Run the parser against a multi-line string. Returns (end_index, entries)."""
    return parse_enum_values_block(text.splitlines(), 0)


def test_default_with_description_does_not_get_mangled():
    """Original device-builder#433 repro: '(default): …' must split cleanly."""
    end, entries = parse(
        """\
<EnumValues values={[
  { value: "ANY",     default: true, description: "Trigger on any edge change (high to low or low to high)" },
  { value: "RISING",  description: "Trigger only on rising edge (low to high)" },
  { value: "FALLING", description: "Trigger only on falling edge (high to low)" },
]} />
"""
    )
    assert entries == [
        {
            "value": "ANY",
            "default": True,
            "description": "Trigger on any edge change (high to low or low to high)",
        },
        {
            "value": "RISING",
            "description": "Trigger only on rising edge (low to high)",
        },
        {
            "value": "FALLING",
            "description": "Trigger only on falling edge (high to low)",
        },
    ]
    assert end == 5  # past the closing `]} />` line


def test_bare_values_no_descriptions():
    _, entries = parse(
        """\
<EnumValues values={[
  { value: "OFF", default: true },
  { value: "2x" },
  { value: "4x" },
]} />
"""
    )
    assert entries == [
        {"value": "OFF", "default": True},
        {"value": "2x"},
        {"value": "4x"},
    ]


def test_indented_block_inside_list_item():
    """The JSX is indented two spaces because it sits inside a list item."""
    _, entries = parse(
        """\
  <EnumValues values={[
    { value: "ANY", default: true, description: "Any edge" },
  ]} />
"""
    )
    assert entries == [{"value": "ANY", "default": True, "description": "Any edge"}]


def test_skips_leading_blank_lines():
    end, entries = parse(
        """\


<EnumValues values={[
  { value: "X" },
]} />
"""
    )
    assert entries == [{"value": "X"}]
    assert end == 5


def test_returns_none_when_no_block_present():
    end, entries = parse(
        """\
- `ANY` (default): Trigger on any edge change
- `RISING`: Trigger only on rising edge
"""
    )
    assert entries is None
    assert end == 0  # no advance — caller falls through to bullet path


def test_returns_none_when_entry_missing_value(capsys):
    """A malformed entry: caller sees None entries but a non-zero
    end_index so it can advance past the broken block. Anything else
    would put process_schema in an infinite loop on the same lines."""
    end, entries = parse(
        """\
<EnumValues values={[
  { default: true, description: "no value here" },
]} />
"""
    )
    assert entries is None
    assert end == 3  # advanced past the broken block (regression: was 0)
    assert "needs a 'value'" in capsys.readouterr().out


def test_unterminated_block_consumes_to_eof(capsys):
    """Without a closing `/>` the block is treated as running to EOF —
    the caller advances all the way so there's nothing left to loop on."""
    text = """\
<EnumValues values={[
  { value: "X" },
"""
    end, entries = parse(text)
    assert entries is None
    assert end == len(text.splitlines())  # consumed to EOF
    assert "unterminated" in capsys.readouterr().out


def test_trailing_commas_tolerated():
    _, entries = parse(
        """\
<EnumValues values={[
  { value: "A", description: "first", },
  { value: "B", },
]} />
"""
    )
    assert entries == [
        {"value": "A", "description": "first"},
        {"value": "B"},
    ]


def test_compact_one_line_form():
    _, entries = parse(
        """\
<EnumValues values={[{ value: "X", default: true }]} />
"""
    )
    assert entries == [{"value": "X", "default": True}]


def test_jsx_block_terminates_parent_bullet():
    """Without this, md_get_next_config slurps the JSX opener as bullet text.

    The bullet walker's continuation rule joins any indented non-blank
    line into the current bullet. A `<EnumValues>` opener is indented to
    sit inside its parent list item — so without an explicit boundary,
    the parent bullet's text would end up as e.g.
    ``"**interrupt_type** … One of: <EnumValues values={["``, and the
    structured-enum walker downstream would never run.
    """
    lines = [
        "- **interrupt_type** (*Optional*): One of:",
        "",
        "  <EnumValues values={[",
        '    { value: "ANY", default: true, description: "Any edge" },',
        "  ]} />",
        "",
    ]
    end_index, item_config, item_indent = md_get_next_config(lines, 0)
    assert item_config == "**interrupt_type** (*Optional*): One of:"
    assert item_indent == 0
    assert end_index == 2  # positioned at the JSX opener line


def test_apply_enum_entries_writes_docs_and_default():
    """End-to-end: parsed entries → schema dict matches what device-builder
    consumes. This is the structured-output side of the device-builder#433
    regression.

    Compare with the un-migrated bullet form, which (still) produces:

        "ANY":     {"docs": "default): Trigger on any edge change (high to low or low to high"}
        "RISING":  {"docs": null}
        "FALLING": {"docs": null}

    against the structured form's clean output, asserted below.
    """
    # Schema as esphome generates it: an enum with empty value slots.
    config = {
        "type": "enum",
        "values": {"ANY": None, "RISING": None, "FALLING": None},
    }
    entries = [
        {
            "value": "ANY",
            "default": True,
            "description": "Trigger on any edge change (high to low or low to high)",
        },
        {
            "value": "RISING",
            "description": "Trigger only on rising edge (low to high)",
        },
        {
            "value": "FALLING",
            "description": "Trigger only on falling edge (high to low)",
        },
    ]
    _apply_enum_entries(config, entries, Path("dummy.mdx"), 0)

    assert config["values"] == {
        "ANY": {
            "docs": "Trigger on any edge change (high to low or low to high)",
            "default": True,
        },
        "RISING": {"docs": "Trigger only on rising edge (low to high)"},
        "FALLING": {"docs": "Trigger only on falling edge (high to low)"},
    }


def test_apply_enum_entries_skips_values_not_in_schema():
    """Authors typo a value name → entry is silently dropped, matching
    the bullet path's behavior (which uses the same `if enum_value in
    values` guard). No crash, no schema corruption."""
    config = {"type": "enum", "values": {"A": None}}
    entries = [
        {"value": "A", "description": "first"},
        {"value": "BOGUS", "description": "second"},
    ]
    _apply_enum_entries(config, entries, Path("dummy.mdx"), 0)
    assert config["values"] == {"A": {"docs": "first"}}


def test_malformed_block_does_not_loop_forever():
    """Regression: a malformed <EnumValues> block must return an
    advanced end_index, not the same input index. Otherwise
    process_schema would reach md_get_next_config (which now stops at
    <EnumValues but doesn't consume it), get back the same index, and
    spin forever on the same lines.
    """
    lines = [
        "<EnumValues values={[",
        "  { default: true },",  # missing 'value' key
        "]} />",
        "",
    ]
    end, entries = parse_enum_values_block(lines, 0)
    assert entries is None
    assert end > 0, "must advance past the broken block to avoid infinite loop"


def test_no_block_present_does_not_advance():
    """Counterpart to the loop-prevention test: when there's no
    <EnumValues> block at the index, the parser MUST return the input
    index unchanged so the caller falls through to bullet handling."""
    lines = ["- `ANY` (default): Trigger on any edge change", ""]
    end, entries = parse_enum_values_block(lines, 0)
    assert entries is None
    assert end == 0  # no advance — caller falls through


def test_apply_enum_entries_default_only_no_description():
    """A value marked default but with no description gets the default flag
    written without a docs key. (The es8388 / bmp581 enum-list shape.)"""
    config = {"type": "enum", "values": {"OFF": None, "2x": None}}
    entries = [{"value": "OFF", "default": True}, {"value": "2x"}]
    _apply_enum_entries(config, entries, Path("dummy.mdx"), 0)
    assert config["values"] == {"OFF": {"default": True}, "2x": {}}


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-v"]))
