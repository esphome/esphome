"""Integration tests for list items added at run time on an LVGL instance whose encoder has no group of its own.

lvgl.list.add puts its item in the group of the page or msgbox the list is shown in, also before LVGL's setup().
They use the list_lvgl instance of the pages fixture. Each test boots its own device. Most blocks start with
the instance's reset action, which hides the msgboxes, shows page A and focuses its first button.
"""

from __future__ import annotations

from contextlib import AbstractAsyncContextManager

import pytest

from .lvgl_input_utils import Device, booted_device
from .types import APIClientConnectedFactory, RunCompiledFunction


def boot(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> AbstractAsyncContextManager[Device]:
    return booted_device(
        yaml_config,
        run_compiled,
        api_client_connected,
        ("list",),
        prefix="list_",
    )


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_list_items(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """List items added at run time join the group of their list's page or msgbox, not of what is shown, and
    leave LVGL's default group on the group the inputs are on."""
    async with boot(yaml_config, run_compiled, api_client_connected) as device:
        # on_boot adds a list item before LVGL's setup(), while the group of a msgbox is the default group. The
        # item goes into the group of page B.
        await device.run("reset", page=0, encoder="page_a_button")
        await device.run("show_page_b", page=1, encoder="page_b_button")
        await device.move("encoder_right", "encoder", "boot_item")

        # A list item added at run time joins the group of the page it is on, not of what is shown, so the
        # encoder reaches the items added on page A only there
        await device.run("reset", page=0)
        await device.run("show_page_b", page=1, encoder="boot_item")
        await device.run("add_list_item", default_is_input_group=1)
        await device.run("show_msgbox", encoder="msgbox_ok")
        await device.run("add_list_item", encoder="msgbox_ok", default_is_input_group=1)
        await device.run("hide_msgbox", encoder="boot_item")
        await device.move(
            "encoder_right",
            "encoder",
            "top_button",
            "bottom_button",
            "page_b_button",
            "boot_item",
        )
        await device.run("next_page", page=0, encoder="page_a_button")
        await device.move(
            "encoder_right",
            "encoder",
            "page_a_button2",
            "page_a_item1",
            "page_a_item2",
            "top_button",
            "bottom_button",
            "page_a_button",
        )

        # Items added at run time to a list in a msgbox join the msgbox's group, not the shown page's
        await device.run("reset", page=0)
        await device.run("show_page_b", page=1, encoder="boot_item")
        await device.run("add_msgbox_list_item", encoder="boot_item")
        await device.run("add_msgbox_list_item", encoder="boot_item")
        await device.run("show_list_msgbox", encoder="msgbox_item1")
        await device.move("encoder_right", "encoder", "msgbox_item2", "msgbox_item1")


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_input_groups_pages")
async def test_lvgl_input_groups_layer_list(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Items added to a list on a layer join the shown page's group and move with the layers."""
    async with boot(yaml_config, run_compiled, api_client_connected) as device:
        await device.run("reset", page=0, encoder="page_a_button")
        await device.run("add_top_list_item", encoder="page_a_button")
        await device.move(
            "encoder_right",
            "encoder",
            "page_a_button2",
            "top_button",
            "bottom_button",
            "top_item",
            "page_a_button",
        )
        await device.run("show_page_b", page=1, encoder="page_b_button")
        await device.move(
            "encoder_right",
            "encoder",
            "boot_item",
            "top_button",
            "bottom_button",
            "top_item",
            "page_b_button",
        )
