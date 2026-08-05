"""Tests for the LVGL ``list`` widget: schema validation for its actions
(``lvgl.list.add_text``/``add``/``remove``/``clear``) and the code they generate.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.components.lvgl.widgets.lv_list import (
    LIST_CREATE_SCHEMA,
    LIST_SCHEMA,
    list_add_schema,
)
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE

# ---------------------------------------------------------------------------
# lvgl.list.add schema: id + optional index + exactly one widget-type key
# ---------------------------------------------------------------------------


class TestListAddSchema:
    def test_valid_single_widget(self) -> None:
        result = list_add_schema({"id": "my_list", "label": {"text": "hi"}})
        assert result["id"].id == "my_list"
        assert "widget" in result

    def test_index_optional_and_templatable(self) -> None:
        result = list_add_schema({"id": "my_list", "index": 2, "label": {"text": "hi"}})
        assert result["index"] == 2

    def test_index_omitted_when_not_given(self) -> None:
        result = list_add_schema({"id": "my_list", "label": {"text": "hi"}})
        assert "index" not in result

    def test_missing_id_rejected(self) -> None:
        with pytest.raises(cv.Invalid, match="required key 'id' not provided"):
            list_add_schema({"label": {"text": "hi"}})

    def test_no_widget_key_rejected(self) -> None:
        with pytest.raises(cv.Invalid, match="exactly one widget definition"):
            list_add_schema({"id": "my_list"})

    def test_two_widget_keys_rejected(self) -> None:
        with pytest.raises(cv.Invalid, match="exactly one widget definition"):
            list_add_schema(
                {
                    "id": "my_list",
                    "label": {"text": "a"},
                    "button": {"text": "b"},
                }
            )

    def test_non_mapping_rejected(self) -> None:
        with pytest.raises(cv.Invalid, match="Expected a mapping"):
            list_add_schema("not_a_mapping")

    def test_any_registered_widget_type_accepted(self) -> None:
        for widget_key, widget_conf in (
            ("checkbox", {"text": "Option"}),
            ("switch", {}),
            ("spinner", {}),
            ("obj", {}),
            ("dropdown", {"options": ["a", "b"]}),
        ):
            result = list_add_schema({"id": "my_list", widget_key: widget_conf})
            assert widget_key in result["widget"][0]

    @pytest.mark.parametrize(
        ("widget_key", "widget_conf"),
        [
            ("buttonmatrix", {"rows": [{"buttons": [{"text": "A"}]}]}),
            ("tabview", {"tabs": [{"name": "Tab1"}]}),
            ("tileview", {"tiles": [{"row": 0, "column": 0}]}),
            ("meter", {"scales": [{"range_from": 0, "range_to": 100}]}),
        ],
    )
    def test_dynamic_widget_unsupported_rejected(
        self, widget_key: str, widget_conf: dict
    ) -> None:
        """buttonmatrix/tabview/tileview all register their own child widgets into
        the global widget map from inside their to_code - fine for a widget built
        once at boot, but broken if lvgl.list.add re-enters that on every call.
        meter is rejected for a related but distinct reason: its scale/indicator
        objects are declared with cg.Pvariable(), which emits its assignment
        wherever code is currently being generated -- fine at the top level of a
        boot-time to_code, but lvgl.list.add's do_add runs inside a lambda, so that
        assignment would end up outside the very lambda that declares the local
        meter object it refers to, which doesn't compile.
        """
        with pytest.raises(cv.Invalid, match="cannot be used with lvgl.list.add"):
            list_add_schema({"id": "my_list", widget_key: widget_conf})

    def test_dynamic_widget_unsupported_rejected_when_nested(self) -> None:
        """The check must recurse into `widgets:` so a tabview hidden a few levels
        deep inside another widget is caught too, not just at the top level.
        """
        with pytest.raises(cv.Invalid, match="cannot be used with lvgl.list.add"):
            list_add_schema(
                {
                    "id": "my_list",
                    "obj": {"widgets": [{"tabview": {"tabs": [{"name": "Tab1"}]}}]},
                }
            )


# ---------------------------------------------------------------------------
# The list widget's own schema: pad_row is shared between create/update, but
# on_add/on_remove only make sense at creation time.
# ---------------------------------------------------------------------------


class TestListCreateVsModifySchema:
    def test_create_schema_has_pad_row_and_triggers(self) -> None:
        keys = {str(k) for k in LIST_CREATE_SCHEMA.schema}
        assert "pad_row" in keys
        assert "on_add" in keys
        assert "on_remove" in keys

    def test_modify_schema_has_pad_row_but_not_triggers(self) -> None:
        """``lvgl.list.update`` can change pad_row but can't (re-)declare triggers."""
        keys = {str(k) for k in LIST_SCHEMA.schema}
        assert "pad_row" in keys
        assert "on_add" not in keys
        assert "on_remove" not in keys

    def test_on_add_single_automation_with_multiple_actions(self) -> None:
        """A bare action list under on_add: is one automation with a multi-step
        `then:`, not multiple independent automations.
        """
        config = LIST_CREATE_SCHEMA({"on_add": [{"delay": "10ms"}, {"delay": "20ms"}]})
        assert len(config["on_add"]) == 1
        assert len(config["on_add"][0]["then"]) == 2

    def test_on_add_accepts_multiple_independent_automations(self) -> None:
        """Each explicit `then:` entry gets its own Trigger, so on_add can fire
        more than one independent automation.
        """
        config = LIST_CREATE_SCHEMA(
            {
                "on_add": [
                    {"then": [{"delay": "10ms"}]},
                    {"then": [{"delay": "20ms"}]},
                ]
            }
        )
        assert len(config["on_add"]) == 2


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    """Generate the C++ output for the shared list-widget YAML config once per
    module -- see test_widget_state.py for why this is module-scoped and
    inlines the generate_main fixture logic rather than depending on it.
    """
    config_path = Path(request.fspath).parent / "config" / "list_test.yaml"
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_global_section + CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_pad_row_set_at_creation(main_cpp: str) -> None:
    assert "lv_obj_set_style_pad_row(test_list, 4, LV_PART_MAIN);" in main_cpp


def test_pad_row_updated_via_update_action(main_cpp: str) -> None:
    assert "lv_obj_set_style_pad_row(test_list, 8, LV_PART_MAIN);" in main_cpp


def test_add_text_appends(main_cpp: str) -> None:
    assert 'lv_list_add_text(test_list, "Header");' in main_cpp


def test_add_text_with_index_moves_before_firing_on_add(main_cpp: str) -> None:
    """The index move must happen before on_add fires, so the reported
    list_index reflects the entry's final position, not where it was appended.
    """
    assert (
        'lv_obj_t *list_entry_VAR_ = lv_list_add_text(test_list, "Pinned");\n'
        "        lv_obj_move_to_index(list_entry_VAR_, 0);\n"
        "        triggerint_id->trigger(lvgl::lv_list_get_row_index(test_list, list_entry_VAR_));"
    ) in main_cpp


def test_add_button_with_checkable_flag(main_cpp: str) -> None:
    assert "lv_obj_t *dyn_button_VAR_ = lv_btn_create(test_list);" in main_cpp
    assert (
        "lv_obj_add_flag(dyn_button_VAR_, (lv_obj_flag_t)(LV_OBJ_FLAG_CHECKABLE));"
        in main_cpp
    )
    assert (
        'lv_label_set_text(lv_obj_get_child(dyn_button_VAR_, 0), "Entry");' in main_cpp
    )


def test_add_nested_hierarchy_with_compound_child(main_cpp: str) -> None:
    """`obj: {widgets: [label, dropdown]}` builds a plain label child and a
    heap-allocated (compound) dropdown child, both parented to the new row.
    """
    assert "lv_obj_t *dyn_obj_VAR_ = lv_obj_create(test_list);" in main_cpp
    assert (
        "lv_obj_t *dyn_label_VAR_ = lv_label_create(dyn_obj_VAR_);\n"
        "            lv_obj_add_style(dyn_label_VAR_, _lv_theme_style_label_main_default, "
        "(lv_state_t)(LV_PART_MAIN));\n"
        '            lv_label_set_text(dyn_label_VAR_, "Nested");'
    ) in main_cpp


def test_add_applies_theme_styles_to_dynamic_widget(main_cpp: str) -> None:
    """A widget added via lvgl.list.add must pick up the same `theme:` styling a
    statically-declared widget of the same type gets, not render unthemed.
    """
    assert (
        "lv_obj_add_style(dyn_label_VAR_, _lv_theme_style_label_main_default, "
        "(lv_state_t)(LV_PART_MAIN));"
    ) in main_cpp
    assert "LvDropdownType *dyn_dropdown_VAR_ = new LvDropdownType();" in main_cpp
    assert "lv_dropdown_create(dyn_obj_VAR_)" in main_cpp
    assert (
        "lvgl::delete_lv_compound_on_delete<LvDropdownType>, LV_EVENT_DELETE, "
        "dyn_dropdown_VAR_);"
    ) in main_cpp


def test_add_moves_row_to_given_index_before_firing_on_add(main_cpp: str) -> None:
    assert (
        "lv_obj_move_to_index(dyn_obj_VAR_, 1);\n"
        "        triggerint_id->trigger(lvgl::lv_list_get_row_index(test_list, dyn_obj_VAR_));"
    ) in main_cpp


def test_on_add_fires_once_per_entry_via_shared_trigger(main_cpp: str) -> None:
    """A single on_add: automation means a single Trigger instance, reused by
    every lvgl.list.add_text/add call site.
    """
    assert main_cpp.count("triggerint_id->trigger(lvgl::lv_list_get_row_index(") == 4


def test_remove_guards_against_missing_child_and_fires_before_delete(
    main_cpp: str,
) -> None:
    """The index is materialised into a local once (list_index_VAR_) and reused for
    both the child lookup and the on_remove trigger, so a templatable index isn't
    evaluated twice.
    """
    assert (
        "int list_index_VAR_ = 0;\n"
        "        {\n"
        "            lv_obj_t *list_child_VAR_ = lv_obj_get_child(test_list, list_index_VAR_);\n"
        "            if (list_child_VAR_) {\n"
        "                triggerint_id_2->trigger(list_index_VAR_);\n"
        "                lv_obj_del(list_child_VAR_);"
    ) in main_cpp


def test_clear_fires_on_remove_for_every_entry_then_cleans(main_cpp: str) -> None:
    assert (
        "for (int list_index = (int) (lv_obj_get_child_count(test_list)) - 1; "
        "list_index >= 0; list_index--) {\n"
        "        triggerint_id_2->trigger(list_index);\n"
        "    }\n"
        "    lv_obj_clean(test_list);"
    ) in main_cpp
