"""Integration test for constant_brightness with unequal white channels.

A color_temperature_priority cwww light with a cold_to_warm_ratio of 0.7077
(channels giving 0.92 and 1.30), from the YAML to the levels its outputs write:
- at full brightness both channels are full where their light balances
- the light (0.92 * cold + 1.30 * warm) stays constant across color temperatures
  at low brightness
"""

from __future__ import annotations

import re

from aioesphomeapi import LightInfo
import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

CW_RE = re.compile(r"WEIGHTED_CW_OUTPUT:([\d.]+)")
WW_RE = re.compile(r"WEIGHTED_WW_OUTPUT:([\d.]+)")
# Only the ratio is configured (0.7077); these two are in the same ratio
COLD_RELATIVE = 0.92
WARM_RELATIVE = 1.30
COLD_MIREDS = 153.0
WARM_MIREDS = 500.0


def mireds_for(cold_fraction: float) -> float:
    """Mireds at which the cold channel's linear share of the light is cold_fraction."""
    return WARM_MIREDS - cold_fraction * (WARM_MIREDS - COLD_MIREDS)


@pytest.mark.asyncio
async def test_light_constant_brightness_weights(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test relative brightness from config to output levels."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        light = require_entity(entities, "test_weighted_light", LightInfo, exact=True)

        async def outputs(**kwargs: float | bool) -> tuple[float, float]:
            """Send a command and return the cold/warm levels its write logs."""
            cold = waiter.expect(CW_RE)
            warm = waiter.expect(WW_RE)
            client.light_command(key=light.key, transition_length=0, **kwargs)
            return float((await waiter.wait(cold))[1]), float(
                (await waiter.wait(warm))[1]
            )

        # Full brightness drives both channels fully where their light balances
        balanced = mireds_for(COLD_RELATIVE / (COLD_RELATIVE + WARM_RELATIVE))
        cw, ww = await outputs(state=True, brightness=1.0, color_temperature=balanced)
        assert cw == pytest.approx(1.0, abs=0.02)
        assert ww == pytest.approx(1.0, abs=0.02)

        # At 40% linear brightness (0.4 ** (1 / 2.8) before gamma) the light is
        # constant: the weaker cold strip alone can give it up to
        # 0.92 / (0.92 + 1.30) = 41%, beyond that the cold end runs out
        expected_light = 0.4 * (COLD_RELATIVE + WARM_RELATIVE)
        await outputs(brightness=0.4 ** (1 / 2.8), color_temperature=WARM_MIREDS)
        for mireds in (153.0, 200.0, 280.0, 326.5, 400.0, 500.0):
            cw, ww = await outputs(color_temperature=mireds)
            light_out = COLD_RELATIVE * cw + WARM_RELATIVE * ww
            assert light_out == pytest.approx(expected_light, rel=0.03), (
                f"Light at {mireds} mireds is {light_out:.4f}, expected "
                f"{expected_light:.4f} (CW={cw:.4f}, WW={ww:.4f})"
            )
