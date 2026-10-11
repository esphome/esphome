"""Integration tests for the LD2410S component with mock UART.

test_uart_mock_ld2410s:
  1. Init sequence - config-mode start, output-mode switch and config-mode end go
     out in order, each after the previous one is acknowledged, then the
     component logs "Setup done" and clears its warning
  2. Presence from short frames (states 0/1 off, 2/3 on) and from a standard
     data frame
  3. Resync - a noise byte or a partial header directly before a short frame
     does not hide that frame

test_uart_mock_ld2410s_recovery:
  1. A standard frame whose length field exceeds the receive buffer is dropped
  2. A truncated standard frame swallows the bytes after it up to its declared
     length, fails the footer check, and later frames parse again
  3. A command frame with a bad footer is dropped
  4. 100 bytes that never start a header are skipped
  5. A standard frame of the largest size that fits (86 byte payload) is accepted

test_uart_mock_ld2410s_no_ack:
  1. Config-mode start is resent after the acknowledgement timeout
  2. Once the retries run out, config-mode end is sent and the component warns
     that it is starting over
"""

from __future__ import annotations

import asyncio
from pathlib import Path
import re

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import InitialStateHelper, SensorStateCollector, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

CONFIG_START_TX = "FD:FC:FB:FA:04:00:FF:00:01:00:04:03:02:01"
OUTPUT_MODE_TX = "FD:FC:FB:FA:08:00:7A:00:00:00:00:00:00:00:04:03:02:01"
CONFIG_END_TX = "FD:FC:FB:FA:02:00:FE:00:04:03:02:01"

# has_target after each injection: short 2, short 0, standard frame state 2,
# short 1, noise + short 2, short 0, partial header + short 3
ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")

EXPECTED_PRESENCE = [True, False, True, False, True, False, True]


def _load_config(yaml_config: str) -> str:
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    return yaml_config.replace("EXTERNAL_COMPONENT_PATH", external_components_path)


def _tx_frame(line: str) -> str | None:
    """Return the hex payload of a uart_mock TX log line, or None."""
    if "uart_mock" not in line or "TX " not in line or "bytes: " not in line:
        return None
    return ANSI_ESCAPE.sub("", line.split("bytes: ", 1)[1]).strip()


@pytest.mark.asyncio
async def test_uart_mock_ld2410s(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test the LD2410S init sequence, presence parsing and receiver resync."""
    loop = asyncio.get_running_loop()
    tx_frames: list[str] = []
    setup_done = loop.create_future()
    warning_cleared = loop.create_future()

    def line_callback(line: str) -> None:
        if (frame := _tx_frame(line)) is not None:
            tx_frames.append(frame)
        if "[ld2410s" in line and "Setup done" in line and not setup_done.done():
            setup_done.set_result(True)
        if "cleared Warning flag" in line and not warning_cleared.done():
            warning_cleared.set_result(True)

    collector = SensorStateCollector(
        sensor_names=[], binary_sensor_names=["has_target"]
    )
    all_presence = collector.add_waiter(
        lambda: len(collector.binary_states["has_target"]) >= len(EXPECTED_PRESENCE)
    )

    async with (
        run_compiled(_load_config(yaml_config), line_callback=line_callback),
        api_client_connected() as client,
    ):
        try:
            await asyncio.wait_for(asyncio.gather(setup_done, warning_cleared), 5.0)
        except TimeoutError:
            pytest.fail(f"Init sequence did not finish. TX frames: {tx_frames}")

        assert tx_frames[:3] == [CONFIG_START_TX, OUTPUT_MODE_TX, CONFIG_END_TX]

        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        try:
            await asyncio.wait_for(all_presence, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for presence updates. Received: "
                f"{collector.binary_states['has_target']}"
            )

        assert collector.binary_states["has_target"] == EXPECTED_PRESENCE
        # Data frames must not restart the init sequence
        assert len(tx_frames) == 3, tx_frames


@pytest.mark.asyncio
async def test_uart_mock_ld2410s_no_ack(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that an unanswered command is resent, then the sequence gives up."""
    loop = asyncio.get_running_loop()
    tx_frames: list[tuple[float, str]] = []
    resent = loop.create_future()
    gave_up = loop.create_future()

    def line_callback(line: str) -> None:
        if (frame := _tx_frame(line)) is not None:
            tx_frames.append((loop.time(), frame))
        if "No acknowledgement for command 00FF, resending" in line and not (
            resent.done()
        ):
            resent.set_result(True)
        if (
            "No acknowledgement for command 00FF, starting over in 15 s" in line
            and not gave_up.done()
        ):
            gave_up.set_result(True)

    async with (
        run_compiled(_load_config(yaml_config), line_callback=line_callback),
        api_client_connected(),
    ):
        try:
            await asyncio.wait_for(asyncio.gather(resent, gave_up), 5.0)
        except TimeoutError:
            pytest.fail(f"No resend or give-up warning. TX frames: {tx_frames}")

        frames = [frame for _, frame in tx_frames]
        # First send plus INIT_MAX_TIMEOUTS (2) resends, then config-mode end
        assert frames == [CONFIG_START_TX] * 3 + [CONFIG_END_TX]
        # Resends follow the 300 ms acknowledgement timeout
        gaps = [b[0] - a[0] for a, b in zip(tx_frames, tx_frames[1:], strict=False)]
        assert all(0.2 <= gap <= 1.0 for gap in gaps), gaps


# has_target after each recovery phase (see uart_mock_ld2410s_recovery.yaml):
# baseline on, oversized length + off, truncated + absorbed frames (no change),
# on, bad command footer + off, junk + on, off, max-size frame on, off
EXPECTED_RECOVERY_PRESENCE = [True, False, True, False, True, False, True, False]
# The truncated frame takes the next 6 bytes to reach its declared length
TRUNCATED_FRAME_HEX = "F4 F3 F2 F1 05 00 01 02 2C 6E 02 2C 01 62 6E"
BAD_COMMAND_FRAME_HEX = "FD FC FB FA 04 00 FE 01 00 00 00 00 00 00"


@pytest.mark.asyncio
async def test_uart_mock_ld2410s_recovery(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that bad, truncated and oversized frames do not stop later frames."""
    loop = asyncio.get_running_loop()
    oversized_dropped = loop.create_future()
    footer_mismatches: list[str] = []

    def line_callback(line: str) -> None:
        if (
            "Dropping frame with a 255 byte payload, larger than the receive buffer"
            in line
            and not oversized_dropped.done()
        ):
            oversized_dropped.set_result(True)
        if "Footer does not match header: " in line:
            footer_mismatches.append(
                ANSI_ESCAPE.sub("", line.split("header: ", 1)[1]).strip()
            )

    collector = SensorStateCollector(
        sensor_names=[], binary_sensor_names=["has_target"]
    )
    all_presence = collector.add_waiter(
        lambda: (
            len(collector.binary_states["has_target"])
            >= len(EXPECTED_RECOVERY_PRESENCE)
        )
    )

    async with (
        run_compiled(_load_config(yaml_config), line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        try:
            await asyncio.wait_for(all_presence, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for presence updates. Received: "
                f"{collector.binary_states['has_target']}"
            )
        # Let any unexpected extra update arrive before comparing
        await asyncio.sleep(0.3)

        assert collector.binary_states["has_target"] == EXPECTED_RECOVERY_PRESENCE
        assert oversized_dropped.done(), "Oversized length field was not dropped"
        assert footer_mismatches == [TRUNCATED_FRAME_HEX, BAD_COMMAND_FRAME_HEX]
