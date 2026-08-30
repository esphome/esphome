"""Integration test for user-defined action names stored in flash and keys hashed at codegen."""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import UserServiceArgType
import pytest

from esphome.helpers import fnv1_hash

from .types import APIClientConnectedFactory, RunCompiledFunction

LONGEST_NAME = "a23456789_123456789_123456789_123456789_123456789_123456789_123"


@pytest.mark.asyncio
async def test_api_action_name_key(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Action names and keys survive the round trip through the string table and codegen hash."""
    loop = asyncio.get_running_loop()
    called: dict[str, asyncio.Future[bool]] = {
        name: loop.create_future()
        for name in ("short_action", "action_with_args", "longest_action")
    }
    patterns = {
        "short_action": re.compile(r"short_action called"),
        "action_with_args": re.compile(r"action_with_args called: 123, test_value"),
        "longest_action": re.compile(r"longest_action called"),
    }

    def check_output(line: str) -> None:
        for name, future in called.items():
            if not future.done() and patterns[name].search(line):
                future.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        by_name = {service.name: service for service in services}
        assert set(by_name) == {"short_action", "action_with_args", LONGEST_NAME}
        assert len(LONGEST_NAME) == 63

        # Keys are hashed at codegen time and must match the runtime hash the client expects
        for name, service in by_name.items():
            assert service.key == fnv1_hash(name), name

        args = {arg.name: arg.type for arg in by_name["action_with_args"].args}
        assert args == {
            "my_int": UserServiceArgType.INT,
            "my_string": UserServiceArgType.STRING,
        }

        await client.execute_service(by_name["short_action"], {})
        await client.execute_service(
            by_name["action_with_args"], {"my_int": 123, "my_string": "test_value"}
        )
        await client.execute_service(by_name[LONGEST_NAME], {})
        for future in called.values():
            await asyncio.wait_for(future, timeout=5.0)
