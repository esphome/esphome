"""Integration test for user-defined action field metadata."""

from __future__ import annotations

import asyncio
import re

import pytest

from esphome.helpers import fnv1_hash

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_action_metadata(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Action and argument metadata reach the client and the actions still run."""
    loop = asyncio.get_running_loop()
    buzzer_called = loop.create_future()
    plain_called = loop.create_future()
    buzzer_pattern = re.compile(r"Buzzer: two_short")
    plain_pattern = re.compile(r"Plain action called")

    def check_output(line: str) -> None:
        if not buzzer_called.done() and buzzer_pattern.search(line):
            buzzer_called.set_result(True)
        elif not plain_called.done() and plain_pattern.search(line):
            plain_called.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
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
        await asyncio.wait_for(buzzer_called, timeout=5.0)
        await asyncio.wait_for(plain_called, timeout=5.0)
