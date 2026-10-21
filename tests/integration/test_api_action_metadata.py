"""Integration test for user-defined action field metadata."""

from __future__ import annotations

import pytest

from esphome.helpers import fnv1_hash

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_action_metadata(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Action and argument metadata reach the client and the actions still run."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()

        by_name = {service.name: service for service in services}
        assert set(by_name) == {"play_buzzer", "plain_action"}
        # Keys are hashed at codegen time and must match what the client expects
        for name, service in by_name.items():
            assert service.key == fnv1_hash(name), name

        buzzer = by_name["play_buzzer"]
        assert buzzer.description == "Play an RTTTL melody on the buzzer"
        args = {arg.name: arg for arg in buzzer.args}
        assert args["song_str"].description == "RTTTL melody string"
        assert args["song_str"].example == "two_short:d=4,o=5,b=100:16e6,16e6"
        # An arg without metadata sends empty strings
        assert args["volume"].description == ""
        assert args["volume"].example == ""

        # An action without metadata sends empty strings
        plain = by_name["plain_action"]
        assert plain.description == ""
        assert plain.args[0].description == ""

        await client.execute_service(
            buzzer, {"song_str": "two_short:d=4,o=5,b=100:16e6,16e6", "volume": 3}
        )
        await client.execute_service(plain, {"value": 1})
        await waiter.wait_for("Buzzer: two_short", timeout=5.0)
        await waiter.wait_for("Plain action called", timeout=5.0)
