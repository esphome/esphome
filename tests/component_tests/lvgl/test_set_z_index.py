"""Tests for the ``lvgl.widget.set_z_index`` action: schema validation and
code generation.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.components.lvgl.automation import SET_Z_INDEX_SCHEMA
from esphome.config_validation import Invalid

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------


class TestSetZIndexSchemaValidation:
    """Test that SET_Z_INDEX_SCHEMA accepts the documented forms and rejects
    everything else.
    """

    @pytest.mark.parametrize("position", ["top", "bottom", "up", "down"])
    def test_keyword_position_accepted(self, position: str) -> None:
        config = SET_Z_INDEX_SCHEMA({"id": "my_widget", "position": position})
        assert config["position"] == position.upper()

    @pytest.mark.parametrize("position", ["Top", "BOTTOM", "Up", "dOwN"])
    def test_keyword_position_case_insensitive(self, position: str) -> None:
        config = SET_Z_INDEX_SCHEMA({"id": "my_widget", "position": position})
        assert config["position"] == position.upper()

    @pytest.mark.parametrize("position", [0, 1, 5, -1, -5])
    def test_integer_position_accepted(self, position: int) -> None:
        config = SET_Z_INDEX_SCHEMA({"id": "my_widget", "position": position})
        assert config["position"] == position

    def test_unknown_keyword_rejected(self) -> None:
        with pytest.raises(Invalid):
            SET_Z_INDEX_SCHEMA({"id": "my_widget", "position": "sideways"})

    def test_float_position_rejected(self) -> None:
        with pytest.raises(Invalid):
            SET_Z_INDEX_SCHEMA({"id": "my_widget", "position": 1.5})

    def test_missing_id_rejected(self) -> None:
        with pytest.raises(Invalid):
            SET_Z_INDEX_SCHEMA({"position": "top"})

    def test_missing_position_rejected(self) -> None:
        with pytest.raises(Invalid):
            SET_Z_INDEX_SCHEMA({"id": "my_widget"})

    def test_single_id_is_wrapped_in_list(self) -> None:
        config = SET_Z_INDEX_SCHEMA({"id": "my_widget", "position": "top"})
        assert len(config["id"]) == 1
        assert config["id"][0]["id"].id == "my_widget"

    def test_list_of_ids_accepted(self) -> None:
        config = SET_Z_INDEX_SCHEMA({"id": ["widget_a", "widget_b"], "position": "top"})
        assert [entry["id"].id for entry in config["id"]] == ["widget_a", "widget_b"]


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    """Generate the C++ output for the shared set_z_index YAML config once
    per module. See ``test_widget_state.py`` for why this is module-scoped
    and self-contained rather than using the function-scoped ``generate_main``
    fixture from ``conftest.py``.
    """
    from esphome.__main__ import generate_cpp_contents
    from esphome.config import read_config
    from esphome.core import CORE

    config_path = Path(request.fspath).parent / "config" / "set_z_index_test.yaml"
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_global_section + CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_top_emits_move_foreground(main_cpp: str) -> None:
    assert "lv_obj_move_foreground(label_a);" in main_cpp


def test_bottom_emits_move_background(main_cpp: str) -> None:
    assert "lv_obj_move_background(label_a);" in main_cpp


def test_up_emits_unguarded_index_increment(main_cpp: str) -> None:
    assert "lv_obj_move_to_index(label_a, lv_obj_get_index(label_a) + 1);" in main_cpp


def test_down_emits_guarded_index_decrement(main_cpp: str) -> None:
    """``down`` must be guarded so that a widget already at index 0 isn't
    reinterpreted by LVGL as "move to the top" (LVGL treats a negative
    index as "count from the back").
    """
    assert "if (lv_obj_get_index(label_a) > 0) {" in main_cpp
    assert "lv_obj_move_to_index(label_a, lv_obj_get_index(label_a) - 1);" in main_cpp


def test_positive_integer_emits_direct_index(main_cpp: str) -> None:
    assert "lv_obj_move_to_index(label_a, 3);" in main_cpp


def test_negative_integer_emits_direct_index(main_cpp: str) -> None:
    assert "lv_obj_move_to_index(label_a, -2);" in main_cpp


def test_list_of_ids_applies_to_each_widget(main_cpp: str) -> None:
    """``id: [label_a, label_b]`` must emit the move call once per widget."""
    assert (
        main_cpp.count("lv_obj_move_to_index(label_a, lv_obj_get_index(label_a) + 1);")
        == 2
    )
    assert "lv_obj_move_to_index(label_b, lv_obj_get_index(label_b) + 1);" in main_cpp
