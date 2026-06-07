"""Integration test for addressable light transitions with gamma correction.

Regression test for a bug where a long turn-on transition on an addressable
light with gamma correction (e.g. gamma_correct: 2.8) produced no visible
output for ~90% of the transition duration, then jumped to the target in the
final ~10%. Root cause: the transition algorithm read each LED's current value
back through the 8-bit stored byte every step; at gamma 2.8 any pre-gamma value
below ~27 rounds to stored byte 0, so the stored byte stalled at 0 until
progress was high enough for a single step to produce a large-enough pre-gamma
value to clear the gamma threshold.

The fix interpolates against a cached start color when all LEDs started at the
same value (the common case for plain turn_on/turn_off), avoiding the round-trip.

This test uses a host-only mock addressable light that exposes the raw stored
byte of each LED, so we can observe the transition directly.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import LightInfo, SensorInfo, SensorState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_addressable_light_transition(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """With gamma 2.8, the stored raw byte must rise visibly well before the end."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        light = require_entity(entities, "test_strip", LightInfo)
        sensor = require_entity(entities, "led0_red_raw", SensorInfo)

        # Track the raw-byte sensor. It polls every 10ms in the fixture, and
        # ESPHome sensors publish on every change, so we collect a time series.
        # Samples are stored as absolute (loop_time, value); we rebase to the
        # command-issue time after the run so pre-command samples are strictly
        # negative and reliably excluded.
        loop = asyncio.get_running_loop()
        samples: list[tuple[float, float]] = []

        def on_state(state: object) -> None:
            if not isinstance(state, SensorState) or state.key != sensor.key:
                return
            samples.append((loop.time(), state.state))

        # InitialStateHelper swallows the first state ESPHome sends per entity
        # on subscribe, so on_state only sees real post-subscribe updates.
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        # Start transition: off -> full white over 1 second. This is the
        # scenario from the bug report, compressed in time.
        transition_s = 1.0
        command_time = loop.time()
        client.light_command(
            key=light.key,
            state=True,
            rgb=(1.0, 1.0, 1.0),
            brightness=1.0,
            transition_length=transition_s,
        )

        # Let the full transition run, plus margin for the final sample.
        await asyncio.sleep(transition_s + 0.2)

        # Rebase to command-issue time. Pre-command samples have t < 0 and are
        # excluded; everything else is in seconds since the command was issued.
        post_command = [
            (t - command_time, v) for (t, v) in samples if t >= command_time
        ]
        assert post_command, "no sensor samples received after command was issued"

        # Assertion 1: the transition is not stalled. With the bug, the raw
        # byte stays at 0 until ~90% of the transition duration. With the fix,
        # it becomes nonzero in the first ~30% (for gamma 2.8, pre-gamma 76
        # clears the gamma threshold at progress ~0.30). Require the first
        # nonzero sample to land well before 50% of the transition duration,
        # measured from the command-issue time. The 50% bound (rather than
        # 70%) leaves headroom for assertion 2's mid-window check.
        first_nonzero = next(((t, v) for (t, v) in post_command if v > 0), None)
        assert first_nonzero is not None, (
            "raw byte never rose above 0 during the transition — the fade stalled"
        )
        assert first_nonzero[0] < transition_s * 0.5, (
            f"raw byte only rose above 0 at t={first_nonzero[0]:.3f}s "
            f"(>{transition_s * 0.5:.3f}s after command) — transition is stalling"
        )

        # Assertion 2: by mid-late transition, the raw byte should have reached
        # a substantial fraction of its final value. Bound the window to
        # [50%, 90%] of the transition so the post-transition settled value
        # (which always reaches 255) can't satisfy this assertion — that would
        # let "stays at 0 then jumps at 99%" regressions slip through.
        mid_window = [
            v
            for (t, v) in post_command
            if transition_s * 0.5 <= t <= transition_s * 0.9
        ]
        assert mid_window, "no samples captured in mid-transition window"
        assert max(mid_window) >= 100, (
            f"raw byte peaked at only {max(mid_window)} between 50%–90% of "
            "transition (expected >= 100 for white target at gamma 2.8)"
        )

        # Assertion 3: final value reaches target. Gamma 2.8 of 255 is 255.
        final_samples = [v for (_, v) in post_command[-5:]]
        assert max(final_samples) >= 250, (
            f"final raw byte was {max(final_samples)}, expected >= 250"
        )
