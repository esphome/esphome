"""Unit tests for script/build_language_schema.py."""

from __future__ import annotations

import ast
from collections.abc import Callable
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
from typing import Any

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


def test_convert_sees_pin_through_optional_pin_wrapper() -> None:
    """A pin schema wrapped by PIN_SCHEMA_REGISTRY.optional() is still dumped as a pin."""
    from esphome import pins

    _bls.add_pin_validators()
    schema = pins.gpio_output_pin_schema
    expected: dict = {}
    _bls.convert(schema, expected, "/test")
    config_var: dict = {}
    _bls.convert(pins.PIN_SCHEMA_REGISTRY.optional(schema), config_var, "/test")

    assert expected["type"] == "pin"
    assert config_var == expected


def test_convert_emits_key_constraints() -> None:
    base = cv.Schema({cv.Optional("transport", default="tcp"): cv.string})
    schema = cv.All(
        base.extend({cv.Optional("path"): cv.string}),
        cv.only_when("path", "transport", ("ws", "wss")),
        cv.only_when(("path", "transport"), "mode", "tls", negate=True),
        cv.key_requires("path", "a", "b", any_of=True),
    )
    assert _bls.convert_config(schema, "/test")["schema"]["constraints"] == [
        {
            "type": "only_when",
            "keys": ["path"],
            "selector": "transport",
            "values": ["ws", "wss"],
            "negate": False,
        },
        {
            "type": "only_when",
            "keys": ["path", "transport"],
            "selector": "mode",
            "values": ["tls"],
            "negate": True,
        },
        {
            "type": "key_requires",
            "keys": ["path"],
            "required": ["a", "b"],
            "any_of": True,
        },
    ]


def test_overlay_field_keeps_base_and_local_constraints() -> None:
    base = {"schema": {"constraints": [{"type": "key_requires", "keys": ["a"]}]}}
    local = {"schema": {"constraints": [{"type": "only_when", "keys": ["b"]}]}}
    assert _bls.overlay_field(local, base)["schema"]["constraints"] == [
        {"type": "key_requires", "keys": ["a"]},
        {"type": "only_when", "keys": ["b"]},
    ]


def test_overlay_field_enum_option_named_constraints() -> None:
    base = {"values": {"constraints": None}}
    assert _bls.overlay_field({"key": "Optional"}, base) == {
        "key": "Optional",
        "values": {"constraints": None},
    }


def test_inline_schema_keeps_base_and_local_constraints() -> None:
    node = {"constraints": [{"type": "only_when"}]}
    base = {"config_vars": {"a": {}}, "constraints": [{"type": "key_requires"}]}
    _bls.inline_schema(node, base)
    assert node == {
        "config_vars": {"a": {}},
        "constraints": [{"type": "key_requires"}, {"type": "only_when"}],
    }


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


def _wildcard_validator(value: Any) -> Any:
    return value


def test_convert_keys_marker_wrapped_callable_key_normalizes() -> None:
    converted: dict = {}
    _bls.convert_keys(converted, {cv.Optional(_wildcard_validator): cv.string}, "/root")

    config_vars = converted["schema"]["config_vars"]
    assert set(config_vars) == {"string"}
    assert config_vars["string"]["key"] == "Optional"
    assert config_vars["string"]["key_type"] == "_wildcard_validator"


def test_convert_keys_marker_wrapped_callable_beside_fixed_keys() -> None:
    converted: dict = {}
    _bls.convert_keys(
        converted,
        {cv.Required("id"): cv.string, cv.Optional(_wildcard_validator): cv.string},
        "/root",
    )

    assert set(converted["schema"]["config_vars"]) == {"id", "string"}


def test_convert_keys_bare_callable_dotted_qualname() -> None:
    def make_validator() -> Callable[[Any], Any]:
        def validator(value: Any) -> Any:
            return value

        return validator

    converted: dict = {}
    _bls.convert_keys(converted, {make_validator(): cv.string}, "/root")

    assert converted["key"] == "String"
    assert converted["key_type"].endswith("make_validator.<locals>.validator")
    assert "at 0x" not in converted["key_type"]
    assert set(converted["schema"]["config_vars"]) == {"string"}


@pytest.fixture(scope="module")
def language_schema_dir(tmp_path_factory: pytest.TempPathFactory) -> Path:
    """Run the full language-schema build once and return the output directory.

    The build must run in a fresh interpreter: ``build_language_schema.py``
    enables schema extraction *before* importing any esphome component, and the
    extraction hooks are no-ops if the components were already imported (as they
    are inside the pytest session). Running it as a subprocess mirrors how CI
    generates the schema and keeps these tests isolated from import order.
    """
    out_dir = tmp_path_factory.mktemp("language_schema")
    subprocess.run(
        [sys.executable, str(SCRIPT_PATH), "--output-path", str(out_dir)],
        check=True,
        capture_output=True,
        text=True,
    )
    return out_dir


def test_uart_clock_source_preserves_variant_metadata(
    language_schema_dir: Path,
) -> None:
    """UART clock choices retain chip restrictions in the editor schema."""
    uart_schema = json.loads((language_schema_dir / "uart.json").read_text())
    clock_source = uart_schema["uart"]["schemas"]["CONFIG_SCHEMA"]["schema"][
        "config_vars"
    ]["clock_source"]

    assert clock_source["type"] == "enum"
    assert clock_source["values"]["REF_TICK"] == {"variants": ["ESP32", "ESP32S2"]}
    assert "ESP32C6" in clock_source["values"]["XTAL"]["variants"]
    assert "ESP32C6" not in clock_source["values"]["APB"]["variants"]


def test_automation_with_one_extra_key_keeps_it(language_schema_dir: Path) -> None:
    """A trigger whose only extra key is not trigger_id still lists that key."""
    modbus_schema = json.loads((language_schema_dir / "modbus_client.json").read_text())
    on_no_response = modbus_schema["modbus_client"]["schemas"]["_ACTION_BASE_SCHEMA"][
        "schema"
    ]["config_vars"]["on_no_response"]

    assert on_no_response["type"] == "trigger"
    assert set(on_no_response["schema"]["config_vars"]) == {"retry", "then"}


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
def lvgl_schema(language_schema_dir: Path) -> dict:
    return json.loads((language_schema_dir / "lvgl.json").read_text())


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


def test_convert_typed_schema_with_an_empty_type(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A typed_schema type with no keys of its own is dumped with no config vars."""
    types = {
        "with_keys": cv.Schema({cv.Required("byte"): cv.hex_uint8_t}),
        "no_keys": cv.Schema({}),
    }
    schema = cv.typed_schema(types, key="mode")
    # cv.typed_schema was decorated before the script enabled extraction, so register it by hand
    monkeypatch.setitem(
        _bls.ejs.typed_schemas, repr(schema), ((types,), {"key": "mode"})
    )
    config_var: dict = {}
    _bls.convert(schema, config_var, "/test")

    assert config_var["type"] == "typed"
    assert config_var["typed_key"] == "mode"
    assert "byte" in config_var["types"]["with_keys"]["config_vars"]
    assert config_var["types"]["no_keys"] == {"config_vars": {}}


def _schema(config_vars: dict | None = None, extends: list | None = None) -> dict:
    schema: dict = {}
    if config_vars is not None:
        schema["config_vars"] = config_vars
    if extends is not None:
        schema["extends"] = extends
    return {"type": "schema", "schema": schema}


def test_splice_extends_drops_the_key_when_nothing_remains() -> None:
    node: dict = {"extends": []}
    _bls.splice_extends(node, ["b"], "b")
    assert node == {}


def _shrink(monkeypatch: pytest.MonkeyPatch, output: dict) -> None:
    monkeypatch.setattr(_bls, "output", output)
    monkeypatch.setattr(_bls, "components", {})
    _bls.shrink()


def test_shrink_inlining_one_extend_keeps_the_others(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Inlining a once-referenced schema keeps the target's other extends."""
    output = {
        "base": {"schemas": {"SHARED": _schema({"name": {"key": "Optional"}})}},
        "mod": {
            "schemas": {
                "ONCE": _schema({"extra": {"key": "Optional"}}),
                "CONFIG_SCHEMA": _schema({}, ["base.SHARED", "mod.ONCE"]),
            }
        },
        "other": {"schemas": {"CONFIG_SCHEMA": _schema({}, ["base.SHARED"])}},
    }
    _shrink(monkeypatch, output)

    schema = output["mod"]["schemas"]["CONFIG_SCHEMA"]["schema"]
    assert schema["extends"] == ["base.SHARED"]
    assert "extra" in schema["config_vars"]


def test_shrink_drops_a_bare_validator_beside_config_vars(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """An empty extend next to real keys (cv.All(SCHEMA, check)) is dropped, not collapsed."""
    output = {
        "mod": {
            "schemas": {
                "check": {},
                "CONFIG_SCHEMA": _schema(
                    {"address": {"key": "Required"}}, ["mod.check"]
                ),
            }
        },
        "other": {"schemas": {"CONFIG_SCHEMA": _schema(None, ["mod.check"])}},
    }
    _shrink(monkeypatch, output)

    assert output["mod"]["schemas"]["CONFIG_SCHEMA"] == _schema(
        {"address": {"key": "Required"}}
    )
    assert output["other"]["schemas"]["CONFIG_SCHEMA"] == {"data_type": "check"}


def test_shrink_inlined_schema_brings_its_own_extends(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The inlined schema's extends take its place beside the target's siblings."""
    output = {
        "base": {
            "schemas": {
                "SHARED": _schema({"name": {"key": "Optional"}}),
                "INNER": _schema({"inner": {"key": "Optional"}}),
            }
        },
        "mod": {
            "schemas": {
                "ONCE": _schema({"extra": {"key": "Optional"}}, ["base.INNER"]),
                "CONFIG_SCHEMA": _schema(
                    {"local": {"key": "Optional"}}, ["base.SHARED", "mod.ONCE"]
                ),
            }
        },
        "other": {
            "schemas": {"CONFIG_SCHEMA": _schema({}, ["base.SHARED", "base.INNER"])}
        },
    }
    _shrink(monkeypatch, output)

    schema = output["mod"]["schemas"]["CONFIG_SCHEMA"]["schema"]
    assert schema["extends"] == ["base.SHARED", "base.INNER"]
    assert set(schema["config_vars"]) == {"local", "extra"}


def test_shrink_inlines_into_a_typed_variant(monkeypatch: pytest.MonkeyPatch) -> None:
    """A typed variant's top-level extends keep the sibling, the inherited base and local keys."""
    output = {
        "base": {
            "schemas": {
                "SHARED": _schema({"name": {"key": "Optional"}}),
                "INNER": _schema({"inner": {"key": "Optional"}}),
            }
        },
        "mod": {
            "schemas": {
                "ONCE": _schema({"extra": {"key": "Optional"}}, ["base.INNER"]),
                "CONFIG_SCHEMA": {
                    "type": "typed",
                    "types": {
                        "v1": {
                            "config_vars": {"local": {"key": "Optional"}},
                            "extends": ["base.SHARED", "mod.ONCE"],
                        }
                    },
                },
            }
        },
        "other": {
            "schemas": {"CONFIG_SCHEMA": _schema({}, ["base.SHARED", "base.INNER"])}
        },
    }
    _shrink(monkeypatch, output)

    variant = output["mod"]["schemas"]["CONFIG_SCHEMA"]["types"]["v1"]
    assert variant["extends"] == ["base.SHARED", "base.INNER"]
    assert set(variant["config_vars"]) == {"local", "extra"}


def test_shrink_spread_keeps_enum_values_beside_a_schema(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """An enum spliced beside a plain schema keeps its values; alone it is folded in."""
    enum = {"type": "enum", "values": {"a": None}}
    output = {
        "base": {
            "schemas": {"E": enum, "B": _schema({"b": {"key": "Optional"}})},
        },
        "mod": {
            "schemas": {
                "CONFIG_SCHEMA": _schema({"mode": _schema(None, ["base.E", "base.B"])})
            }
        },
        "other": {
            "schemas": {
                "CONFIG_SCHEMA": _schema(
                    {
                        "mode": _schema(None, ["base.E"]),
                        "b": _schema(None, ["base.B"]),
                    }
                )
            }
        },
    }
    _shrink(monkeypatch, output)

    mod_vars = output["mod"]["schemas"]["CONFIG_SCHEMA"]["schema"]["config_vars"]
    other_vars = output["other"]["schemas"]["CONFIG_SCHEMA"]["schema"]["config_vars"]
    assert mod_vars["mode"] == {**_schema(None, ["base.B"]), "values": {"a": None}}
    assert other_vars["mode"] == enum


def test_gree_feature_switches_extend_the_switch_schema(
    language_schema_dir: Path,
) -> None:
    """Gree's closure-built feature switches export the switch schema."""
    gree = json.loads((language_schema_dir / "gree.json").read_text())
    config_vars = gree["gree.switch"]["schemas"]["CONFIG_SCHEMA"]["schema"][
        "config_vars"
    ]
    for key in ("health", "light", "turbo", "xfan"):
        assert config_vars[key]["schema"]["extends"] == ["switch._SWITCH_SCHEMA"]


def test_inline_schema_merges_a_same_key_override_per_field() -> None:
    """A local override keeps the base field's type and values, and the base is not mutated."""
    base = {
        "config_vars": {
            "mode": {
                "key": "Optional",
                "type": "enum",
                "values": {"a": None, "b": None},
            }
        }
    }
    node = {"config_vars": {"mode": {"default": "b"}}, "extends": ["x"]}
    _bls.inline_schema(node, base)

    assert node["config_vars"]["mode"] == {
        "key": "Optional",
        "type": "enum",
        "values": {"a": None, "b": None},
        "default": "b",
    }
    assert "default" not in base["config_vars"]["mode"]


def test_inline_schema_lets_a_narrowed_enum_replace_the_base_values() -> None:
    """An explicitly overridden values map replaces the base's instead of merging with it."""
    base = {
        "config_vars": {
            "mode": {
                "key": "Optional",
                "type": "enum",
                "values": {"a": None, "b": None},
            }
        }
    }
    node = {"config_vars": {"mode": {"values": {"b": None}}}}
    _bls.inline_schema(node, base)

    assert node["config_vars"]["mode"] == {
        "key": "Optional",
        "type": "enum",
        "values": {"b": None},
    }


def test_inline_schema_overlays_nested_schema_fields() -> None:
    """A nested schema's config vars overlay per field too."""
    base = {
        "config_vars": {
            "outer": {
                "type": "schema",
                "schema": {
                    "config_vars": {"inner": {"key": "Optional", "type": "boolean"}}
                },
            }
        }
    }
    node = {
        "config_vars": {
            "outer": {"schema": {"config_vars": {"inner": {"default": "True"}}}}
        }
    }
    _bls.inline_schema(node, base)

    assert node["config_vars"]["outer"]["schema"]["config_vars"]["inner"] == {
        "key": "Optional",
        "type": "boolean",
        "default": "True",
    }


def test_shrink_enum_collapse_reports_dropped_siblings(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """Collapsing into an enum prints the sibling extends it cannot keep."""
    output = {
        "base": {"schemas": {"B": _schema({"b": {"key": "Optional"}})}},
        "mod": {
            "schemas": {
                "E": {"type": "enum", "values": {"a": None}},
                "CONFIG_SCHEMA": _schema({"mode": _schema(None, ["mod.E", "base.B"])}),
            }
        },
        "other": {"schemas": {"CONFIG_SCHEMA": _schema({}, ["base.B"])}},
    }
    _shrink(monkeypatch, output)

    mode = output["mod"]["schemas"]["CONFIG_SCHEMA"]["schema"]["config_vars"]["mode"]
    assert mode == {"type": "enum", "values": {"a": None}}
    assert "dropped ['base.B'] beside sibling extends at mod.schemas.CONFIG_SCHEMA" in (
        capsys.readouterr().out
    )
