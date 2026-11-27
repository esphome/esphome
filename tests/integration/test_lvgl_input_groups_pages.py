"""Integration tests for an encoder without a group of its own on an LVGL instance with pages and msgboxes.

Such an input follows the topmost visible msgbox, else the current page, and top and bottom layer widgets
move with the page into its group. Each test boots its own device. Most blocks start with the fixture's
reset action, which hides the msgboxes, shows page A and focuses its first button. It leaves page B's
focus and the counters as they were.
"""

from __future__ import annotations

import pytest

from .lvgl_input_utils import assert_fields, booted_device
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_pages(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The encoder cycles through the current page's widgets and the layer widgets."""
    async with booted_device(yaml_config, run_compiled, api_client_connected) as device:
        # initial_focus on a top layer widget stays when setup() moves the widget into the group of the first page
        await device.run("report", page=0, encoder="top_button")
        await device.run("focus_top_button", page=0, encoder="top_button")
        # A full cycle never reaches another page's widgets, a hidden widget or the widgets on the
        # screen that the pages replace
        await device.move(
            "encoder_right",
            "encoder",
            "bottom_button",
            "page_a_button",
            "page_a_button2",
            "top_button",
        )
        await device.move("encoder_left", "encoder", "page_a_button2", "page_a_button")
        await device.move("encoder_right", "encoder", "page_a_button2", "top_button")
        # Focus on a layer widget is kept across a page change
        await device.run("next_page", page=1, encoder="top_button")
        await device.move(
            "encoder_right",
            "encoder",
            "bottom_button",
            "page_b_button",
            "page_b_button2",
            "top_button",
        )

        # Each page keeps its own focus across a visit to another page
        await device.run("reset", page=0, encoder="page_a_button")
        await device.move("encoder_right", "encoder", "page_a_button2")
        await device.run("show_page_b", page=1)
        await device.run("focus_page_b_button", encoder="page_b_button")
        await device.run("next_page", page=0, encoder="page_a_button2")
        await device.run("next_page", page=1, encoder="page_b_button")

        # When a focused layer widget leaves a page's group, LVGL focuses the widget before it there, and
        # the page shows that focus on its next visit
        await device.run("reset", page=0)
        await device.run("show_page_b", page=1)
        await device.run("focus_top_button", encoder="top_button")
        await device.run("reset", page=0, encoder="page_a_button")
        await device.run("next_page", page=1, encoder="page_b_button2")

        # page.previous moves the layer widgets, and the focus on one of them, to the page that it shows
        await device.run("reset", page=0)
        await device.run("show_page_b", page=1)
        await device.run("focus_top_button", encoder="top_button")
        await device.run("previous_page", page=0, encoder="top_button")
        await device.move(
            "encoder_right",
            "encoder",
            "bottom_button",
            "page_a_button",
            "page_a_button2",
            "top_button",
        )


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_msgboxes(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The inputs follow the topmost visible msgbox and return to the page when none is shown."""
    async with booted_device(yaml_config, run_compiled, api_client_connected) as device:
        # Focus cycles within an open msgbox, and its close button returns the inputs to the page
        await device.run("reset", page=0, encoder="page_a_button")
        await device.run("show_close_msgbox", close_msgbox_hidden=0)
        await device.run("focus_close_ok", encoder="close_ok")
        await device.run("focus_next", encoder="close_cancel")
        await device.move(
            "encoder_right", "encoder", "close_button", "close_ok", "close_cancel"
        )
        await device.move("encoder_right", "encoder", "close_button")
        await device.press("encoder_enter")
        fields = await device.release("encoder_enter")
        assert_fields(
            fields,
            "after clicking the close button",
            close_msgbox_hidden=1,
            encoder="page_a_button",
            page_a_button_pressed=0,
            page_a_button_clicks=0,
        )
        await device.move("encoder_right", "encoder", "page_a_button2")

        # When a msgbox closes, the page group gets back the focus that it had, not its first widget
        await device.run("reset", page=0)
        await device.run("show_page_b", page=1)
        await device.run("focus_top_button", encoder="top_button")
        await device.run("show_msgbox")
        await device.run("focus_msgbox_ok", encoder="msgbox_ok")
        await device.run("hide_msgbox", encoder="top_button")

        # A page change while a msgbox is open leaves the inputs on the msgbox
        await device.run("reset", page=0)
        await device.run("show_close_msgbox", close_msgbox_hidden=0)
        await device.run("focus_close_ok", encoder="close_ok")
        await device.run("next_page", page=1, encoder="close_ok")
        await device.move("encoder_right", "encoder", "close_cancel")

        # A msgbox with nothing that can take focus, and a restack of two shown msgboxes
        await device.run("reset", page=0)
        await device.run("show_msgbox")
        await device.run("focus_msgbox_ok", encoder="msgbox_ok")
        await device.run("show_empty_msgbox", encoder="none")
        await device.run("hide_empty_msgbox", encoder="msgbox_ok")
        await device.run("show_empty_msgbox", encoder="none")
        await device.run("raise_msgbox", encoder="msgbox_ok")
        await device.run("lower_msgbox", encoder="none")
        await device.run("hide_empty_msgbox", encoder="msgbox_ok")
        await device.run("hide_msgbox", encoder="page_a_button")


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_initial_focus(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A focus that on_boot moves away from the initial_focus of an encoder is kept, and initial_focus on a later page
    waits there for the page.

    The later page is a regression guard: a single group already kept that focus before each page had a group.
    """
    async with booted_device(
        yaml_config, run_compiled, api_client_connected, ("initial",), {}
    ) as device:
        await device.run(
            "report", later_page=0, later="later_a_button", boot="boot_a_button3"
        )
        await device.run("next_later_page", later_page=1, later="later_b_button2")


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_menu_sidebar(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The back button LVGL creates when lvgl.menu.set_page first gives a menu a sidebar joins LVGL's default
    group, which is the group the inputs are on, so the encoder reaches it.

    It uses the menu_lvgl instance, whose reset action hides the msgbox, shows page A with the menu and focuses
    the menu item.
    """
    async with booted_device(
        yaml_config, run_compiled, api_client_connected, ("menu",), prefix="menu_"
    ) as device:
        # After the test shows another page and a msgbox, the group of page A is the default again when page A shows
        await device.run("reset", page=0, encoder="item")
        await device.run("show_page_b", page=1, encoder="page_b_button")
        await device.run("show_page_a", page=0, encoder="item")
        await device.run("show_msgbox")
        await device.run("hide_msgbox", encoder="item")
        await device.run("show_sidebar", encoder="item")
        await device.move(
            "encoder_right", "encoder", "nav_button", "sidebar_back_button", "item"
        )
