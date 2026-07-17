"""Integration test verifying light effects can dim to 0% brightness while staying on.

Regression test for https://github.com/esphome/esphome/issues/17639, where PR #17103's
"make turn-on visible" logic in LightCall::validate_() also clobbered brightness set by a
running effect (e.g. pulse, strobe), forcing it back to 100% and breaking the dark phase
of those effects.

Effect ticks are published with `publish: false` (so Home Assistant isn't spammed with
every frame), so the effect's actual output can't be observed via API state broadcasts.
Instead, this test reads the output component's log lines, which are written on every
update regardless of the publish flag.
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_effect_zero_brightness(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Pulse and strobe effects must be able to reach 0% brightness while the light stays on."""
    output_pattern = re.compile(r"PULSE_OUTPUT:([\d.]+)")
    observed: list[float] = []

    def on_log_line(line: str) -> None:
        match = output_pattern.search(line)
        if match:
            observed.append(float(match.group(1)))

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        light = next(e for e in entities if e.object_id == "test_pulse_light")

        state_future: asyncio.Future[object] = (
            asyncio.get_running_loop().create_future()
        )

        def on_state(state: object) -> None:
            if not state_future.done():
                state_future.set_result(state)

        client.subscribe_states(on_state)

        # Turn the light on first so the effect starts from a known, visible state.
        client.light_command(key=light.key, state=True, brightness=1.0)
        await asyncio.wait_for(state_future, timeout=5.0)

        for effect_name in ("Fast Pulse", "Fast Strobe"):
            observed.clear()
            client.light_command(key=light.key, effect=effect_name)
            # Let several effect cycles run (update_interval/duration is 50ms in the fixture).
            await asyncio.sleep(1.0)

            assert observed, f"No output observed while running effect {effect_name!r}"
            assert min(observed) == pytest.approx(0.0, abs=0.01), (
                f"Effect {effect_name!r} never dimmed to 0% brightness while the light "
                f"stayed on -- got min={min(observed):.4f} (values: {observed})"
            )
            assert max(observed) > 0.5, (
                f"Effect {effect_name!r} never reached full brightness -- "
                f"got max={max(observed):.4f}"
            )

        client.light_command(key=light.key, effect="None")
