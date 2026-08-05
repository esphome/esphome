"""Tests for the LVGL ``menu`` widget: schema validation for the ``menu``/
``menu_page``/``menu_section`` widget types and the ``lvgl.menu.set_page``
action, and the code they generate.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.automation import ACTION_REGISTRY
from esphome.components.lvgl.widgets.menu import MENU_PAGE_SCHEMA, MENU_SCHEMA
from esphome.config import load_config, read_config
import esphome.config_validation as cv
from esphome.core import CORE

# ---------------------------------------------------------------------------
# The menu widget's own schema
# ---------------------------------------------------------------------------


class TestMenuSchema:
    def test_pages_required(self) -> None:
        with pytest.raises(
            cv.Invalid, match=r"required key not provided @ data\['pages'\]"
        ):
            MENU_SCHEMA({"root_page": "main"})

    def test_root_page_required(self) -> None:
        with pytest.raises(
            cv.Invalid, match=r"required key not provided @ data\['root_page'\]"
        ):
            MENU_SCHEMA({"pages": [{"id": "main"}]})

    def test_sidebar_page_optional(self) -> None:
        config = MENU_SCHEMA({"pages": [{"id": "main"}], "root_page": "main"})
        assert "sidebar_page" not in config

    def test_header_mode_and_root_back_btn_defaults(self) -> None:
        config = MENU_SCHEMA({"pages": [{"id": "main"}], "root_page": "main"})
        assert config["header_mode"] == "LV_MENU_HEADER_TOP_FIXED"
        assert config["root_back_btn"] == "LV_MENU_ROOT_BACK_BUTTON_DISABLED"

    def test_header_mode_accepts_bare_choice(self) -> None:
        config = MENU_SCHEMA(
            {
                "pages": [{"id": "main"}],
                "root_page": "main",
                "header_mode": "bottom_fixed",
            }
        )
        assert config["header_mode"] == "LV_MENU_HEADER_BOTTOM_FIXED"


# ---------------------------------------------------------------------------
# A menu page's own schema (title optional, sections optional)
# ---------------------------------------------------------------------------


class TestMenuPageSchema:
    def test_empty_page_is_valid(self) -> None:
        config = MENU_PAGE_SCHEMA({})
        assert "title" not in config
        assert "sections" not in config

    def test_title_optional(self) -> None:
        config = MENU_PAGE_SCHEMA({"title": "Settings"})
        assert config["title"] == "Settings"

    def test_section_id_is_optional(self) -> None:
        """cv.GenerateID() means a section's id: is optional, unlike a page's."""
        config = MENU_PAGE_SCHEMA({"sections": [{}]})
        assert config["sections"][0]["id"].is_declaration


# ---------------------------------------------------------------------------
# lvgl.menu.set_page action schema
# ---------------------------------------------------------------------------


class TestMenuSetPageActionSchema:
    @property
    def schema(self):
        return ACTION_REGISTRY["lvgl.menu.set_page"].raw_schema

    def test_id_and_page_required(self) -> None:
        with pytest.raises(
            cv.Invalid, match=r"required key not provided @ data\['page'\]"
        ):
            self.schema({"id": "my_menu"})

    def test_sidebar_defaults_false(self) -> None:
        config = self.schema({"id": "my_menu", "page": "my_page"})
        assert config["sidebar"] is False

    def test_sidebar_can_be_set_true(self) -> None:
        config = self.schema({"id": "my_menu", "page": "my_page", "sidebar": True})
        assert config["sidebar"] is True


# ---------------------------------------------------------------------------
# Pages belong to exactly one menu
# ---------------------------------------------------------------------------

# Boilerplate around the widgets under test: an LVGL instance needs a display.
MENU_CONFIG_TEMPLATE = """
esphome:
  name: test

esp32:
  board: esp32dev
  framework:
    type: esp-idf

spi:
  - id: spi_bus
    clk_pin: GPIO18
    mosi_pin: GPIO23

display:
  - platform: mipi_spi
    spi_id: spi_bus
    model: st7789v
    id: tft_display
    dimensions:
      width: 240
      height: 320
    cs_pin: GPIO22
    dc_pin: GPIO21
    auto_clear_enabled: false
    invert_colors: false
    update_interval: never

lvgl:
  displays: tft_display
  widgets:
{widgets}
"""


def _config_errors(tmp_path: Path, widgets: str) -> list[str]:
    """Validate a config built from the given `widgets:` block and return the
    validation errors as text."""
    config_path = tmp_path / "menu.yaml"
    config_path.write_text(MENU_CONFIG_TEMPLATE.format(widgets=widgets))
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        return [str(err) for err in load_config({}).errors]
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_root_page_of_another_menu_is_rejected(tmp_path: Path) -> None:
    errors = _config_errors(
        tmp_path,
        """\
    - menu:
        id: menu_a
        root_page: menu_b_page
        pages:
          - id: menu_a_page
    - menu:
        id: menu_b
        root_page: menu_b_page
        pages:
          - id: menu_b_page
""",
    )
    assert any("'menu_b_page' is not a page of this menu" in err for err in errors)


def test_sidebar_page_of_another_menu_is_rejected(tmp_path: Path) -> None:
    errors = _config_errors(
        tmp_path,
        """\
    - menu:
        id: menu_a
        root_page: menu_a_page
        sidebar_page: menu_b_page
        pages:
          - id: menu_a_page
    - menu:
        id: menu_b
        root_page: menu_b_page
        pages:
          - id: menu_b_page
""",
    )
    assert any("'menu_b_page' is not a page of this menu" in err for err in errors)


def test_set_page_action_with_a_page_of_another_menu_is_rejected(
    tmp_path: Path,
) -> None:
    errors = _config_errors(
        tmp_path,
        """\
    - menu:
        id: menu_a
        root_page: menu_a_page
        pages:
          - id: menu_a_page
            widgets:
              - button:
                  text: "Go"
                  on_click:
                    - lvgl.menu.set_page:
                        id: menu_a
                        page: menu_b_page
    - menu:
        id: menu_b
        root_page: menu_b_page
        pages:
          - id: menu_b_page
""",
    )
    assert any(
        "page 'menu_b_page' is not a page of menu 'menu_a'" in err for err in errors
    )


def test_section_id_is_not_accepted_where_a_page_is_required(tmp_path: Path) -> None:
    """The page and section tag types are distinct, so a section id can't be
    passed off as a page."""
    errors = _config_errors(
        tmp_path,
        """\
    - menu:
        id: menu_a
        root_page: menu_a_section
        pages:
          - id: menu_a_page
            sections:
              - id: menu_a_section
""",
    )
    assert any(
        "ID 'menu_a_section' of type lv_menu_section_t doesn't inherit from "
        "lv_menu_page_t" in err
        for err in errors
    )


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    """Generate the C++ output for the shared menu-widget YAML config once per
    module -- see test_widget_state.py for why this is module-scoped and
    inlines the generate_main fixture logic rather than depending on it.
    """
    config_path = Path(request.fspath).parent / "config" / "menu_test.yaml"
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_global_section + CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_menu_created_with_header_and_back_button_modes(main_cpp: str) -> None:
    assert "test_menu = lv_menu_create(lvgl_id->get_screen_active());" in main_cpp
    assert "lv_menu_set_mode_header(test_menu, LV_MENU_HEADER_TOP_FIXED);" in main_cpp
    assert (
        "lv_menu_set_mode_root_back_button(test_menu, LV_MENU_ROOT_BACK_BUTTON_ENABLED);"
        in main_cpp
    )


def test_page_created_with_title(main_cpp: str) -> None:
    assert 'main_page = lv_menu_page_create(test_menu, "Settings");' in main_cpp
    assert 'advanced_page = lv_menu_page_create(test_menu, "Advanced");' in main_cpp


def test_section_created_under_its_page_not_the_menu(main_cpp: str) -> None:
    assert "wifi_section = lv_menu_section_create(main_page);" in main_cpp
    assert "wifi_switch = lv_switch_create(wifi_section);" in main_cpp


def test_ungrouped_widget_created_directly_under_page(main_cpp: str) -> None:
    assert "advanced_button = lv_btn_create(main_page);" in main_cpp


def test_root_page_and_sidebar_page_set_at_boot(main_cpp: str) -> None:
    assert "lv_menu_set_page(test_menu, main_page);" in main_cpp
    assert "lv_menu_set_sidebar_page(test_menu, nav_page);" in main_cpp


def test_header_style_applied_via_main_header_accessor(main_cpp: str) -> None:
    assert (
        "lv_obj_t *menu_header_VAR_ = lv_menu_get_main_header(test_menu);\n"
        "lv_obj_set_style_bg_color(menu_header_VAR_, lv_color_make(255, 0, 0), LV_PART_MAIN);"
    ) in main_cpp


def test_root_back_button_click_guarded_by_helper(main_cpp: str) -> None:
    """The on_root_back_button_click trigger must only fire when the guard
    helper (which wraps lv_menu_back_button_is_root) returns true.
    """
    assert (
        "lv_obj_add_event_cb(test_menu, [](lv_event_t * event) -> void {"
    ) in main_cpp
    assert (
        "if (lvgl::lv_menu_back_button_is_root_click(test_menu, event)) {\n"
        "        trigger_id->trigger();\n"
        "    }"
    ) in main_cpp
    assert "}, LV_EVENT_CLICKED, nullptr);" in main_cpp


def test_set_page_action_targets_main_by_default(main_cpp: str) -> None:
    assert "lv_menu_set_page(test_menu, advanced_page);" in main_cpp


def test_set_page_action_targets_sidebar_when_flagged(main_cpp: str) -> None:
    assert "lv_menu_set_sidebar_page(test_menu, main_page);" in main_cpp
