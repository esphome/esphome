"""Unit tests for script/build_language_schema.py."""

from __future__ import annotations

import ast
import importlib.util
from pathlib import Path

from esphome import config_validation as cv

SCRIPT_PATH = (
    Path(__file__).resolve().parent.parent.parent
    / "script"
    / "build_language_schema.py"
)


def _load_script_module():
    spec = importlib.util.spec_from_file_location("build_language_schema", SCRIPT_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _extract_sort_obj():
    # ``sort_obj`` is pure and self-contained; pulling it via AST avoids
    # exercising the module-level component-loading state for these tests.
    tree = ast.parse(SCRIPT_PATH.read_text())
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "sort_obj":
            namespace: dict = {"S_TYPE": "type"}
            module = ast.Module(body=[node], type_ignores=[])
            exec(compile(module, str(SCRIPT_PATH), "exec"), namespace)
            return namespace["sort_obj"]
    raise AssertionError("sort_obj not found in build_language_schema.py")


sort_obj = _extract_sort_obj()
_bls = _load_script_module()


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


def test_convert_emits_explicit_sensitive_marker() -> None:
    config_var: dict = {}
    _bls.convert(cv.sensitive(cv.string), config_var, "/test")

    assert config_var["sensitive"] is True
    assert config_var["sensitive_source"] == "explicit"
    assert config_var["type"] == "string"


def test_convert_keys_emits_heuristic_sensitive_marker() -> None:
    converted: dict = {}
    _bls.convert_keys(converted, {cv.Optional("password"): cv.string}, "/root")

    entry = converted["schema"]["config_vars"]["password"]
    assert entry["sensitive"] is True
    assert entry["sensitive_source"] == "heuristic"
    assert entry["type"] == "string"


def test_convert_keys_explicit_beats_heuristic() -> None:
    # Key name matches a fragment but the validator is explicitly wrapped;
    # the explicit branch should win and emit ``sensitive_source: explicit``.
    converted: dict = {}
    _bls.convert_keys(
        converted, {cv.Optional("password"): cv.sensitive(cv.string)}, "/root"
    )

    entry = converted["schema"]["config_vars"]["password"]
    assert entry["sensitive"] is True
    assert entry["sensitive_source"] == "explicit"


def test_convert_keys_no_heuristic_for_non_string_leaves() -> None:
    # Even though the key contains a fragment, a non-string leaf must not
    # be flagged. Prevents false positives on unrelated fields whose name
    # happens to embed a substring like "token".
    converted: dict = {}
    _bls.convert_keys(converted, {cv.Optional("password"): cv.boolean}, "/root")

    entry = converted["schema"]["config_vars"]["password"]
    assert "sensitive" not in entry
    assert "sensitive_source" not in entry


def test_convert_keys_no_marker_for_non_sensitive_field() -> None:
    converted: dict = {}
    _bls.convert_keys(converted, {cv.Optional("hostname"): cv.string}, "/root")

    entry = converted["schema"]["config_vars"]["hostname"]
    assert "sensitive" not in entry
    assert "sensitive_source" not in entry
