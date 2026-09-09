"""Integration test for entity preference key stability.

Entity preferences are stored under keys derived from the sanitized object_id
hash. This test seeds the host preferences file the way existing firmware
wrote it and verifies the state is restored, proving the key scheme has not
drifted; a save and reload round trip cannot catch drift because it writes
and reads with the same code.

The second run also seeds the raw-name-hash entries a 2026.8 beta device left
behind (see https://github.com/esphome/esphome/pull/18361) and proves they are
ignored: the object_id entries win and the beta leftovers are inert.
"""

from __future__ import annotations

import socket
import struct

from aioesphomeapi import (
    NumberInfo,
    NumberState,
    SwitchInfo,
    SwitchState,
    TextInfo,
    TextState,
)
import pytest

from esphome.helpers import fnv1_hash, fnv1_hash_name, fnv1_hash_object_id

from .conftest import run_binary_and_wait_for_port, wait_and_connect_api_client
from .host_prefs import clear_host_prefs, write_host_prefs
from .state_utils import InitialStateHelper, require_entity
from .types import CompileFunction, ConfigWriter

DEVICE_NAME = "host-pref-key-stability"

# All entities are on the main device (device_id 0) and their preferences use
# no version salt, so the key is just the object_id hash.
SWITCH_KEY = fnv1_hash_object_id("Test Switch")
NUMBER_KEY = fnv1_hash_object_id("Test Number")

# Raw-name-hash keys as written by 2026.8 beta firmware; never read by this build
SWITCH_BETA_KEY = fnv1_hash_name("Test Switch")
NUMBER_BETA_KEY = fnv1_hash_name("Test Number")

# template_text salts its key with the length limits and pattern hash; this must
# match TemplateText::setup() in template_text.cpp (min_length 0, max_length 20,
# no pattern configured)
TEXT_KEY_EXTRA = (0 << 2) + (20 << 4) + (fnv1_hash("") << 6)
TEXT_KEY = (fnv1_hash_object_id("Test Text") + TEXT_KEY_EXTRA) & 0xFFFFFFFF
TEXT_BETA_KEY = (fnv1_hash_name("Test Text") + TEXT_KEY_EXTRA) & 0xFFFFFFFF

# TextSaver<20> stores a length-prefixed buffer of max_length + 1 bytes
TEXT_MAX_LENGTH = 20


def text_pref_payload(value: str) -> bytes:
    """Build the length-prefixed buffer TextSaver stores for a value."""
    data = value.encode("utf-8")
    assert len(data) <= TEXT_MAX_LENGTH
    return bytes([len(data)]) + data + b"\x00" * (TEXT_MAX_LENGTH - len(data))


@pytest.mark.asyncio
async def test_preference_key_stability(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> None:
    """Test that preferences stored by earlier firmware are restored."""
    port, port_socket = reserved_tcp_port

    assert SWITCH_KEY != SWITCH_BETA_KEY
    assert NUMBER_KEY != NUMBER_BETA_KEY
    assert TEXT_KEY != TEXT_BETA_KEY

    # Write and compile once
    config_path = await write_yaml_config(yaml_config)
    binary_path = await compile_esphome(config_path)

    # Release the reserved port so the binary can bind to it
    port_socket.close()

    async def boot_and_get_initial_states() -> tuple[
        SwitchState, NumberState, TextState
    ]:
        """Boot the binary and return the restored entity states."""
        async with (
            run_binary_and_wait_for_port(binary_path, "127.0.0.1", port),
            wait_and_connect_api_client(port=port) as client,
        ):
            device_info = await client.device_info()
            assert device_info.name == DEVICE_NAME

            entities, _ = await client.list_entities_services()
            switch_entity = require_entity(
                entities, "test_switch", SwitchInfo, "Test Switch"
            )
            number_entity = require_entity(
                entities, "test_number", NumberInfo, "Test Number"
            )
            text_entity = require_entity(entities, "test_text", TextInfo, "Test Text")

            initial_state_helper = InitialStateHelper(entities)
            client.subscribe_states(
                initial_state_helper.on_state_wrapper(lambda s: None)
            )
            await initial_state_helper.wait_for_initial_states()

            switch_state = initial_state_helper.initial_states[switch_entity.key]
            number_state = initial_state_helper.initial_states[number_entity.key]
            text_state = initial_state_helper.initial_states[text_entity.key]
            assert isinstance(switch_state, SwitchState)
            assert isinstance(number_state, NumberState)
            assert isinstance(text_state, TextState)
            return switch_state, number_state, text_state

    try:
        # --- Run 1: entries under the object_id-hash keys, exactly as any
        # earlier firmware wrote them. The restored states prove the key
        # scheme has not drifted.
        write_host_prefs(
            DEVICE_NAME,
            {
                SWITCH_KEY: b"\x01",  # bool: switch was ON
                NUMBER_KEY: struct.pack("<f", 42.5),
                TEXT_KEY: text_pref_payload("hello"),
            },
        )
        switch_state, number_state, text_state = await boot_and_get_initial_states()
        assert switch_state.state is True, (
            "Switch state stored under the object_id preference key was lost"
        )
        assert number_state.state == 42.5, (
            "Number value stored under the object_id preference key was lost"
        )
        assert text_state.state == "hello", (
            "Text value stored under the object_id preference key was lost"
        )

        # --- Run 2: raw-name-hash entries from a 2026.8 beta device present
        # alongside the object_id entries. The object_id data must win; the
        # beta entries are never read.
        write_host_prefs(
            DEVICE_NAME,
            {
                SWITCH_KEY: b"\x01",  # current: ON
                SWITCH_BETA_KEY: b"\x00",  # beta leftover: OFF
                NUMBER_KEY: struct.pack("<f", 13.5),  # current
                NUMBER_BETA_KEY: struct.pack("<f", 99.5),  # beta leftover
                TEXT_KEY: text_pref_payload("world"),  # current
                TEXT_BETA_KEY: text_pref_payload("ignored"),  # beta leftover
            },
        )
        switch_state, number_state, text_state = await boot_and_get_initial_states()
        assert switch_state.state is True, (
            "Beta raw-name-key data overrode the object_id switch state"
        )
        assert number_state.state == 13.5, (
            "Beta raw-name-key data overrode the object_id number value"
        )
        assert text_state.state == "world", (
            "Beta raw-name-key data overrode the object_id text value"
        )
    finally:
        clear_host_prefs(DEVICE_NAME)
