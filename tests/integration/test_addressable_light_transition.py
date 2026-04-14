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

from aioesphomeapi import SensorState
import pytest

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
        light = next(e for e in entities if e.object_id == "test_strip")
        sensor = next(e for e in entities if e.object_id == "led0_red_raw")

        # Track the raw-byte sensor. It polls every 10ms in the fixture, and
        # ESPHome sensors publish on every change, so we collect a time series.
        # Samples are stored as (seconds_since_command_issue, value). Times
        # before the command was issued are negative.
        loop = asyncio.get_running_loop()
        samples: list[tuple[float, float]] = []
        command_time: float | None = None

        def on_state(state: object) -> None:
            if not isinstance(state, SensorState) or state.key != sensor.key:
                return
            now = loop.time()
            # If the command hasn't been issued yet, use 0 as the origin;
            # those samples get negative times and are excluded below.
            origin = command_time if command_time is not None else now
            samples.append((now - origin, state.state))

        client.subscribe_states(on_state)

        # Give the first poll a chance to land so we have a baseline of 0.
        await asyncio.sleep(0.1)

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

        # Only look at samples that arrived after the command was issued.
        post_command = [(t, v) for (t, v) in samples if t >= 0]
        assert post_command, "no sensor samples received after command was issued"

        # Assertion 1: the transition is not stalled. With the bug, the raw
        # byte stays at 0 until ~90% of the transition duration. With the fix,
        # it becomes nonzero in the first ~30% (for gamma 2.8, pre-gamma 76
        # clears the gamma threshold at progress ~0.30). Require the first
        # nonzero sample to land well before 70% of the transition duration,
        # measured from the command-issue time.
        first_nonzero = next(((t, v) for (t, v) in post_command if v > 0), None)
        assert first_nonzero is not None, (
            "raw byte never rose above 0 during the transition — the fade stalled"
        )
        assert first_nonzero[0] < transition_s * 0.7, (
            f"raw byte only rose above 0 at t={first_nonzero[0]:.3f}s "
            f"(>{transition_s * 0.7:.3f}s after command) — transition is stalling"
        )

        # Assertion 2: by 70% of the transition duration after the command,
        # the raw byte should have reached a substantial fraction of its final
        # value. This catches "barely moves then jumps at the end" regressions
        # that pass assertion 1 but still stall most of the range.
        late_samples = [v for (t, v) in post_command if t >= transition_s * 0.7]
        assert late_samples, "no samples captured late in transition"
        assert max(late_samples) >= 100, (
            f"raw byte peaked at only {max(late_samples)} at/after 70% of "
            "transition (expected >= 100 for white target at gamma 2.8)"
        )

        # Assertion 3: final value reaches target. Gamma 2.8 of 255 is 255.
        final_samples = [v for (_, v) in post_command[-5:]]
        assert max(final_samples) >= 250, (
            f"final raw byte was {max(final_samples)}, expected >= 250"
        )
