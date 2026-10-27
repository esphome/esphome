"""Integration test for LD2450 link loss detection with mock UART.

The fixture's mock radar sends a frame every 100 ms in the mode the test picks (silent, no targets, or a moving
target inside the polygon zone) and goes silent for 2 s when the component restarts it, as the real one does.
link_timeout is 1 s.

test_uart_mock_ld2450_link_loss:
  1. The boot restart's 2 s of silence does not count: the link is not reported lost, and comes up on the first frame
  2. An empty room with frames arriving for 3 s turns nothing unknown
  3. Silence in an empty room: presence, zone presence and counts turn unknown and the link sensor turns off; the
     first empty frame after the gap gives presence off again, although every presence filter last passed off
  4. A target in the zone, then silence: presence, zone presence, counts, target values and the direction turn unknown
     within the link timeout plus a second; settings keep their values and get no new state
  5. Frames with the target again: the link sensor turns on, and presence on and off, the counts, the target values
     and the direction (the same one as before) are published from the first frame
  6. Silence, then an empty room, with presence_timeout at 10 s: presence turns off on the first empty frame
  7. The restart button: the mock radar is silent for longer than the link timeout, and nothing turns unknown
  8. A baud rate change: no frames follow, and the link is lost once the grace window has run out
  9. One warning is logged per loss and one info line per restore

test_uart_mock_ld2450_link_loss_never_connected:
  A radar that never sends: the link sensor reads off from boot, the link is reported lost once, only after the boot
  restart's grace window and the link timeout, and the first frame brings it up
"""

from __future__ import annotations

import asyncio
import math
import re

from aioesphomeapi import (
    BinarySensorInfo,
    BinarySensorState,
    ButtonInfo,
    EntityInfo,
    EntityState,
    NumberInfo,
    NumberState,
    SelectInfo,
    SensorInfo,
    SensorState,
    SwitchInfo,
    TextInfo,
    TextSensorInfo,
    TextSensorState,
    TextState,
)
import pytest

from .host_prefs import clear_host_prefs
from .log_utils import LineWaiter
from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

DEVICE_NAME = "uart-mock-ld2450-link-loss"
ZONE_POLYGON = "-1000,800;0,800;0,1500;-1000,1500"
LINK_TIMEOUT_S = 1.0
PRESENCE_TIMEOUT_S = 10
# The boot restart opens a grace window that the info read inside it extends: about 1.5 s + 5 s after setup
BOOT_GRACE_S = 6.5
# The mock radar's restart silence, and a margin for the API round trip
RADAR_RESTART_S = 2.0
ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")

PRESENCE = ["has_target", "has_moving_target", "has_still_target", "hold_zone_presence"]
COUNTS = [
    "target_count",
    "still_target_count",
    "moving_target_count",
    "zone-1_all_target_count",
    "zone-1_still_target_count",
    "zone-1_moving_target_count",
]
TARGET_1 = [
    f"target-1_{name}"
    for name in ("x", "y", "speed", "angle", "distance", "resolution")
]
# Configuration and diagnostics: the radar's settings, which keep their values on link loss
SETTINGS = [
    ("bluetooth", SwitchInfo),
    ("multi_target_tracking", SwitchInfo),
    ("baud_rate", SelectInfo),
    ("zone_type", SelectInfo),
    ("presence_timeout", NumberInfo),
    ("zone-1_x1", NumberInfo),
    ("zone-1_y1", NumberInfo),
    ("zone-1_x2", NumberInfo),
    ("zone-1_y2", NumberInfo),
    ("hold_zone_polygon", TextInfo),
    ("firmware", TextSensorInfo),
    ("bt_mac", TextSensorInfo),
]


def is_unknown(state: EntityState) -> bool:
    """Whether Home Assistant would show this state as unknown."""
    if state.missing_state:
        return True
    if isinstance(state, SensorState):
        return math.isnan(state.state)
    return isinstance(state, TextSensorState) and state.state == "unknown"


def is_binary(key: int, value: bool):
    def predicate(state: EntityState) -> bool:
        return (
            isinstance(state, BinarySensorState)
            and state.key == key
            and not state.missing_state
            and state.state is value
        )

    return predicate


def is_sensor(key: int, value: float):
    def predicate(state: EntityState) -> bool:
        return (
            isinstance(state, SensorState)
            and state.key == key
            and not state.missing_state
            and state.state == pytest.approx(value)
        )

    return predicate


def is_text(key: int, value: str):
    def predicate(state: EntityState) -> bool:
        return (
            isinstance(state, TextSensorState)
            and state.key == key
            and not state.missing_state
            and state.state == value
        )

    return predicate


@pytest.mark.asyncio
async def test_uart_mock_ld2450_link_loss(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that live values go unknown on link loss and repopulate when frames return."""
    # The polygon and presence_timeout are saved, so a previous run would otherwise restore them
    clear_host_prefs(DEVICE_NAME)
    waiter = LineWaiter()
    frame_times: list[float] = []
    boot_times: list[float] = []

    def on_line(line: str) -> None:
        if "RX inject 30 bytes" in line:
            frame_times.append(asyncio.get_running_loop().time())
        elif "Mock UART initialized" in line:
            boot_times.append(asyncio.get_running_loop().time())
        waiter.callback(line)

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        def by_name(name: str, kind: type[EntityInfo]) -> EntityInfo:
            return require_entity(entities, name, kind, exact=True)

        connected = by_name("radar_link", BinarySensorInfo)
        presence = {name: by_name(name, BinarySensorInfo) for name in PRESENCE}
        counts = {name: by_name(name, SensorInfo) for name in COUNTS}
        target_1 = {name: by_name(name, SensorInfo) for name in TARGET_1}
        direction = by_name("target-1_direction", TextSensorInfo)
        settings = {name: by_name(name, kind) for name, kind in SETTINGS}
        polygon = settings["hold_zone_polygon"]
        timeout = settings["presence_timeout"]
        # An empty room already reads unknown on the per-target numbers, so they cannot show a link loss there
        watched_keys = {
            info.key for info in [*presence.values(), *counts.values(), direction]
        }
        settings_keys = {info.key for info in settings.values()}

        history: list[EntityState] = []
        latest: dict[int, EntityState] = {}

        def on_state(state: EntityState) -> None:
            history.append(state)
            latest[state.key] = state

        sub = await subscribe_after_initial_states(client, entities, on_state=on_state)
        latest.update(sub.initial_states)
        expect = sub.waiter.expect

        def press(name: str) -> None:
            client.button_command(by_name(name, ButtonInfo).key)

        async def settle() -> None:
            # Everything a pass publishes arrives together; let it all be processed
            await asyncio.sleep(0.3)

        def unknown_since(mark: int) -> list[EntityState]:
            """Watched values that turned unknown, and link sensor offs, in the states since `mark`."""
            return [
                state
                for state in history[mark:]
                if (state.key in watched_keys and is_unknown(state))
                or (
                    state.key == connected.key
                    and not state.missing_state
                    and state.state is False
                )
            ]

        def assert_unknown(*infos: EntityInfo) -> None:
            for info in infos:
                assert is_unknown(latest[info.key]), (
                    f"{info.object_id}: {latest[info.key]}"
                )

        async def lose_link() -> None:
            lost = expect(is_binary(connected.key, False), timeout=LINK_TIMEOUT_S + 1.0)
            press("mode_silent")
            await lost
            await settle()

        # 1. The boot restart: the mock radar is silent for 2 s, longer than the link timeout. The link sensor starts
        # off and turns on with the first frame.
        assert (
            sub.initial_states[connected.key].state is False
            or latest[connected.key].state
        )
        await wait_or_fail(
            _until(lambda: _is_on(latest.get(connected.key))),
            10.0,
            "the link sensor never turned on after boot",
            connected=lambda: latest.get(connected.key),
        )
        assert not any("Radar link lost" in line for line in waiter.lines)

        # The settings are read from the mock radar once the restart is over; give them a state to protect
        await wait_or_fail(
            _until(
                lambda: all(
                    key in latest and not latest[key].missing_state
                    for key in settings_keys - {timeout.key, polygon.key}
                )
            ),
            10.0,
            "the radar settings were not read",
            settings=lambda: {
                name: latest.get(info.key) for name, info in settings.items()
            },
        )
        polygon_set = expect(
            lambda s: isinstance(s, TextState) and s.key == polygon.key
        )
        client.text_command(polygon.key, ZONE_POLYGON)
        await polygon_set
        timeout_set = expect(
            lambda s: (
                isinstance(s, NumberState)
                and s.key == timeout.key
                and s.state == PRESENCE_TIMEOUT_S
            )
        )
        client.number_command(timeout.key, PRESENCE_TIMEOUT_S)
        await timeout_set
        await wait_or_fail(
            _until(
                lambda: (
                    all(
                        is_binary(info.key, False)(latest[info.key])
                        for info in presence.values()
                        if info.key in latest
                    )
                    and all(info.key in latest for info in presence.values())
                )
            ),
            5.0,
            "no presence state from the empty room",
            presence=lambda: {n: latest.get(i.key) for n, i in presence.items()},
        )

        # 2. An empty room with frames arriving: nothing turns unknown
        mark = len(history)
        await asyncio.sleep(3.0)
        assert unknown_since(mark) == []
        # The boot window is over; link loss is timed by the link timeout alone from here on
        await asyncio.sleep(
            max(
                0.0,
                boot_times[0] + BOOT_GRACE_S + 0.5 - asyncio.get_running_loop().time(),
            )
        )
        assert unknown_since(mark) == []

        # 3. Silence in an empty room, with every presence filter having last passed off
        await lose_link()
        assert_unknown(*presence.values(), *counts.values())
        presence_back = [
            expect(is_binary(info.key, False), timeout=2.0)
            for info in presence.values()
        ]
        link_back = expect(is_binary(connected.key, True), timeout=2.0)
        press("mode_empty")
        await asyncio.gather(link_back, *presence_back)

        # 4. A target in the zone: everything is populated, then the radar goes silent
        occupied = [
            expect(is_binary(presence[name].key, value), timeout=3.0)
            for name, value in [
                ("has_target", True),
                ("has_moving_target", True),
                ("hold_zone_presence", True),
            ]
        ]
        press("mode_moving_inside")
        await asyncio.gather(*occupied)
        await wait_or_fail(
            _until(
                lambda: (
                    is_sensor(target_1["target-1_x"].key, -500)(
                        latest[target_1["target-1_x"].key]
                    )
                    and is_sensor(counts["target_count"].key, 1)(
                        latest[counts["target_count"].key]
                    )
                    and is_sensor(counts["zone-1_all_target_count"].key, 1)(
                        latest[counts["zone-1_all_target_count"].key]
                    )
                    and is_text(direction.key, "Moving away")(latest[direction.key])
                )
            ),
            3.0,
            "the target was not published",
        )
        assert is_binary(presence["has_still_target"].key, False)(
            latest[presence["has_still_target"].key]
        )

        mark_loss = len(history)
        mark_loss_ms = asyncio.get_running_loop().time()
        await lose_link()
        assert (
            asyncio.get_running_loop().time() - mark_loss_ms
            < LINK_TIMEOUT_S + 1.0 + 0.3 + 0.1
        )
        assert_unknown(
            *presence.values(), *counts.values(), *target_1.values(), direction
        )
        assert latest[direction.key].state == "unknown"
        assert latest[connected.key].state is False
        # The first loss line says nothing arrived: no bytes, no invalid frames
        loss_lines = [line for line in waiter.lines if "Radar link lost" in line]
        assert re.search(
            r"no valid frame for \d+ ms \(0 bytes received, 0 invalid frames\)",
            ANSI_ESCAPE.sub("", loss_lines[0]),
        )

        # 5. The frames return with the same target: everything is published from the first frame
        back = [
            expect(is_binary(connected.key, True), timeout=2.0),
            expect(is_binary(presence["has_target"].key, True), timeout=2.0),
            expect(is_binary(presence["has_moving_target"].key, True), timeout=2.0),
            # Off before and after the gap: the filter has seen that value, but the sensor must not stay unknown
            expect(is_binary(presence["has_still_target"].key, False), timeout=2.0),
            expect(is_binary(presence["hold_zone_presence"].key, True), timeout=2.0),
            expect(is_sensor(target_1["target-1_x"].key, -500), timeout=2.0),
            expect(is_sensor(target_1["target-1_y"].key, 1000), timeout=2.0),
            expect(is_sensor(counts["target_count"].key, 1), timeout=2.0),
            expect(is_sensor(counts["moving_target_count"].key, 1), timeout=2.0),
            expect(is_sensor(counts["still_target_count"].key, 0), timeout=2.0),
            expect(is_sensor(counts["zone-1_all_target_count"].key, 1), timeout=2.0),
            expect(is_text(direction.key, "Moving away"), timeout=2.0),
        ]
        press("mode_moving_inside")
        await asyncio.gather(*back)

        # 6. Silence, then an empty room: presence_timeout is 10 s, but the hold does not outlive the gap
        await lose_link()
        began = asyncio.get_running_loop().time()
        cleared = [
            expect(is_binary(info.key, False), timeout=2.0)
            for info in presence.values()
        ]
        press("mode_empty")
        await asyncio.gather(*cleared)
        assert asyncio.get_running_loop().time() - began < PRESENCE_TIMEOUT_S / 2
        await wait_or_fail(
            _until(lambda: _is_on(latest.get(connected.key))),
            2.0,
            "the link sensor did not turn on",
        )

        # The settings were not touched by any of it
        touched = [s for s in history[mark_loss:] if s.key in settings_keys]
        assert touched == []

        # 7. The restart button: the mock radar goes silent for 2 s, twice the link timeout, and nothing turns unknown
        mark = len(history)
        pressed = asyncio.get_running_loop().time()
        client.button_command(by_name("radar_restart", ButtonInfo).key)
        await asyncio.sleep(RADAR_RESTART_S + 2.5)
        gaps = [
            later - earlier
            for earlier, later in zip(frame_times, frame_times[1:], strict=False)
            if later > pressed
        ]
        assert max(gaps) >= RADAR_RESTART_S - 0.3, "the mock radar did not go silent"
        assert unknown_since(mark) == []
        assert (
            is_binary(connected.key, True)(latest[connected.key])
            or latest[connected.key].state
        )

        # 8. A baud rate change: no frames follow, and the link is lost when the grace window has run out
        lost = expect(is_binary(connected.key, False), timeout=9.0)
        changed = asyncio.get_running_loop().time()
        client.select_command(settings["baud_rate"].key, "115200")
        await lost
        elapsed = asyncio.get_running_loop().time() - changed
        assert 4.5 <= elapsed <= 7.5, (
            f"link lost {elapsed:.1f} s after the baud rate change"
        )

        # 9. One warning per loss and one info line per restore
        text = [ANSI_ESCAPE.sub("", line) for line in waiter.lines]
        assert len([line for line in text if "Radar link lost" in line]) == 4
        assert len([line for line in text if "Radar link restored" in line]) == 3


@pytest.mark.asyncio
async def test_uart_mock_ld2450_link_loss_never_connected(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test a radar that never sends: the link sensor reads off and the loss is reported once."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        connected = require_entity(entities, "radar_link", BinarySensorInfo, exact=True)
        has_target = require_entity(
            entities, "has_target", BinarySensorInfo, exact=True
        )

        history: list[EntityState] = []
        sub = await subscribe_after_initial_states(
            client, entities, on_state=history.append
        )
        # Off from boot, before any frame
        assert sub.initial_states[connected.key].state is False
        assert not sub.initial_states[connected.key].missing_state
        assert sub.initial_states[has_target.key].missing_state

        # The boot restart's grace window, the info read inside it and the link timeout all have to pass first
        began = asyncio.get_running_loop().time()
        await waiter.wait_for("Radar link lost", timeout=15.0)
        assert asyncio.get_running_loop().time() - began >= 4.5
        await asyncio.sleep(3.0)
        text = [ANSI_ESCAPE.sub("", line) for line in waiter.lines]
        lost_lines = [line for line in text if "Radar link lost" in line]
        assert len(lost_lines) == 1
        assert not any("Radar link restored" in line for line in text)
        # Off before and after; the link sensor never published a change, and nothing else has a state
        assert history == []

        # The first frame brings the link up
        up = sub.waiter.expect(is_binary(connected.key, True), timeout=2.0)
        presence = sub.waiter.expect(is_binary(has_target.key, False), timeout=2.0)
        client.button_command(
            require_entity(entities, "send_frame", ButtonInfo, exact=True).key
        )
        await asyncio.gather(up, presence)
        await waiter.wait_for("Radar link restored", timeout=2.0)


async def _until(condition) -> None:
    while not condition():
        await asyncio.sleep(0.05)


def _is_on(state: EntityState | None) -> bool:
    return state is not None and not state.missing_state and state.state is True
