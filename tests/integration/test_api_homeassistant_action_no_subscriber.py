"""Integration test for Home Assistant actions fired without a subscriber.

Home Assistant subscribes to device actions shortly after authenticating, while
on_client_connected (and similar triggers) fire right at authentication. Actions
fired before any client has subscribed cannot be delivered - they must produce a
warning in the log instead of vanishing silently.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, HomeassistantServiceCall
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_homeassistant_action_no_subscriber(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Undeliverable actions warn in the log; actions after subscribing arrive."""
    loop = asyncio.get_running_loop()

    boot_warning_future = loop.create_future()
    connected_warning_future = loop.create_future()
    button_action_future = loop.create_future()

    def check_output(line: str) -> None:
        if (
            not boot_warning_future.done()
            and "Home Assistant action 'test.boot_action' dropped; no client connected"
            in line
        ):
            boot_warning_future.set_result(True)
        if (
            not connected_warning_future.done()
            and "Home Assistant action 'test.connected_action' dropped; "
            "client has not subscribed to actions (yet)"
            in line
        ):
            connected_warning_future.set_result(True)

    service_calls: list[HomeassistantServiceCall] = []

    def on_service_call(service_call: HomeassistantServiceCall) -> None:
        service_calls.append(service_call)
        if (
            service_call.service == "test.button_action"
            and not button_action_future.done()
        ):
            button_action_future.set_result(service_call)

    async with run_compiled(yaml_config, line_callback=check_output):
        # The on_boot action fires with no client connected at all.
        await asyncio.wait_for(boot_warning_future, timeout=10.0)

        async with api_client_connected() as client:
            device_info = await client.device_info()
            assert device_info.name == "test-ha-action-no-subscriber"

            # on_client_connected fired at authentication, before this client
            # subscribed to Home Assistant actions.
            await asyncio.wait_for(connected_warning_future, timeout=5.0)

            # After subscribing, actions must be delivered normally (and the
            # dropped ones must not suddenly show up).
            client.subscribe_service_calls(on_service_call)

            entities, _ = await client.list_entities_services()
            button = next(e for e in entities if isinstance(e, ButtonInfo))
            client.button_command(button.key)

            button_call = await asyncio.wait_for(button_action_future, timeout=5.0)
            assert button_call.data == {"value": "subscribed"}
            assert [call.service for call in service_calls] == ["test.button_action"]
