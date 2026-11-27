"""Integration tests for encoders and keypads without a group of their own on several LVGL instances.

They share the pages fixture, whose paged instance has the encoder that the pages tests drive.
"""

from __future__ import annotations

import pytest

from .lvgl_input_utils import assert_fields, booted_device
from .types import APIClientConnectedFactory, RunCompiledFunction

REPORTS = ("focus", "instances")


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_instances(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each instance's inputs move only within that instance, and an input with a group keeps it."""
    pressed_fields = {
        "encoder_right": "encoder_pressed",
        "plain_next": "plain_pressed",
        "plain_enter": "plain_pressed",
        "grouped_next": "grouped_pressed",
    }
    async with booted_device(
        yaml_config, run_compiled, api_client_connected, REPORTS, pressed_fields
    ) as device:
        await device.run(
            "report",
            encoder="top_button",
            plain="plain_button",
            grouped="grouped_button1",
        )
        fields = await device.move("plain_next", "plain", "plain_button2")
        assert_fields(
            fields,
            "after plain next",
            encoder="top_button",
            grouped="grouped_button1",
        )
        fields = await device.move("encoder_right", "encoder", "bottom_button")
        assert_fields(fields, "after encoder right", plain="plain_button2")
        await device.run(
            "focus_next_plain", plain="plain_button", encoder="bottom_button"
        )
        fields = await device.press("plain_enter", "plain_button")
        assert_fields(fields, "after plain enter", encoder_pressed=0)
        await device.release("plain_enter")

        # A msgbox on one instance does not move another instance's inputs or an input with its own group
        await device.run("show_close_msgbox", close_msgbox_hidden=0)
        await device.run("focus_close_ok", encoder="close_ok")
        await device.move("plain_next", "plain", "plain_button2")
        fields = await device.move(
            "grouped_next", "grouped", "grouped_button2", "grouped_button1"
        )
        assert_fields(fields, "after grouped next", encoder="close_ok")


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_keypad(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A keypad that is its instance's only input reaches only the current page's widgets, never a hidden
    page's or the replaced screen's."""
    async with booted_device(
        yaml_config,
        run_compiled,
        api_client_connected,
        REPORTS,
        {"keypad_next": "keypad_pressed"},
    ) as device:
        await device.run("report", keypad_page=0, keypad="keypad_a_button")
        await device.move(
            "keypad_next", "keypad", "keypad_a_button2", "keypad_a_button"
        )
        await device.run("next_keypad_page", keypad_page=1, keypad="keypad_b_button")
        await device.move(
            "keypad_next", "keypad", "keypad_b_button2", "keypad_b_button"
        )
