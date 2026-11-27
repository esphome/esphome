"""Integration tests for the focus of an encoder without a group of its own as it changes group.

When such an input moves to another group, the focus there moves off a widget that is hidden or disabled,
a press held across the move ends unless its widget moved with it, and the focus shows the focus ring.
They use the change_lvgl instance of the pages fixture. Each test boots its own device. Most blocks start
with the instance's reset action, which hides the msgboxes and the widgets that only some steps show, enables
the widgets a step may disable, shows page A and focuses its first button. It leaves page B's focus and the
counters as they were.
"""

from __future__ import annotations

from contextlib import AbstractAsyncContextManager

import pytest

from .lvgl_input_utils import MAX_POLLS, Device, assert_fields, booted_device
from .types import APIClientConnectedFactory, RunCompiledFunction


def boot(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> AbstractAsyncContextManager[Device]:
    return booted_device(
        yaml_config, run_compiled, api_client_connected, ("change",), prefix="change_"
    )


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_focus_boot(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each group's first focus can take the focus and shows the focus ring when an input arrives."""
    async with boot(yaml_config, run_compiled, api_client_connected) as device:
        # initial_focus shows the focus ring although LVGL focused it before the encoder was on that group
        await device.run("report", page=0, encoder="top_button", encoder_focus_key=1)
        # LVGL gives the focus to the first widget of a page when the widget goes into the page group, before
        # it is hidden. The first visit to the page moves that focus to a widget that is not hidden.
        await device.run("reset", page=0, encoder="page_a_button")
        await device.run(
            "show_page_b", page=1, encoder="page_b_button", encoder_focus_key=1
        )
        # A msgbox's first widget took the focus as it was created, with no input on the msgbox's group
        await device.run("show_msgbox", encoder="msgbox_ok", encoder_focus_key=1)


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_edit_mode(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A group is not in edit mode after its focus or a layer widget leaves it."""
    async with boot(yaml_config, run_compiled, api_client_connected) as device:
        # A widget in edit mode that an action disabled while its page was not shown loses the focus and edit
        # mode. Thus the encoder moves the focus and does not edit the widget that gets the focus.
        await device.run("reset", page=0)
        await device.run("show_page_b", page=1)
        await device.run(
            "focus_slider_editing", encoder="edit_slider", encoder_editing=1
        )
        await device.run("next_page", page=0)
        await device.run("disable_slider")
        await device.run(
            "next_page", page=1, encoder="page_b_button2", encoder_editing=0
        )
        await device.move("encoder_right", "encoder", "top_button")

        # The same, but no other widget can take the focus. The disabled widget keeps the focus, but not edit mode.
        await device.run("reset", page=0)
        await device.run("show_slider_msgbox", encoder="msgbox_slider")
        await device.run(
            "focus_msgbox_slider_editing", encoder_editing=1, encoder_edited=1
        )
        await device.run("hide_slider_msgbox", encoder="page_a_button")
        await device.run("disable_msgbox_slider")
        await device.run(
            "show_slider_msgbox",
            encoder="msgbox_slider",
            encoder_editing=0,
            encoder_edited=0,
        )

        # A focused layer widget in edit mode keeps the focus on another page, but not edit mode
        await device.run("reset", page=0, encoder="page_a_button")
        await device.run("show_page_b", page=1)
        await device.run(
            "focus_top_slider_editing", encoder="top_slider", encoder_editing=1
        )
        await device.run(
            "next_page",
            page=0,
            encoder="top_slider",
            encoder_editing=0,
            encoder_edited=0,
        )
        # The old group is left out of edit mode, so the encoder moves its focus on the next visit
        await device.run("reset", page=0, encoder="page_a_button")
        await device.run(
            "show_page_b", page=1, encoder="page_b_button2", encoder_editing=0
        )
        await device.move("encoder_right", "encoder", "top_button")

        # A page in edit mode leaves it when a focused layer widget arrives there
        await device.run("reset", page=0)
        await device.run("focus_page_a_button2_editing", encoder_editing=1)
        await device.run("next_page", page=1)
        await device.run("focus_top_button", encoder="top_button")
        await device.run("next_page", page=0, encoder="top_button", encoder_editing=0)


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_held_press(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A press held across a change of the inputs' group ends unless its widget moved with it."""
    async with boot(yaml_config, run_compiled, api_client_connected) as device:
        # A press that opens a msgbox is not released onto the msgbox's button
        await device.run("reset", page=0)
        await device.run("show_page_b", page=1)
        await device.run("focus_page_b_button", encoder="page_b_button")
        fields = await device.press("encoder_enter", "page_b_button")
        assert_fields(fields, "after pressing page_b_button", encoder="msgbox_ok")
        fields = await device.release("encoder_enter")
        assert_fields(
            fields,
            "after releasing page_b_button",
            encoder="msgbox_ok",
            msgbox_ok_clicks=0,
            page_b_button_pressed=0,
        )

        # A press that closes a msgbox is not released onto the page's widget
        await device.run("reset", page=0)
        await device.run("show_page_b", page=1)
        await device.run("focus_page_b_button", encoder="page_b_button")
        await device.run("show_msgbox", encoder="msgbox_ok")
        fields = await device.press("encoder_enter", "msgbox_ok")
        assert_fields(fields, "after pressing msgbox_ok", encoder="page_b_button")
        fields = await device.release("encoder_enter")
        assert_fields(
            fields,
            "after releasing msgbox_ok",
            page_b_button_clicks=0,
            msgbox_ok_pressed=0,
        )

        # A press on a layer widget that changes the page continues in the group of the new page
        await device.run("reset", page=0)
        await device.run("focus_top_button", encoder="top_button")
        fields = await device.press("encoder_enter", "top_button")
        assert_fields(
            fields,
            "after pressing top_button",
            page=1,
            encoder="top_button",
            top_button_pressed=1,
        )
        fields = await device.release("encoder_enter")
        assert_fields(
            fields,
            "after releasing top_button",
            page=1,
            top_button_pressed=0,
            top_button_clicks=1,
            top_button_releases=1,
        )


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_pointer(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """With a pointer on the display, the focus shows the focus ring when the encoder moves to a page that an
    action shows. It does not show the ring when a pointer press shows the page. LVGL does the same for a focus
    that the pointer sets."""
    async with boot(yaml_config, run_compiled, api_client_connected) as device:
        # Page B gets its focus while no input is on its group and a pointer exists. Thus LVGL does not give it
        # the FOCUS_KEY state. Only the move of the encoder to page B can give it that state.
        await device.run("reset", page=0, encoder="page_a_button")
        await device.execute("attach_pointer")
        await device.run("focus_page_b_button")
        # The pointer presses top_button, which changes the page and reports
        await device.run(
            "press_pointer", page=1, encoder="page_b_button", encoder_focus_key=0
        )
        for _ in range(MAX_POLLS):
            if (await device.run("report"))["top_button_releases"] == "1":
                break
        else:
            pytest.fail(
                f"LVGL did not read the pointer's release in {MAX_POLLS} reports"
            )
        # The pointer gave the focus to top_button. An update that moves no input does not add the focus ring.
        await device.run(
            "hide_slider_msgbox", encoder="top_button", encoder_focus_key=0
        )

        await device.run("reset", page=0, encoder="page_a_button")
        await device.run("focus_page_b_button")
        await device.run(
            "show_page_b", page=1, encoder="page_b_button", encoder_focus_key=1
        )
