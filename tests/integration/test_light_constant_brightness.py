"""Integration test for constant_brightness with gamma correction.

Tests both constant_brightness: true and false cwww lights with gamma
correction in a single compilation to verify:
- constant_brightness: true maintains constant total CW+WW power output
- constant_brightness: false correctly varies total power across color temps

This is a regression test for https://github.com/esphome/esphome/issues/15040
where the gamma LUT refactor (#14123) broke constant_brightness by applying
gamma after the balancing formula instead of before it.
"""

from __future__ import annotations

import asyncio
import re
from typing import Any

from aioesphomeapi import EntityState, LightInfo, LightState
import pytest

from .log_utils import LineWaiter
from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

CB_CW_RE = re.compile(r"(?<!N)CB_CW_OUTPUT:([\d.]+)")
CB_WW_RE = re.compile(r"(?<!N)CB_WW_OUTPUT:([\d.]+)")
NCB_CW_RE = re.compile(r"NCB_CW_OUTPUT:([\d.]+)")
NCB_WW_RE = re.compile(r"NCB_WW_OUTPUT:([\d.]+)")


@pytest.mark.asyncio
async def test_light_constant_brightness(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test constant_brightness true and false behavior with gamma correction."""
    waiter = LineWaiter()

    def latest(pattern: re.Pattern[str]) -> float:
        """Return the last output value logged so far, or 0.0 if none."""
        for line in reversed(waiter.lines):
            if match := pattern.search(line):
                return float(match[1])
        return 0.0

    loop = asyncio.get_running_loop()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        cb_light = require_entity(entities, "test_cb_light", LightInfo, exact=True)
        ncb_light = require_entity(entities, "test_ncb_light", LightInfo, exact=True)

        # Use InitialStateHelper to wait for initial state broadcast
        initial_state_helper = InitialStateHelper(entities)

        # Track state changes per light key
        state_futures: dict[int, asyncio.Future[EntityState]] = {}

        def on_state(state: EntityState) -> None:
            if isinstance(state, LightState) and state.key in state_futures:
                future = state_futures[state.key]
                if not future.done():
                    future.set_result(state)

        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        await initial_state_helper.wait_for_initial_states()

        async def send_and_wait(
            light_key: int, timeout: float = 5.0, **kwargs: Any
        ) -> LightState:
            """Send a light command and wait for the state response."""
            state_futures[light_key] = loop.create_future()
            client.light_command(key=light_key, **kwargs)
            return await wait_or_fail(
                state_futures[light_key],
                timeout,
                f"Timeout waiting for light state after command: {kwargs}",
            )

        # --- Test constant_brightness: true ---

        # Turn on CB light at full brightness
        await send_and_wait(
            cb_light.key,
            state=True,
            brightness=1.0,
            color_temperature=153.0,
            transition_length=0,
        )

        test_mireds = [
            153.0,  # Pure cold white
            200.0,  # Mostly cold
            280.0,  # Mixed
            326.5,  # Midpoint
            400.0,  # Mostly warm
            500.0,  # Pure warm white
        ]

        cb_totals: list[tuple[float, float, float]] = []
        for mireds in test_mireds:
            await send_and_wait(
                cb_light.key, color_temperature=mireds, transition_length=0
            )
            cb_totals.append((mireds, latest(CB_CW_RE), latest(CB_WW_RE)))

        # All totals should be approximately equal (constant brightness)
        reference_total = next((cw + ww for _, cw, ww in cb_totals if cw + ww > 0), 0)
        assert reference_total > 0, (
            f"Reference total power is zero, CB light outputs not working. "
            f"Values: {cb_totals}"
        )

        for mireds, cw, ww in cb_totals:
            total = cw + ww
            assert total == pytest.approx(reference_total, rel=0.05), (
                f"constant_brightness: Total power at {mireds} mireds "
                f"({total:.4f}) differs from reference ({reference_total:.4f}) "
                f"by more than 5%. CW={cw:.4f}, WW={ww:.4f}. "
                f"All values: {cb_totals}"
            )

        # --- Test constant_brightness: false ---

        # Turn on NCB light at full brightness
        await send_and_wait(
            ncb_light.key,
            state=True,
            brightness=1.0,
            color_temperature=153.0,
            transition_length=0,
        )

        ncb_totals: list[tuple[float, float, float]] = []
        for mireds in test_mireds:
            await send_and_wait(
                ncb_light.key, color_temperature=mireds, transition_length=0
            )
            ncb_totals.append((mireds, latest(NCB_CW_RE), latest(NCB_WW_RE)))

        extreme_cw = ncb_totals[0]  # 153 mireds - pure cold
        extreme_ww = ncb_totals[-1]  # 500 mireds - pure warm
        midpoint = ncb_totals[3]  # 326.5 mireds - midpoint

        # At pure cold white, WW should be ~0
        assert extreme_cw[2] == pytest.approx(0.0, abs=0.01), (
            f"Pure cold white should have WW~0, got WW={extreme_cw[2]:.4f}"
        )
        # At pure warm white, CW should be ~0
        assert extreme_ww[1] == pytest.approx(0.0, abs=0.01), (
            f"Pure warm white should have CW~0, got CW={extreme_ww[1]:.4f}"
        )

        # At midpoint, both channels should be non-zero
        assert midpoint[1] > 0.05, f"Midpoint CW should be >0.05, got {midpoint[1]:.4f}"
        assert midpoint[2] > 0.05, f"Midpoint WW should be >0.05, got {midpoint[2]:.4f}"

        # Total power at midpoint should be higher than at the extremes
        midpoint_total = midpoint[1] + midpoint[2]
        extreme_cw_total = extreme_cw[1] + extreme_cw[2]
        extreme_ww_total = extreme_ww[1] + extreme_ww[2]

        assert midpoint_total > extreme_cw_total, (
            f"Midpoint total ({midpoint_total:.4f}) should be > pure CW total "
            f"({extreme_cw_total:.4f}). All values: {ncb_totals}"
        )
        assert midpoint_total > extreme_ww_total, (
            f"Midpoint total ({midpoint_total:.4f}) should be > pure WW total "
            f"({extreme_ww_total:.4f}). All values: {ncb_totals}"
        )
