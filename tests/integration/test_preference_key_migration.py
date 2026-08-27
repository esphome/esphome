"""Integration test for entity preference key migration.

Entity keys are now the FNV-1 hash of the raw name instead of the sanitized
object_id (https://github.com/esphome/backlog/issues/85). On key-lookup
preference backends, make_entity_preference() must move data stored under the
old key to the new key, so devices keep their restored state after upgrading.

This test seeds the host preferences file the way a pre-migration firmware
would have written it and verifies:
1. Data stored under the OLD key is restored (migration happened, no data loss)
2. Data already stored under the NEW key is never overwritten by old data
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

DEVICE_NAME = "host-pref-key-migration"

# The pre-migration preference key was the sanitized object_id hash; the new
# key is the raw-name hash. All entities are on the main device (device_id 0)
# and their preferences use no version salt, so the key is just the hash.
SWITCH_OLD_KEY = fnv1_hash_object_id("Test Switch")
SWITCH_NEW_KEY = fnv1_hash_name("Test Switch")
NUMBER_OLD_KEY = fnv1_hash_object_id("Test Number")
NUMBER_NEW_KEY = fnv1_hash_name("Test Number")

# template_text salts its key with the length limits and pattern hash; this must
# match TemplateText::setup() in template_text.cpp (min_length 0, max_length 20,
# no pattern configured)
TEXT_KEY_EXTRA = (0 << 2) + (20 << 4) + (fnv1_hash("") << 6)
TEXT_OLD_KEY = (fnv1_hash_object_id("Test Text") + TEXT_KEY_EXTRA) & 0xFFFFFFFF
TEXT_NEW_KEY = (fnv1_hash_name("Test Text") + TEXT_KEY_EXTRA) & 0xFFFFFFFF

# TextSaver<20> stores a length-prefixed buffer of max_length + 1 bytes
TEXT_MAX_LENGTH = 20


def text_pref_payload(value: str) -> bytes:
    """Build the length-prefixed buffer TextSaver stores for a value."""
    data = value.encode("utf-8")
    assert len(data) <= TEXT_MAX_LENGTH
    return bytes([len(data)]) + data + b"\x00" * (TEXT_MAX_LENGTH - len(data))


@pytest.mark.asyncio
async def test_preference_key_migration(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> None:
    """Test that preferences stored under the old key survive the upgrade."""
    port, port_socket = reserved_tcp_port

    assert SWITCH_OLD_KEY != SWITCH_NEW_KEY
    assert NUMBER_OLD_KEY != NUMBER_NEW_KEY
    assert TEXT_OLD_KEY != TEXT_NEW_KEY

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
        # --- Run 1: only OLD keys present, as written by pre-migration firmware.
        # The restored states prove the data was migrated to the new keys.
        write_host_prefs(
            DEVICE_NAME,
            {
                SWITCH_OLD_KEY: b"\x01",  # bool: switch was ON
                NUMBER_OLD_KEY: struct.pack("<f", 42.5),
                TEXT_OLD_KEY: text_pref_payload("hello"),
            },
        )
        switch_state, number_state, text_state = await boot_and_get_initial_states()
        assert switch_state.state is True, (
            "Switch state stored under the old preference key was lost"
        )
        assert number_state.state == 42.5, (
            "Number value stored under the old preference key was lost"
        )
        assert text_state.state == "hello", (
            "Text value stored under the old preference key was lost"
        )

        # --- Run 2: both keys present with different values. The NEW key holds
        # the current data and must win; stale old-key data must never clobber it.
        write_host_prefs(
            DEVICE_NAME,
            {
                SWITCH_OLD_KEY: b"\x00",  # stale: OFF
                SWITCH_NEW_KEY: b"\x01",  # current: ON
                NUMBER_OLD_KEY: struct.pack("<f", 42.5),  # stale
                NUMBER_NEW_KEY: struct.pack("<f", 13.5),  # current
                TEXT_OLD_KEY: text_pref_payload("hello"),  # stale
                TEXT_NEW_KEY: text_pref_payload("world"),  # current
            },
        )
        switch_state, number_state, text_state = await boot_and_get_initial_states()
        assert switch_state.state is True, (
            "Stale old-key data overwrote the current new-key switch state"
        )
        assert number_state.state == 13.5, (
            "Stale old-key data overwrote the current new-key number value"
        )
        assert text_state.state == "world", (
            "Stale old-key data overwrote the current new-key text value"
        )
    finally:
        clear_host_prefs(DEVICE_NAME)
