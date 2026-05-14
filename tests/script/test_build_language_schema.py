"""Unit tests for script/build_language_schema.py."""

from __future__ import annotations

import ast
from pathlib import Path

SCRIPT_PATH = (
    Path(__file__).resolve().parent.parent.parent
    / "script"
    / "build_language_schema.py"
)


def _extract_sort_obj():
    # build_language_schema.py runs argparse, loads every component, and
    # calls build_schema() at import time, so a plain import isn't viable
    # in a unit test. Pull just the pure helper out via AST instead.
    tree = ast.parse(SCRIPT_PATH.read_text())
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "sort_obj":
            namespace: dict = {"S_TYPE": "type"}
            module = ast.Module(body=[node], type_ignores=[])
            exec(compile(module, str(SCRIPT_PATH), "exec"), namespace)
            return namespace["sort_obj"]
    raise AssertionError("sort_obj not found in build_language_schema.py")


sort_obj = _extract_sort_obj()


def test_sort_obj_sorts_dict_keys() -> None:
    result = sort_obj({"b": 1, "a": 2, "c": 3})
    assert list(result.keys()) == ["a", "b", "c"]


def test_sort_obj_sorts_nested_dicts() -> None:
    result = sort_obj({"outer": {"z": 1, "a": 2}})
    assert list(result["outer"].keys()) == ["a", "z"]


def test_sort_obj_preserves_enum_values_order() -> None:
    config = {
        "type": "enum",
        "values": {
            "2MB": None,
            "4MB": None,
            "8MB": None,
            "16MB": None,
            "32MB": None,
        },
    }
    result = sort_obj(config)
    assert list(result["values"].keys()) == ["2MB", "4MB", "8MB", "16MB", "32MB"]


def test_sort_obj_sorts_non_enum_values_key() -> None:
    config = {"type": "schema", "values": {"z": 1, "a": 2}}
    result = sort_obj(config)
    assert list(result["values"].keys()) == ["a", "z"]


def test_sort_obj_sorts_other_keys_in_enum() -> None:
    config = {
        "type": "enum",
        "default": "4MB",
        "key": "Optional",
        "values": {"2MB": None, "4MB": None},
    }
    result = sort_obj(config)
    assert list(result.keys()) == ["default", "key", "type", "values"]
    assert list(result["values"].keys()) == ["2MB", "4MB"]


def test_sort_obj_recurses_into_enum_value_entries() -> None:
    config = {
        "type": "enum",
        "values": {
            "esp32": {"name": "ESP32", "docs": "Original"},
            "esp32-c3": {"name": "ESP32-C3", "docs": "RISC-V"},
        },
    }
    result = sort_obj(config)
    assert list(result["values"].keys()) == ["esp32", "esp32-c3"]
    assert list(result["values"]["esp32"].keys()) == ["docs", "name"]


def test_sort_obj_handles_lists() -> None:
    result = sort_obj([{"b": 1, "a": 2}, {"d": 3, "c": 4}])
    assert list(result[0].keys()) == ["a", "b"]
    assert list(result[1].keys()) == ["c", "d"]


def test_sort_obj_passes_through_scalars() -> None:
    assert sort_obj("hello") == "hello"
    assert sort_obj(42) == 42
    assert sort_obj(None) is None
    assert sort_obj(True) is True
