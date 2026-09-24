"""Integration tests for the light transition_state_publish_interval option."""

from __future__ import annotations

import asyncio
from collections.abc import Callable
from itertools import pairwise

from aioesphomeapi import (
    APIClient,
    ButtonInfo,
    EntityInfo,
    EntityState,
    LightInfo,
    LightState,
)
import pytest

from .state_utils import InitialStateHelper, require_entity, wait_for_state
from .types import APIClientConnectedFactory, RunCompiledFunction

INTERVAL_FIXTURE = "light_transition_state_publish_interval"
SAVE_FIXTURE = "light_transition_interval_save"
SAVED_LINE = "Saving deferred preferences"

Timeline = list[tuple[float, LightState]]
DonePredicate = Callable[[float, LightState], bool]


class _Recorder:
    """Records the states one light publishes while an action runs.

    ``run`` fires ``action`` and returns the (elapsed, state) timeline once a
    published state satisfies ``done``, after ``settle`` more seconds so late
    publishes still land in the timeline.
    """

    def __init__(self) -> None:
        self._loop = asyncio.get_running_loop()
        self._event = asyncio.Event()
        self._key = 0
        self._start = 0.0
        self._done: DonePredicate | None = None
        self.timeline: Timeline = []

    def on_state(self, state: EntityState) -> None:
        if (
            self._done is None
            or not isinstance(state, LightState)
            or state.key != self._key
        ):
            return
        elapsed = self._loop.time() - self._start
        self.timeline.append((elapsed, state))
        if self._done(elapsed, state):
            self._event.set()

    async def run(
        self,
        key: int,
        action: Callable[[], None],
        done: DonePredicate,
        settle: float = 0.0,
    ) -> Timeline:
        self.timeline = []
        self._key = key
        self._done = done
        self._event.clear()
        self._start = self._loop.time()
        action()
        async with asyncio.timeout(5):
            await self._event.wait()
        if settle:
            await asyncio.sleep(settle)
        self._done = None
        return self.timeline


class _LogCounter:
    """Counts device log lines containing ``needle`` and lets a test await a count."""

    def __init__(self, needle: str) -> None:
        self.needle = needle
        self.count = 0
        self._changed = asyncio.Event()

    def on_line(self, line: str) -> None:
        if self.needle in line:
            self.count += 1
            self._changed.set()

    async def wait_for(self, count: int) -> None:
        async with asyncio.timeout(5):
            while self.count < count:
                self._changed.clear()
                await self._changed.wait()


async def _subscribe(client: APIClient) -> tuple[list[EntityInfo], _Recorder]:
    """List entities and attach a recorder once the initial states have arrived."""
    entities, _ = await client.list_entities_services()
    helper = InitialStateHelper(entities)
    recorder = _Recorder()
    client.subscribe_states(helper.on_state_wrapper(recorder.on_state))
    await helper.wait_for_initial_states()
    return entities, recorder


def _visible_brightness(state: LightState) -> float:
    """Brightness as a remote sees it: an off light counts as zero."""
    return state.brightness if state.state else 0.0


def _brightness_is(value: float) -> DonePredicate:
    return lambda _elapsed, state: (
        _visible_brightness(state) == pytest.approx(value, abs=0.01)
    )


def _brightness_values(timeline: Timeline) -> list[float]:
    return [_visible_brightness(state) for _, state in timeline]


def _assert_ramp(timeline: Timeline, target: float) -> None:
    """Several states were published and the last one is ``target`` after ~1 s."""
    values = _brightness_values(timeline)
    assert len(values) >= 3, values
    assert values[-1] == pytest.approx(target, abs=0.05), values
    assert timeline[-1][0] >= 0.8, timeline[-1][0]


@pytest.mark.asyncio
@pytest.mark.shared_yaml(INTERVAL_FIXTURE)
async def test_transition_interval_zero_behaves_like_legacy(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A light with the default 0s interval publishes the target once, up front."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, recorder = await _subscribe(client)
        legacy = require_entity(entities, "test_legacy_light", LightInfo)

        timeline = await recorder.run(
            legacy.key,
            lambda: client.light_command(
                key=legacy.key, state=True, brightness=0.8, transition_length=1.0
            ),
            _brightness_is(0.8),
            settle=1.3,
        )

        assert _brightness_values(timeline) == [pytest.approx(0.8)]


@pytest.mark.asyncio
@pytest.mark.shared_yaml(INTERVAL_FIXTURE)
async def test_transition_interval_nonzero_emits_intermediate_updates(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Turning on over 1 s with a 200 ms interval publishes a rising ramp."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, recorder = await _subscribe(client)
        mono = require_entity(entities, "test_mono_light", LightInfo)

        timeline = await recorder.run(
            mono.key,
            lambda: client.light_command(
                key=mono.key, state=True, brightness=1.0, transition_length=1.0
            ),
            _brightness_is(1.0),
        )

        values = _brightness_values(timeline)
        assert len(values) >= 5, values
        assert values[0] == pytest.approx(0.0, abs=0.1), values
        assert values[-1] == pytest.approx(1.0, abs=0.05), values
        assert len([v for v in values if 0.1 < v < 0.9]) >= 2, values
        assert all(b >= a - 0.1 for a, b in pairwise(values)), values
        assert timeline[-1][0] >= 0.8, timeline[-1][0]


@pytest.mark.asyncio
@pytest.mark.shared_yaml(INTERVAL_FIXTURE)
async def test_light_transition_state_publish_interval(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Default-length, RGB and color temperature transitions publish on the interval."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, recorder = await _subscribe(client)
        mono = require_entity(entities, "test_mono_light", LightInfo)
        rgb = require_entity(entities, "test_rgb_light", LightInfo)
        cwww = require_entity(entities, "test_cwww_light", LightInfo)

        # No transition_length: default_transition_length (1 s) applies
        timeline = await recorder.run(
            mono.key,
            lambda: client.light_command(key=mono.key, state=True, brightness=1.0),
            _brightness_is(1.0),
        )
        _assert_ramp(timeline, 1.0)

        timeline = await recorder.run(
            rgb.key,
            lambda: client.light_command(
                key=rgb.key,
                state=True,
                brightness=1.0,
                rgb=(1.0, 0.0, 0.0),
                transition_length=1.0,
            ),
            _brightness_is(1.0),
        )
        _assert_ramp(timeline, 1.0)

        # Start at the cold end instantly so the fade to 300 mireds has a gradient
        await recorder.run(
            cwww.key,
            lambda: client.light_command(
                key=cwww.key,
                state=True,
                brightness=1.0,
                color_temperature=153.0,
                transition_length=0.0,
            ),
            lambda _t, s: (
                s.state and s.color_temperature == pytest.approx(153.0, abs=1.0)
            ),
        )
        timeline = await recorder.run(
            cwww.key,
            lambda: client.light_command(
                key=cwww.key,
                state=True,
                brightness=1.0,
                color_temperature=300.0,
                transition_length=1.0,
            ),
            lambda _t, s: s.color_temperature == pytest.approx(300.0, abs=1.0),
        )
        ct_values = [state.color_temperature for _, state in timeline]
        assert len(ct_values) >= 3, ct_values
        assert min(ct_values) < max(ct_values), ct_values
        assert timeline[-1][0] >= 0.8, timeline[-1][0]


@pytest.mark.asyncio
@pytest.mark.shared_yaml(INTERVAL_FIXTURE)
async def test_flash_interval_emits_intermediate_updates(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A flash publishes its value on the interval and ends back where it started."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, recorder = await _subscribe(client)
        mono = require_entity(entities, "test_mono_light", LightInfo)

        await recorder.run(
            mono.key,
            lambda: client.light_command(
                key=mono.key, state=True, brightness=0.4, transition_length=0.0
            ),
            _brightness_is(0.4),
        )
        timeline = await recorder.run(
            mono.key,
            lambda: client.light_command(
                key=mono.key, brightness=1.0, flash_length=1.0
            ),
            lambda t, s: (
                t > 0.5 and _visible_brightness(s) == pytest.approx(0.4, abs=0.01)
            ),
        )

        values = _brightness_values(timeline)
        assert values.count(pytest.approx(1.0, abs=0.01)) >= 3, values
        assert values[-1] == pytest.approx(0.4, abs=0.05), values
        assert timeline[-1][0] >= 0.8, timeline[-1][0]


@pytest.mark.asyncio
@pytest.mark.shared_yaml(SAVE_FIXTURE)
async def test_transition_interval_persistence_semantics(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A save=true interval transition saves once, at its end, and restores that value."""
    saves = _LogCounter(SAVED_LINE)
    async with (
        run_compiled(yaml_config, line_callback=saves.on_line),
        api_client_connected() as client,
    ):
        entities, recorder = await _subscribe(client)
        mono = require_entity(entities, "test_mono_light", LightInfo)
        button = require_entity(entities, "run_persistence_transition", ButtonInfo)
        before = saves.count

        timeline = await recorder.run(
            mono.key,
            lambda: client.button_command(button.key),
            _brightness_is(1.0),
        )
        _assert_ramp(timeline, 1.0)
        await saves.wait_for(before + 1)
        assert saves.count == before + 1

    # The restored light fades up from off over its default transition, so wait for the end
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        mono = require_entity(entities, "test_mono_light", LightInfo)
        await wait_for_state(
            client,
            lambda s: (
                isinstance(s, LightState)
                and s.key == mono.key
                and _visible_brightness(s) == pytest.approx(1.0, abs=0.01)
            ),
        )


@pytest.mark.asyncio
@pytest.mark.shared_yaml(SAVE_FIXTURE)
async def test_interrupted_deferred_save_does_not_leak(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A save=true transition cut short by another call must not save when that call ends."""
    saves = _LogCounter(SAVED_LINE)
    async with (
        run_compiled(yaml_config, line_callback=saves.on_line),
        api_client_connected() as client,
    ):
        entities, recorder = await _subscribe(client)
        mono = require_entity(entities, "test_mono_light", LightInfo)
        by_transition = require_entity(
            entities, "run_interrupted_by_unsaved_transition", ButtonInfo
        )
        by_flash = require_entity(entities, "run_interrupted_by_flash", ButtonInfo)
        before = saves.count

        # A save=false transition to 0.5 interrupts the save=true one
        await recorder.run(
            mono.key,
            lambda: client.button_command(by_transition.key),
            _brightness_is(0.5),
            settle=0.2,
        )
        assert saves.count == before

        # A flash interrupts the next save=true transition and returns to 0.5
        await recorder.run(
            mono.key,
            lambda: client.button_command(by_flash.key),
            lambda t, s: (
                t > 0.5 and _visible_brightness(s) == pytest.approx(0.5, abs=0.01)
            ),
            settle=0.2,
        )
        assert saves.count == before
