"""Tests for the LVGL widget-child-ref feature: `id: {parent:, path:}` on
`lvgl.<type>.update` actions, used to update a widget that has no `id:` of its own.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import run_esphome

# Importing these widget modules registers their WidgetType singletons (and the
# lvgl.<type>.update actions that go with them) as a side effect, exactly as happens
# when the lvgl component is loaded normally.
from esphome.components.lvgl.schemas import (
    _REF_WIDGET_TYPE_KEY,
    CONF_PARENT,
    _update_id_item_schema,
    is_widget_child_ref,
)
from esphome.components.lvgl.widgets.dropdown import dropdown_spec
from esphome.components.lvgl.widgets.label import label_spec
from esphome.components.lvgl.widgets.spinbox import spinbox_spec
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PATH


def _id_schema_for(widget_type) -> cv.Schema:
    return cv.maybe_simple_value(
        {cv.Required(CONF_ID): cv.use_id(widget_type.w_type)}, key=CONF_ID
    )


# --- is_widget_child_ref -----------------------------------------------------------


def test_is_widget_child_ref_true_for_ref_dict() -> None:
    assert is_widget_child_ref({CONF_PARENT: "x", CONF_PATH: [0]}) is True


def test_is_widget_child_ref_false_for_plain_string() -> None:
    assert is_widget_child_ref("some_id") is False


def test_is_widget_child_ref_false_for_unrelated_dict() -> None:
    assert is_widget_child_ref({CONF_ID: "some_id"}) is False


# --- _update_id_item_schema ---------------------------------------------------------


def test_plain_id_passes_through_unchanged() -> None:
    item_schema = _update_id_item_schema(label_spec, _id_schema_for(label_spec))
    result = item_schema("my_label")
    assert result[CONF_ID].id == "my_label"
    assert not is_widget_child_ref(result[CONF_ID])


def test_ref_resolves_to_wrapped_dict() -> None:
    item_schema = _update_id_item_schema(label_spec, _id_schema_for(label_spec))
    result = item_schema({CONF_PARENT: "my_button", CONF_PATH: [1, 2]})
    ref = result[CONF_ID]
    assert is_widget_child_ref(ref)
    assert ref[CONF_PARENT].id == "my_button"
    assert ref[CONF_PATH] == [1, 2]
    assert ref[_REF_WIDGET_TYPE_KEY] == "label"


def test_ref_requires_non_empty_path() -> None:
    item_schema = _update_id_item_schema(label_spec, _id_schema_for(label_spec))
    with pytest.raises(cv.Invalid):
        item_schema({CONF_PARENT: "my_button", CONF_PATH: []})


def test_ref_rejected_for_compound_widget_type() -> None:
    item_schema = _update_id_item_schema(dropdown_spec, _id_schema_for(dropdown_spec))
    with pytest.raises(cv.Invalid, match="compound widget type 'dropdown'"):
        item_schema({CONF_PARENT: "my_obj", CONF_PATH: [0]})


def test_plain_id_still_works_for_compound_widget_type() -> None:
    item_schema = _update_id_item_schema(dropdown_spec, _id_schema_for(dropdown_spec))
    result = item_schema("my_dropdown")
    assert result[CONF_ID].id == "my_dropdown"


def test_ref_rejected_for_spinbox() -> None:
    item_schema = _update_id_item_schema(spinbox_spec, _id_schema_for(spinbox_spec))
    with pytest.raises(cv.Invalid, match="widget type 'spinbox'"):
        item_schema({CONF_PARENT: "my_obj", CONF_PATH: [0]})


# --- Code generation ------------------------------------------------------------
#
# These build a minimal host+LVGL config, generate its C++ (via `esphome compile
# --only-generate`, which skips the actual platformio/toolchain compile step), and
# check the generated source directly -- much faster than a full component test build,
# and precise about the exact codegen shape.

_CONFIG_TEMPLATE = """\
esphome:
  name: gentest
host:
logger:
display:
  - platform: sdl
    id: sdl0
    # Avoids shelling out to `sdl2-config`, which isn't installed on the
    # unit-test runners -- codegen never actually compiles this config.
    sdl_options: "-DESPHOME_TEST"
    dimensions:
      width: 100
      height: 100
lvgl:
  displays: sdl0
  widgets:
    - button:
        id: my_button
        widgets:
          - label:
              text: "First"
          - label:
              text: "Second"
    - label:
        id: plain_label
        text: "Plain"
  on_boot:
{actions}
"""


def _generate(tmp_path: Path, actions_yaml: str) -> str:
    yaml_path = tmp_path / "test.yaml"
    yaml_path.write_text(
        _CONFIG_TEMPLATE.format(actions=actions_yaml), encoding="utf-8"
    )
    assert run_esphome(["esphome", "compile", "--only-generate", str(yaml_path)]) == 0
    main_cpp = tmp_path / ".esphome" / "build" / "gentest" / "src" / "main.cpp"
    return main_cpp.read_text(encoding="utf-8")


def test_codegen_single_level_path(tmp_path: Path) -> None:
    content = _generate(
        tmp_path,
        """\
    - lvgl.label.update:
        id:
          parent: my_button
          path: [0]
        text: "Updated"
""",
    )
    assert "lvgl::lv_obj_get_child_by_path(my_button, {0})" in content
    # Guarded against the reference resolving to null at runtime.
    assert "if (dyn_label_VAR_)" in content


def test_codegen_nested_path_uses_null_safe_helper(tmp_path: Path) -> None:
    content = _generate(
        tmp_path,
        """\
    - lvgl.label.update:
        id:
          parent: my_button
          path: [0, 1]
        text: "Updated"
""",
    )
    assert "lvgl::lv_obj_get_child_by_path(my_button, {0, 1})" in content
    assert "lv_obj_get_child(lv_obj_get_child(" not in content


def test_codegen_mixed_plain_id_and_ref_in_same_action(tmp_path: Path) -> None:
    content = _generate(
        tmp_path,
        """\
    - lvgl.label.update:
        id:
          - plain_label
          - parent: my_button
            path: [1]
        text: "Updated"
""",
    )
    assert "lv_label_set_text(plain_label," in content
    assert "lvgl::lv_obj_get_child_by_path(my_button, {1})" in content


def test_config_validates_with_widget_ref(tmp_path: Path) -> None:
    """Regression test: resolving a widget-child-ref used to crash LVGL's
    final-validate pass (`global_config.get_path_for_id` expects a real declared ID,
    not the ref's dict), for *every* update action using one, whether or not the
    target widget type even defines its own `final_validate`.
    """
    yaml_path = tmp_path / "test.yaml"
    yaml_path.write_text(
        _CONFIG_TEMPLATE.format(
            actions="""\
    - lvgl.label.update:
        id:
          parent: my_button
          path: [0]
        text: "Updated"
"""
        ),
        encoding="utf-8",
    )
    assert run_esphome(["esphome", "config", str(yaml_path)]) == 0
