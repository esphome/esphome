"""Integration tests for the LD2410S component with mock UART.

test_uart_mock_ld2410s:
  1. Init sequence - config-mode start, version read, parameter read, output-mode
     switch and config-mode end go out in order, each after the previous one is
     acknowledged, then the component logs "Setup done" and clears its warning;
     the version and parameter answers are parsed and logged
  1b. Setting the max distance number to 7 m writes all six parameters (gate 10
     plus the values read at init) inside a configuration-mode session, and the
     number reports 7 m
  2. Presence from short frames (states 0/1 off, 2/3 on) and from a standard
     data frame
  3. Resync - a noise byte or a partial header directly before a short frame
     does not hide that frame
  4. A short frame and a standard frame whose state byte is above 3 are
     dropped without publishing, and the short frame after each still publishes

test_uart_mock_ld2410s_recovery (the mock never answers the init commands):
  1. Config-mode start is resent after the acknowledgement timeout
  2. Once the retries run out, config-mode end is sent and the component warns
     that it is starting over
  3. A standard frame whose length field exceeds the receive buffer is dropped
  4. A truncated standard frame swallows the bytes after it up to its declared
     length and fails the footer check; the swallowed bytes are rescanned, so
     the short frames hidden in them still publish
  5. A command frame with a bad footer is dropped
  6. 100 bytes that never start a header are skipped
  7. A standard frame of the largest size that fits (86 byte payload) is accepted

test_uart_mock_ld2410s_link_lost:
  1. Short frames arriving for longer than the 5 s link timeout keep the link up
  2. 5 s of silence logs "No data for 5 s (0 frames dropped), starting over", sets the warning flag
     and sends a new config-mode start
"""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import APIClient, ButtonInfo, EntityState, NumberInfo, NumberState
import pytest

from .state_utils import InitialStateHelper, SensorStateCollector, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

CONFIG_START_TX = "FD:FC:FB:FA:04:00:FF:00:01:00:04:03:02:01"
VERSION_READ_TX = "FD:FC:FB:FA:02:00:00:00:04:03:02:01"
PARAMETERS_READ_TX = (
    "FD:FC:FB:FA:0E:00:71:00:05:00:0A:00:06:00:02:00:0C:00:0B:00:04:03:02:01"
)
OUTPUT_MODE_TX = "FD:FC:FB:FA:08:00:7A:00:00:00:00:00:00:00:04:03:02:01"
CONFIG_END_TX = "FD:FC:FB:FA:02:00:FE:00:04:03:02:01"
INIT_TX = [
    CONFIG_START_TX,
    VERSION_READ_TX,
    # The fixture cuts the first answer short, so the read goes out again
    PARAMETERS_READ_TX,
    PARAMETERS_READ_TX,
    OUTPUT_MODE_TX,
    CONFIG_END_TX,
]
# All six parameters: max gate 10, min gate 0, no delay 10, both frequencies 80, speed 5
PARAMETERS_WRITE_TX = (
    "FD:FC:FB:FA:26:00:70:00:05:00:0A:00:00:00:0A:00:00:00:00:00:06:00:0A:00:00:00:02:00:50:00:00:00:"
    "0C:00:50:00:00:00:0B:00:05:00:00:00:04:03:02:01"
)
WRITE_TX = [CONFIG_START_TX, PARAMETERS_WRITE_TX, CONFIG_END_TX]
# Logged once the version and parameter answers are parsed
PARSED_ANSWERS = [
    "Firmware 1.2.3",
    "Parameter answer too short (4 bytes)",
    "Parameters: gates 0 to 16, no delay 10 s",
]

ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")

# has_target after each injection: short 2, short 0, standard frame state 2,
# short 1, noise + short 2, short 0, partial header + short 3,
# short 7 (dropped) + short 0, standard frame state 7 (dropped) + short 2
EXPECTED_PRESENCE = [True, False, True, False, True, False, True, False, True]
# The short frame and the standard frame with state 7 are logged as dropped
EXPECTED_DROPPED_STATES = ["7", "7"]

# has_target after each recovery phase (see uart_mock_ld2410s_recovery.yaml).
# The truncated frame swallows the state 2 short frame after it plus the header
# byte of the following state 0 short frame; the footer mismatch rescans those
# bytes, which publishes the swallowed frame and resumes the next one.
EXPECTED_RECOVERY_PRESENCE = [
    True,  # baseline short frame
    False,  # short frame after the dropped oversized length field
    True,  # rescued: short frame swallowed by the truncated standard frame
    False,  # short frame whose header byte completed the truncated frame
    True,  # short frame
    False,  # short frame after the command frame with a bad footer
    True,  # short frame after 100 junk bytes
    False,  # short frame
    True,  # maximum-size standard frame
    False,  # short frame
]
# The truncated frame takes the next 6 bytes to reach its declared length
TRUNCATED_FRAME_HEX = "F4 F3 F2 F1 05 00 01 02 2C 6E 02 2C 01 62 6E"
BAD_COMMAND_FRAME_HEX = "FD FC FB FA 04 00 FE 01 00 00 00 00 00 00"


def _tx_frame(line: str) -> str | None:
    """Return the hex payload of a uart_mock TX log line, or None."""
    if "uart_mock" not in line or "TX " not in line or "bytes: " not in line:
        return None
    return ANSI_ESCAPE.sub("", line.split("bytes: ", 1)[1]).strip()


async def _start_scenario(client: APIClient, collector: SensorStateCollector) -> None:
    """Subscribe the collector past the initial states, then start the mock scenario."""
    entities, _ = await client.list_entities_services()
    collector.build_key_mapping(entities)
    initial_state_helper = InitialStateHelper(entities)
    client.subscribe_states(initial_state_helper.on_state_wrapper(collector.on_state))
    await initial_state_helper.wait_for_initial_states()
    start_btn = require_entity(entities, "start_scenario", ButtonInfo)
    client.button_command(start_btn.key)


async def _wait_for_presence(
    collector: SensorStateCollector, waiter: asyncio.Future[bool]
) -> None:
    try:
        await asyncio.wait_for(waiter, timeout=5.0)
    except TimeoutError:
        pytest.fail(
            f"Timeout waiting for presence updates. Received: "
            f"{collector.binary_states['has_target']}"
        )


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
    dropped_states: list[str] = []
    parsed_answers: list[str] = []
    write_done = loop.create_future()

    def line_callback(line: str) -> None:
        if (frame := _tx_frame(line)) is not None:
            tx_frames.append(frame)
            if len(tx_frames) >= 9 and not write_done.done():
                write_done.set_result(True)
        if "[ld2410s" in line and "Setup done" in line and not setup_done.done():
            setup_done.set_result(True)
        parsed_answers.extend(answer for answer in PARSED_ANSWERS if answer in line)
        if "ld2410s cleared Warning flag" in line and not warning_cleared.done():
            warning_cleared.set_result(True)
        if "Dropping frame with impossible state " in line:
            dropped_states.append(
                ANSI_ESCAPE.sub("", line.split("state ", 1)[1]).strip()
            )

    collector = SensorStateCollector(
        sensor_names=["distance"], binary_sensor_names=["has_target"]
    )
    number_states: list[float] = []

    def on_state(state: EntityState) -> None:
        collector.on_state(state)
        if isinstance(state, NumberState) and state.key == max_distance_key:
            number_states.append(state.state)

    all_presence = collector.add_waiter(
        lambda: len(collector.binary_states["has_target"]) >= len(EXPECTED_PRESENCE)
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        try:
            await asyncio.wait_for(asyncio.gather(setup_done, warning_cleared), 5.0)
        except TimeoutError:
            pytest.fail(f"Init sequence did not finish. TX frames: {tx_frames}")

        assert tx_frames[:6] == INIT_TX
        assert parsed_answers == PARSED_ANSWERS

        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)
        max_distance_key = require_entity(entities, "max_distance", NumberInfo).key
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()
        # The values read from the module: gate 16 and 10 s
        no_delay_key = require_entity(entities, "no_delay", NumberInfo).key
        initial = initial_state_helper.initial_states
        assert initial[max_distance_key].state == pytest.approx(11.2)
        assert initial[no_delay_key].state == pytest.approx(10)

        client.number_command(max_distance_key, 7.0)
        try:
            await asyncio.wait_for(write_done, 5.0)
        except TimeoutError:
            pytest.fail(f"Parameter write not sent. TX frames: {tx_frames}")
        assert tx_frames[6:9] == WRITE_TX, tx_frames
        assert number_states == [pytest.approx(7.0)]

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)
        await _wait_for_presence(collector, all_presence)
        # Let any unexpected extra update arrive before comparing
        await asyncio.sleep(0.3)

        assert collector.binary_states["has_target"] == EXPECTED_PRESENCE
        assert dropped_states == EXPECTED_DROPPED_STATES
        # A target 300 cm away, then none: the distance follows the presence
        assert collector.sensor_states["distance"][:2] == [300, 0]
        # Data frames must not restart the init sequence
        assert len(tx_frames) == 9, tx_frames


@pytest.mark.asyncio
async def test_uart_mock_ld2410s_recovery(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test the unanswered-command retry, and that bad frames do not stop later ones."""
    loop = asyncio.get_running_loop()
    tx_frames: list[tuple[float, str]] = []
    resent = loop.create_future()
    gave_up = loop.create_future()
    oversized_dropped = loop.create_future()
    footer_mismatches: list[str] = []

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
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        await _start_scenario(client, collector)
        await _wait_for_presence(collector, all_presence)
        try:
            await asyncio.wait_for(asyncio.gather(resent, gave_up), 5.0)
        except TimeoutError:
            pytest.fail(f"No resend or give-up warning. TX frames: {tx_frames}")
        # Let any unexpected extra update arrive before comparing
        await asyncio.sleep(0.3)

        # Recovery shows in the published states alone; the footer log below is
        # only a cross-check that needs the fixture's VERBOSE logger level
        assert collector.binary_states["has_target"] == EXPECTED_RECOVERY_PRESENCE
        assert oversized_dropped.done(), "Oversized length field was not dropped"
        assert footer_mismatches == [TRUNCATED_FRAME_HEX, BAD_COMMAND_FRAME_HEX]

        frames = [frame for _, frame in tx_frames]
        # First send plus INIT_MAX_TIMEOUTS (2) resends, then config-mode end;
        # the restart after the 15 s pause is out of the test's time window
        assert frames == [CONFIG_START_TX] * 3 + [CONFIG_END_TX]
        # Resends follow the 300 ms acknowledgement timeout
        gaps = [b[0] - a[0] for a, b in zip(tx_frames, tx_frames[1:], strict=False)]
        assert all(0.2 <= gap <= 1.0 for gap in gaps), gaps


# has_target after each short frame in uart_mock_ld2410s_link_lost.yaml
EXPECTED_LINK_PRESENCE = [True, False, True, False, True, False, True]
LINK_TIMEOUT_S = 5.0


@pytest.mark.asyncio
async def test_uart_mock_ld2410s_link_lost(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that short frames keep the link alive and silence restarts the init."""
    loop = asyncio.get_running_loop()
    tx_frames: list[str] = []
    rx_injected_at: list[float] = []
    setup_done = loop.create_future()
    link_lost = loop.create_future()
    warning_set_after_loss = loop.create_future()
    restarted = loop.create_future()
    config_start_before_loss = 0

    def line_callback(line: str) -> None:
        nonlocal config_start_before_loss
        if (frame := _tx_frame(line)) is not None:
            tx_frames.append(frame)
            if frame == CONFIG_START_TX and link_lost.done() and not restarted.done():
                restarted.set_result(True)
        if "uart_mock" in line and "Injecting " in line and "(injection " in line:
            rx_injected_at.append(loop.time())
        if "[ld2410s" in line and "Setup done" in line and not setup_done.done():
            setup_done.set_result(True)
        if (
            "No data for 5 s (0 frames dropped), starting over" in line
            and not link_lost.done()
        ):
            config_start_before_loss = tx_frames.count(CONFIG_START_TX)
            link_lost.set_result(loop.time())
        if (
            link_lost.done()
            and "ld2410s set Warning flag" in line
            and not warning_set_after_loss.done()
        ):
            warning_set_after_loss.set_result(True)

    collector = SensorStateCollector(
        sensor_names=[], binary_sensor_names=["has_target"]
    )
    all_presence = collector.add_waiter(
        lambda: (
            len(collector.binary_states["has_target"]) >= len(EXPECTED_LINK_PRESENCE)
        )
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        try:
            await asyncio.wait_for(setup_done, 5.0)
        except TimeoutError:
            pytest.fail(f"Init sequence did not finish. TX frames: {tx_frames}")

        await _start_scenario(client, collector)
        try:
            await asyncio.wait_for(all_presence, timeout=10.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for presence updates. Received: "
                f"{collector.binary_states['has_target']}"
            )
        assert collector.binary_states["has_target"] == EXPECTED_LINK_PRESENCE

        try:
            lost_at = await asyncio.wait_for(link_lost, timeout=7.0)
            await asyncio.wait_for(
                asyncio.gather(warning_set_after_loss, restarted), timeout=1.0
            )
        except TimeoutError:
            pytest.fail(
                f"No link-lost warning, warning flag or restart. TX frames: {tx_frames}"
            )

        # The stream ran past the link timeout, so only the silence after it counts
        silence = lost_at - rx_injected_at[-1]
        assert silence >= LINK_TIMEOUT_S - 0.5, (
            f"Link declared lost {silence:.2f} s after the last frame"
        )

        # The init sequence starts over with a new config-mode start
        assert config_start_before_loss == 1, tx_frames
        assert tx_frames.count(CONFIG_START_TX) == 2, tx_frames
