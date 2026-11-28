"""Helpers for the integration tests of LVGL input groups.

Each fixture includes fixtures/includes/lvgl_input_groups.yaml, whose `input` action drives the template binary
sensor named by its `button`, and logs where each input's focus is from a `report` script, as `key=value` fields
on lines that end their prefix with `_report`. Several LVGL instances share one fixture, so a test names its
instance's actions and buttons without the prefix that the fixture gives them.
"""

from __future__ import annotations

from collections.abc import AsyncIterator, Mapping
from contextlib import asynccontextmanager
import re

from aioesphomeapi import APIClient, UserService
import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction

ANSI_PATTERN = re.compile(r"\x1b\[[0-9;]*m")

# The report field that says whether LVGL has read the encoder of the fixtures' shared package as pressed
ENCODER_PRESSED_FIELDS = {
    "encoder_left": "encoder_pressed",
    "encoder_right": "encoder_pressed",
    "encoder_enter": "encoder_pressed",
}

SHARED_ACTIONS = ("report", "input")

# LVGL reads its inputs on a timer, so a press or release shows in a later report. One poll is one
# report round trip, far shorter than LVGL's read period, so this allows several read periods.
MAX_POLLS = 1000


def report_fields(line: str) -> dict[str, str]:
    """Split a report line into its key=value fields."""
    fields = ANSI_PATTERN.sub("", line).split("_report")[-1].split()
    return dict(field.split("=", 1) for field in fields if "=" in field)


def assert_fields(fields: dict[str, str], context: str, **expected: str | int) -> None:
    expected_fields = {key: str(value) for key, value in expected.items()}
    assert fields.items() >= expected_fields.items(), f"{context}: {fields}"


class Device:
    """Runs a fixture's actions, drives its inputs and reads back its reports."""

    def __init__(
        self,
        client: APIClient,
        services: list[UserService],
        waiter: LineWaiter,
        reports: tuple[str, ...],
        pressed_fields: Mapping[str, str],
        prefix: str,
    ) -> None:
        self.client = client
        self.services = services
        self.waiter = waiter
        self.reports = reports
        self.pressed_fields = pressed_fields
        self.prefix = prefix

    async def execute(self, action: str, **data: str | bool) -> None:
        name = action if action in SHARED_ACTIONS else self.prefix + action
        await self.client.execute_service(require_service(self.services, name), data)

    async def run(self, action: str, **expected: str | int) -> dict[str, str]:
        """Run an action that reports, and check the report holds `expected`."""
        lines = [self.waiter.expect(f"{report}_report ") for report in self.reports]
        await self.execute(action)
        fields: dict[str, str] = {}
        for line in lines:
            fields |= report_fields(
                await self.waiter.wait(line, msg=f"no report after {action}")
            )
        assert_fields(fields, f"after {action}", **expected)
        return fields

    async def input(self, button: str, pressed: bool) -> None:
        await self.execute("input", button=self.prefix + button, pressed=pressed)

    async def until_read(self, button: str, pressed: bool) -> dict[str, str]:
        """Report until LVGL has read the input of `button` in the given state."""
        field = self.pressed_fields[button]
        for _ in range(MAX_POLLS):
            if (fields := await self.run("report"))[field] == str(int(pressed)):
                return fields
        state = "press" if pressed else "release"
        pytest.fail(f"LVGL did not read the {state} of {button} in {MAX_POLLS} reports")

    async def press(
        self, button: str, pressed_widget: str | None = None
    ) -> dict[str, str]:
        """Press `button`, and report once LVGL has read it and `pressed_widget` logged its press."""
        if pressed_widget is None:
            await self.input(button, True)
        else:
            logged = self.waiter.expect(f"PRESSED {pressed_widget}")
            await self.input(button, True)
            await self.waiter.wait(logged, msg=f"{pressed_widget} not pressed")
        return await self.until_read(button, True)

    async def release(self, button: str) -> dict[str, str]:
        await self.input(button, False)
        return await self.until_read(button, False)

    async def move(self, button: str, field: str, *focus_path: str) -> dict[str, str]:
        """Press and release a focus key once for each widget the input's focus must move to in turn."""
        fields: dict[str, str] = {}
        for widget in focus_path:
            moved = self.waiter.expect("focus moved")
            await self.input(button, True)
            await self.waiter.wait(
                moved, msg=f"focus did not move on {button}, expected {widget}"
            )
            fields = await self.release(button)
            assert_fields(fields, f"{button} towards {widget}", **{field: widget})
        return fields


@asynccontextmanager
async def booted_device(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    reports: tuple[str, ...] = ("focus",),
    pressed_fields: Mapping[str, str] = ENCODER_PRESSED_FIELDS,
    prefix: str = "",
) -> AsyncIterator[Device]:
    """Boot the fixture and connect to it. `pressed_fields` maps each input button to the report field that
    says whether LVGL has read it as pressed. `prefix` is the fixture's prefix for this test's actions and
    buttons."""
    waiter = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        device = Device(client, services, waiter, reports, pressed_fields, prefix)
        # The first state of a binary sensor is its initial state and starts no callbacks. Without this, the
        # first press is lost.
        for button in pressed_fields:
            await device.input(button, False)
        yield device
