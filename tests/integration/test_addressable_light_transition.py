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
        loop = asyncio.get_event_loop()
        samples: list[tuple[float, float]] = []
        start_time: float | None = None

        def on_state(state: object) -> None:
            nonlocal start_time
            if not isinstance(state, SensorState) or state.key != sensor.key:
                return
            now = loop.time()
            if start_time is None:
                start_time = now
            samples.append((now - start_time, state.state))

        client.subscribe_states(on_state)

        # Give the first poll a chance to land so we have a baseline of 0.
        await asyncio.sleep(0.1)

        # Start transition: off -> full white over 1 second. This is the
        # scenario from the bug report, compressed in time.
        transition_s = 1.0
        client.light_command(
            key=light.key,
            state=True,
            rgb=(1.0, 1.0, 1.0),
            brightness=1.0,
            transition_length=transition_s,
        )

        # Let the full transition run, plus margin for the final sample.
        await asyncio.sleep(transition_s + 0.2)

        # Partition samples by transition progress. We reset the time origin
        # at the moment the first post-command sample arrives, since there is
        # some latency between issuing the command and the sensor observing
        # the transition begin.
        assert samples, "no sensor samples received"

        # Find first sample where the transition started producing nonzero
        # output (or fall back to the first sample).
        first_nonzero_idx = next((i for i, (_, v) in enumerate(samples) if v > 0), None)
        assert first_nonzero_idx is not None, (
            "raw byte never rose above 0 during the transition — the fade stalled"
        )

        t0 = samples[first_nonzero_idx][0]
        # Collect samples from the first nonzero point onward, re-based to t=0.
        rel = [(t - t0, v) for (t, v) in samples[first_nonzero_idx:]]

        # Assertion 1: the transition is not stalled. With the bug, the raw
        # byte stays at 0 until ~90% of the transition duration. With the fix,
        # it becomes nonzero in the first ~30% (for gamma 2.8, pre-gamma 76
        # clears the gamma threshold at progress ~0.30). We assert that the
        # first nonzero sample arrives well before 70% of the transition,
        # giving generous slack for scheduling jitter.
        first_nonzero_time = samples[first_nonzero_idx][0] - samples[0][0]
        assert first_nonzero_time < transition_s * 0.7, (
            f"raw byte only rose above 0 at t={first_nonzero_time:.3f}s "
            f"(>{transition_s * 0.7:.3f}s) — transition is stalling"
        )

        # Assertion 2: by the time the transition has had 70% of its duration
        # to run from its first visible step, the raw byte should be at least
        # ~half of its final value. This catches "barely moves then jumps at
        # the end" regressions.
        late_samples = [v for (t, v) in rel if t >= transition_s * 0.7]
        assert late_samples, "no samples captured late in transition"
        assert max(late_samples) >= 100, (
            f"raw byte peaked at only {max(late_samples)} late in transition "
            "(expected >= 100 for white target at gamma 2.8)"
        )

        # Assertion 3: final value reaches target. Gamma 2.8 of 255 is 255.
        final_samples = [v for (_, v) in samples[-5:]]
        assert max(final_samples) >= 250, (
            f"final raw byte was {max(final_samples)}, expected >= 250"
        )
