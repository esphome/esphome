"""Unit tests for script/build_language_schema.py."""

from __future__ import annotations

import ast
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

import pytest

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


def test_convert_walks_callable_schema_extractor() -> None:
    """A callable schema tagged for "schema" extraction is resolved and walked."""
    from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor

    @schema_extractor("schema")
    def dynamic_schema(value):
        if value is SCHEMA_EXTRACT:
            return cv.Schema({cv.Required("foo"): cv.string})
        return value

    config_var: dict = {}
    _bls.convert(dynamic_schema, config_var, "/test")

    assert config_var["type"] == "schema"
    assert "foo" in config_var["schema"]["config_vars"]


def test_convert_emits_variant_enum() -> None:
    """A per-variant enum is dumped with each value tagged by its variants."""
    from esphome.components.esp32 import (
        VARIANT_ESP32,
        VARIANT_ESP32S3,
        variant_filtered_enum,
    )

    validator = variant_filtered_enum(
        {VARIANT_ESP32: ("quad",), VARIANT_ESP32S3: ("quad", "octal")},
        lower=True,
    )
    config_var: dict = {}
    _bls.convert(validator, config_var, "/test")

    assert config_var["type"] == "enum"
    assert config_var["values"] == {
        "quad": {"variants": [VARIANT_ESP32, VARIANT_ESP32S3]},
        "octal": {"variants": [VARIANT_ESP32S3]},
    }


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


# ---------------------------------------------------------------------------
# Regression tests for the lvgl schema dump.
#
# lvgl's CONFIG_SCHEMA is a callable closure and its widget/style schemas are
# built lazily at validation time, so the static dumper used to emit an empty
# `lvgl:` schema, no widget completion, and an inlined ~80-property STYLE_SCHEMA
# duplicated at every widget x part x state (a 17 MB lvgl.json). These exercise
# the full `build_schema()` and assert the generated lvgl.json carries the data
# the schema_extractor hooks added.
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def lvgl_schema(tmp_path_factory: pytest.TempPathFactory) -> dict:
    """Run the full language-schema build once and return parsed lvgl.json.

    The build must run in a fresh interpreter: ``build_language_schema.py``
    enables schema extraction *before* importing any esphome component, and the
    extraction hooks are no-ops if the components were already imported (as they
    are inside the pytest session). Running it as a subprocess mirrors how CI
    generates the schema and keeps this test isolated from import order.
    """
    out_dir = tmp_path_factory.mktemp("language_schema")
    subprocess.run(
        [sys.executable, str(SCRIPT_PATH), "--output-path", str(out_dir)],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads((out_dir / "lvgl.json").read_text())


def _lvgl_config_vars(lvgl_schema: dict) -> dict:
    config_schema = lvgl_schema["lvgl"]["schemas"]["CONFIG_SCHEMA"]
    # Previously empty (`{}`); the schema_extractor on lvgl_config_schema now
    # hands the dumper the composed top-level schema.
    assert config_schema["type"] == "schema"
    return config_schema["schema"]["config_vars"]


def test_lvgl_top_level_schema_is_exposed(lvgl_schema: dict) -> None:
    config_vars = _lvgl_config_vars(lvgl_schema)
    # Was 0 config_vars before LVGL_TOP_LEVEL_SCHEMA was exposed.
    assert len(config_vars) > 100
    # A representative spread of top-level options the runtime validates.
    for key in ("displays", "pages", "default_font", "on_idle", "touchscreens"):
        assert key in config_vars, f"missing top-level lvgl option: {key}"


def test_lvgl_widgets_key_enumerated(lvgl_schema: dict) -> None:
    config_vars = _lvgl_config_vars(lvgl_schema)
    # The widgets: list is assembled per-value at runtime; the extractor
    # enumerates every registered widget type into a named WIDGET_TYPES schema
    # which the widgets: list references (recursive, so widgets can nest).
    assert "widgets" in config_vars
    widgets = config_vars["widgets"]
    assert widgets["is_list"] is True
    assert widgets["schema"]["extends"] == ["lvgl.WIDGET_TYPES"]

    widget_types = lvgl_schema["lvgl"]["schemas"]["WIDGET_TYPES"]["schema"][
        "config_vars"
    ]
    # Every registered widget type should appear as an optional key.
    for name in ("obj", "label", "button", "slider", "switch", "arc"):
        assert name in widget_types, f"widget type not enumerated: {name}"
    # Each enumerated widget carries its own property schema, not an empty stub.
    assert widget_types["label"]["type"] == "schema"
    assert len(widget_types["label"]["schema"]["config_vars"]) > 0
    # Each widget can contain child widgets, via the same named ref — so the
    # tree is recursive and the dump stays finite.
    nested = widget_types["obj"]["schema"]["config_vars"]["widgets"]
    assert nested["is_list"] is True
    assert nested["schema"]["extends"] == ["lvgl.WIDGET_TYPES"]


def test_lvgl_style_schemas_are_named_and_deduped(lvgl_schema: dict) -> None:
    schemas = lvgl_schema["lvgl"]["schemas"]
    # Importing these into the lvgl __init__ namespace lets the dumper register
    # them as named schemas and emit `extends` refs instead of inlining them.
    for name in ("STYLE_SCHEMA", "STATE_SCHEMA", "SET_STATE_SCHEMA"):
        assert name in schemas, f"style schema not registered as named: {name}"

    # STYLE_SCHEMA must be referenced via `extends`, not inlined at every use
    # site. Count the references to prove the dedup actually happened.
    refs = 0

    def _count(node: object) -> None:
        nonlocal refs
        if isinstance(node, dict):
            extends = node.get("extends")
            if isinstance(extends, list) and "lvgl.STYLE_SCHEMA" in extends:
                refs += 1
            for value in node.values():
                _count(value)
        elif isinstance(node, list):
            for value in node:
                _count(value)

    _count(lvgl_schema)
    assert refs > 100, f"STYLE_SCHEMA should be referenced via extends, got {refs}"
