"""Integration test for user-defined action field metadata."""

from __future__ import annotations

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_action_metadata(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test action and argument metadata is sent to the client."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        _, services = await client.list_entities_services()

        services_by_name = {service.name: service for service in services}
        assert set(services_by_name) == {
            "play_buzzer",
            "plain_action",
            "optional_args_action",
        }

        buzzer = services_by_name["play_buzzer"]
        assert buzzer.description == "Play an RTTTL melody on the buzzer"
        args_by_name = {arg.name: arg for arg in buzzer.args}
        assert args_by_name["song_str"].description == "RTTTL melody string"
        assert args_by_name["song_str"].example == "two_short:d=4,o=5,b=100:16e6,16e6"
        # An arg without metadata sends empty strings
        assert args_by_name["volume"].description == ""
        assert args_by_name["volume"].example == ""

        # An action without metadata sends empty strings
        plain = services_by_name["plain_action"]
        assert plain.description == ""
        assert plain.args[0].description == ""
        assert plain.args[0].optional is False
        assert plain.args[0].default_value == ""

        optional = services_by_name["optional_args_action"]
        opt_args = {arg.name: arg for arg in optional.args}
        assert opt_args["song_str"].optional is True
        assert opt_args["song_str"].default_value == ""
        assert opt_args["octave"].optional is True
        assert opt_args["octave"].default_value == "5"
        assert opt_args["loud"].optional is True
        assert opt_args["loud"].default_value == "true"
