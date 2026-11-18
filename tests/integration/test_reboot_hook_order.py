"""Test the order of the shutdown lifecycle hooks on a forced and on a safe reboot."""

import re

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

HOOK_PATTERN = re.compile(r"HOOK (\w+) (\w+)")
# Hooks run in reverse component order, so "first" gets each hook last.
LAST_HOOK = "HOOK first on_powerdown"


async def _record_reboot_hooks(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> list[tuple[str, str]]:
    """Connect, which makes the device reboot, and return its (component, hook) calls in order."""
    waiter = LineWaiter()
    hooks: list[tuple[str, str]] = []

    def on_line(line: str) -> None:
        if match := HOOK_PATTERN.search(line):
            hooks.append((match[1], match[2]))
        waiter.callback(line)

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected(),
    ):
        await wait_or_fail(
            waiter.wait_for(LAST_HOOK),
            5.0,
            "Device did not run the powerdown hooks",
            hooks=hooks,
        )
    return hooks


@pytest.mark.asyncio
async def test_reboot_hook_order_forced(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A forced reboot runs on_shutdown then on_powerdown, with no safe shutdown or teardown."""
    hooks = await _record_reboot_hooks(yaml_config, run_compiled, api_client_connected)

    assert hooks == [
        ("second", "on_shutdown"),
        ("first", "on_shutdown"),
        ("second", "on_powerdown"),
        ("first", "on_powerdown"),
    ]


@pytest.mark.asyncio
async def test_reboot_hook_order_safe(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A safe reboot powers down only after every component has finished its teardown."""
    hooks = await _record_reboot_hooks(yaml_config, run_compiled, api_client_connected)

    assert hooks == [
        ("second", "on_safe_shutdown"),
        ("first", "on_safe_shutdown"),
        ("second", "on_shutdown"),
        ("first", "on_shutdown"),
        ("second", "teardown_pending"),
        ("first", "teardown_pending"),
        ("second", "teardown_done"),
        ("first", "teardown_done"),
        ("second", "on_powerdown"),
        ("first", "on_powerdown"),
    ]
