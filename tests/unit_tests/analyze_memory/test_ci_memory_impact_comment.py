"""Tests for script/ci_memory_impact_comment.py symbol matching."""

from pathlib import Path
import sys

# Add script directory to path so we can import the module
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / "script"))

from ci_memory_impact_comment import prepare_symbol_changes_data  # noqa: E402


def test_prepare_symbol_changes_signature_match() -> None:
    """Symbols with same base name but different args are matched as changed."""
    target = {
        "Foo::bar(std::vector<unsigned char>&, int)": 300,
        "unchanged()": 50,
    }
    pr = {
        "Foo::bar(ProtoByteBuffer&, int)": 320,
        "unchanged()": 50,
    }
    result = prepare_symbol_changes_data(target, pr)
    assert result is not None
    assert len(result["changed_symbols"]) == 1
    assert len(result["new_symbols"]) == 0
    assert len(result["removed_symbols"]) == 0
    sym, t_size, p_size, delta = result["changed_symbols"][0]
    assert sym == "Foo::bar(ProtoByteBuffer&, int)"
    assert t_size == 300
    assert p_size == 320
    assert delta == 20


def test_prepare_symbol_changes_ambiguous_overloads_not_matched() -> None:
    """Multiple overloads with same base name stay as new/removed."""
    target = {
        "Foo::bar(int)": 100,
        "Foo::bar(float)": 200,
    }
    pr = {
        "Foo::bar(double)": 150,
        "Foo::bar(long)": 250,
    }
    result = prepare_symbol_changes_data(target, pr)
    assert result is not None
    assert len(result["changed_symbols"]) == 0
    assert len(result["new_symbols"]) == 2
    assert len(result["removed_symbols"]) == 2


def test_prepare_symbol_changes_no_parens_not_matched() -> None:
    """Symbols without parens (variables) are not fuzzy-matched."""
    target = {"my_global_var": 100}
    pr = {"my_global_var_v2": 120}
    result = prepare_symbol_changes_data(target, pr)
    assert result is not None
    assert len(result["changed_symbols"]) == 0
    assert len(result["new_symbols"]) == 1
    assert len(result["removed_symbols"]) == 1


def test_prepare_symbol_changes_nested_symbols_matched_separately() -> None:
    """Nested symbols like ::__pstr__ don't collide with parent function."""
    target = {
        "Foo::bar(std::vector<unsigned char>&, int)": 300,
        "Foo::bar(std::vector<unsigned char>&, int)::__pstr__": 19,
    }
    pr = {
        "Foo::bar(ProtoByteBuffer&, int)": 320,
        "Foo::bar(ProtoByteBuffer&, int)::__pstr__": 19,
    }
    result = prepare_symbol_changes_data(target, pr)
    assert result is not None
    # Both the function and its nested __pstr__ should be matched (not new/removed)
    assert len(result["new_symbols"]) == 0
    assert len(result["removed_symbols"]) == 0
    # __pstr__ has delta=0 so it's silently dropped, only the function shows
    assert len(result["changed_symbols"]) == 1
    sym, t_size, p_size, delta = result["changed_symbols"][0]
    assert sym == "Foo::bar(ProtoByteBuffer&, int)"
    assert delta == 20


def test_prepare_symbol_changes_exact_match_preferred() -> None:
    """Exact name matches are found before fuzzy matching runs."""
    target = {
        "Foo::bar(int)": 100,
    }
    pr = {
        "Foo::bar(int)": 120,
    }
    result = prepare_symbol_changes_data(target, pr)
    assert result is not None
    assert len(result["changed_symbols"]) == 1
    assert len(result["new_symbols"]) == 0
    assert len(result["removed_symbols"]) == 0
    sym, t_size, p_size, delta = result["changed_symbols"][0]
    assert sym == "Foo::bar(int)"
    assert delta == 20
