"""Test the homeassistant select, text and button platforms against a fake Home Assistant."""

from __future__ import annotations

import asyncio
from collections.abc import Awaitable, Callable

from aioesphomeapi import APIClient, HomeassistantServiceCall, UserService
import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction

SUBSCRIPTIONS = {
    ("select.mode", "options"),
    ("select.mode", None),
    ("input_select.colour", "options"),
    ("input_select.colour", None),
    ("text.note", None),
    ("text.note", "min"),
    ("text.note", "max"),
    ("text.note", "mode"),
    ("input_text.name", None),
    ("input_text.name", "min"),
    ("input_text.name", "max"),
    ("input_text.name", "mode"),
}

# Each is rejected by the options parser, leaving the previous options in place
MALFORMED_OPTIONS = (
    "None",
    "",
    "[",
    "['a'",
    "['a',",
    "['a' 'b']",
    "['a'] x",
    "[1]",
    "['a\\q']",
    "['a\\",
    "['\\x4']",
    "['\\x0']",
    "['\\x00']",
    "['\\U00110000']",
)


@pytest.mark.asyncio
async def test_api_homeassistant_entities(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Options, state and actions of the homeassistant select, text and button platforms."""
    loop = asyncio.get_running_loop()
    waiter = LineWaiter()
    subscribed: set[tuple[str, str | None]] = set()
    all_subscribed = loop.create_future()
    calls: asyncio.Queue[HomeassistantServiceCall] = asyncio.Queue()

    def on_state_sub(entity_id: str, attribute: str | None) -> None:
        subscribed.add((entity_id, attribute or None))
        if not all_subscribed.done() and subscribed >= SUBSCRIPTIONS:
            all_subscribed.set_result(None)

    async def next_call() -> HomeassistantServiceCall:
        return await asyncio.wait_for(calls.get(), timeout=5.0)

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, services = await client.list_entities_services()
        keys = {entity.name: entity.key for entity in entities}
        dump_select = next(s for s in services if s.name == "dump_select")
        client.subscribe_service_calls(calls.put_nowait)
        client.subscribe_home_assistant_states(on_state_sub)
        try:
            await asyncio.wait_for(all_subscribed, timeout=5.0)
        except TimeoutError:
            pytest.fail(f"never subscribed: {SUBSCRIPTIONS - subscribed}")

        await _check_select(client, waiter, keys, dump_select, next_call)
        await _check_text(client, waiter, keys, next_call)

        client.button_command(keys["HA Button"])
        call = await next_call()
        assert call.service == "button.press"
        assert call.data == {"entity_id": "button.doorbell"}
        client.button_command(keys["HA Input Button"])
        call = await next_call()
        assert call.service == "input_button.press"
        assert call.data == {"entity_id": "input_button.reset"}

        assert calls.empty()


async def _check_select(
    client: APIClient,
    waiter: LineWaiter,
    keys: dict[str, int],
    dump_select: UserService,
    next_call: Callable[[], Awaitable[HomeassistantServiceCall]],
) -> None:
    async def dump(tag: str) -> str:
        await client.execute_service(dump_select, {"tag": tag})
        return await waiter.wait_for(f"dump {tag}:", timeout=5.0)

    # Built with str() exactly as Home Assistant does, so every escape it emits is covered
    options = ["Low", "It's", "café\t\x7f\\", "\u200b\U000e0001\r\n'\""]
    client.send_home_assistant_state("select.mode", "options", str(options))
    await waiter.wait_for("'select.mode': Got 4 options", timeout=5.0)

    client.send_home_assistant_state("select.mode", "", "It's")
    await waiter.wait_for("select value 'It's' at 1", timeout=5.0)
    assert "has_state=1 index=1 option='It's'" in await dump("first")

    # Decoded options round trip through select.select_option unchanged
    for option in options[2:]:
        client.select_command(keys["HA Select"], option)
        call = await next_call()
        assert call.service == "select.select_option"
        assert call.data == {"entity_id": "select.mode", "option": option}

    # The active option keeps its state when it moves; double quoted options parse too
    client.send_home_assistant_state("select.mode", "options", "[\"It's\", 'L\\\"ow']")
    await waiter.wait_for("'select.mode': Got 2 options", timeout=5.0)
    assert "has_state=1 index=0 option='It's'" in await dump("moved")
    client.select_command(keys["HA Select"], 'L"ow')
    assert (await next_call()).data["option"] == 'L"ow'

    # Dropping the active option clears the state
    client.send_home_assistant_state(
        "select.mode", "options", "[ 'Low' , 'Max' , 'High' ]"
    )
    await waiter.wait_for("'select.mode': Got 3 options", timeout=5.0)
    assert "has_state=0" in await dump("dropped")

    client.send_home_assistant_state("select.mode", "", "unavailable")
    await waiter.wait_for("State 'unavailable' is not one of the options", timeout=5.0)

    # Rejected lists keep the previous options
    client.send_home_assistant_state("select.mode", "options", str(list("abcde")))
    await waiter.wait_for("5 options exceed max_options (4)", timeout=5.0)
    client.send_home_assistant_state("select.mode", "options", str(["x" * 50]))
    await waiter.wait_for(
        "Options need 51 bytes, more than options_buffer_size (40)", timeout=5.0
    )
    for malformed in MALFORMED_OPTIONS:
        client.send_home_assistant_state("select.mode", "options", malformed)
    client.send_home_assistant_state("select.mode", "", "Max")
    await waiter.wait_for("select value 'Max' at 1", timeout=5.0)
    assert sum("Can't parse options" in line for line in waiter.lines) == len(
        MALFORMED_OPTIONS
    )

    # Repeating the active state does not publish again
    client.send_home_assistant_state("select.mode", "", "Max")
    client.send_home_assistant_state("select.mode", "options", "[]")
    await waiter.wait_for("'select.mode': Got 0 options", timeout=5.0)
    assert sum("select value 'Max'" in line for line in waiter.lines) == 1
    assert "has_state=0 index=-1" in await dump("empty")

    client.send_home_assistant_state(
        "input_select.colour", "options", "['Red', 'Green']"
    )
    await waiter.wait_for("'input_select.colour': Got 2 options", timeout=5.0)
    client.select_command(keys["HA Input Select"], "Green")
    call = await next_call()
    assert call.service == "input_select.select_option"
    assert call.data == {"entity_id": "input_select.colour", "option": "Green"}


async def _check_text(
    client: APIClient,
    waiter: LineWaiter,
    keys: dict[str, int],
    next_call: Callable[[], Awaitable[HomeassistantServiceCall]],
) -> None:
    client.send_home_assistant_state("text.note", "min", "2")
    client.send_home_assistant_state("text.note", "max", "10")
    client.send_home_assistant_state("text.note", "mode", "password")
    await waiter.wait_for("'text.note': Mode retrieved: password", timeout=5.0)

    client.send_home_assistant_state("input_text.name", "min", "abc")
    client.send_home_assistant_state("input_text.name", "max", "x")
    client.send_home_assistant_state("input_text.name", "mode", "fancy")
    await waiter.wait_for(
        "'input_text.name': Unknown 'mode' value 'fancy'", timeout=5.0
    )
    await waiter.wait_for("Can't convert 'min' value 'abc'", timeout=5.0)
    await waiter.wait_for("Can't convert 'max' value 'x'", timeout=5.0)
    client.send_home_assistant_state("input_text.name", "mode", "text")
    await waiter.wait_for("'input_text.name': Mode retrieved: text", timeout=5.0)

    client.send_home_assistant_state("text.note", "", "hello")
    client.send_home_assistant_state("text.note", "", "hello")
    client.send_home_assistant_state("text.note", "", "there")
    await waiter.wait_for("text value 'there'", timeout=5.0)
    assert sum("text value 'hello'" in line for line in waiter.lines) == 1

    # The limits from Home Assistant apply: too short and too long are refused
    client.text_command(keys["HA Text"], "a")
    client.text_command(keys["HA Text"], "abcdefghijk")
    client.text_command(keys["HA Text"], "fine")
    call = await next_call()
    assert call.service == "text.set_value"
    assert call.data == {"entity_id": "text.note", "value": "fine"}

    client.text_command(keys["HA Input Text"], "Bob")
    call = await next_call()
    assert call.service == "input_text.set_value"
    assert call.data == {"entity_id": "input_text.name", "value": "Bob"}
