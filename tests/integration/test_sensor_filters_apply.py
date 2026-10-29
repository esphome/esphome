"""Pin the output of the stateless sensor filters for finite, NaN and infinite inputs."""

from __future__ import annotations

import math
import re

import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

INPUTS = [-10.0, 0.0, 0.5, 2.25, 12.345, 50.0, 150.0, math.nan, math.inf]
NAN, INF = math.nan, math.inf
OUT_PATTERN = re.compile(r"OUT (\w+)=([^\s\x1b]+)")
TEXT_PATTERN = re.compile(r"TXT (\w+)=([^\s\x1b]*)")


def _clamp(value: float, low: float | None = None, high: float | None = None) -> float:
    # NaN fails every comparison, so it takes the first bound that is set
    if low is not None and not low <= value:
        return low
    if high is not None and not value <= high:
        return high
    return value


EXPECTED: dict[str, list[float]] = {
    "offset_const": [v + 2.5 for v in INPUTS],
    "offset_lambda": [v + 1.25 for v in INPUTS],
    "multiply_const": [v * 1.5 for v in INPUTS],
    # The lambda is `1.0f + 1.0f`, so it must bind as a whole: x * 2, not x * 1 + 1
    "multiply_lambda": [v * 2.0 for v in INPUTS],
    "clamp_min": [_clamp(v, low=0.0) for v in INPUTS],
    "clamp_max": [_clamp(v, high=10.0) for v in INPUTS],
    "clamp_both": [_clamp(v, 0.0, 10.0) for v in INPUTS],
    "clamp_ignore": [0.0, 0.5, 2.25],
    # Pins a known bug, to be fixed separately: with b_constant (c == 0) the formula gives NaN
    "ntc_resistance": [NAN] * len(INPUTS),
    # Computed in double from the float rounded coefficients; non-finite inputs give NaN
    "ntc_resistance_abc": [
        68785.8828,
        40731.2266,
        39711.0898,
        36359.6367,
        22255.3613,
        4505.29346,
        226.547989,
        NAN,
        NAN,
    ],
    # The inputs are multiplied by 1000 first, so they are resistances in ohms
    "ntc_temperature": [
        NAN,
        NAN,
        112.11689,
        62.8283424,
        20.3332348,
        -7.29642916,
        -25.600666,
        NAN,
        NAN,
    ],
    # log(0) is -inf, which the cubic term turns into absolute zero; with c == 0 it is NaN above
    "ntc_temperature_abc": [
        NAN,
        -273.149994,
        118.490578,
        69.1652069,
        25.3175488,
        -3.98647237,
        -23.8062897,
        NAN,
        NAN,
    ],
    "calibrate_least_squares": [v * 0.95 + 1.0 / 3.0 for v in INPUTS],
    "calibrate_exact": [v * 10.0 if v < 10.0 else v * 5.0 + 50.0 for v in INPUTS[:-2]]
    + [NAN, INF],
    "calibrate_polynomial": [1.0 + v + v * v for v in INPUTS],
    "lambda_filter": [v * 2.0 for v in INPUTS if not v > 100.0],
    "round_filter": [-10.0, 0.0, 0.5, 2.3, 12.3, 50.0, 150.0, NAN, INF],
    "round_multiple": [-10.0, 0.0, 0.5, 2.0, 12.5, 50.0, 150.0, NAN, INF],
}


TEXT_EXPECTED: dict[str, list[str]] = {
    "text_lambda_replace": ["[a]", "[skip]", "[b]"],
    "text_lambda_drop": ["a", "b"],
}


@pytest.mark.asyncio
async def test_sensor_filters_apply(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each copy sensor runs one filter over the same inputs; every output is logged in order."""
    waiter = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        done = waiter.expect("FILTERS DONE")
        client.button_command(require_entity(entities, "run_filters").key)
        await waiter.wait(done, timeout=5.0)

    outputs: dict[str, list[float]] = {name: [] for name in EXPECTED}
    for line in waiter.lines:
        if (match := OUT_PATTERN.search(line)) is not None:
            outputs[match.group(1)].append(float(match.group(2)))
    texts: dict[str, list[str]] = {name: [] for name in TEXT_EXPECTED}
    for line in waiter.lines:
        if (match := TEXT_PATTERN.search(line)) is not None:
            texts[match.group(1)].append(match.group(2))
    assert texts == TEXT_EXPECTED
    for name, expected in EXPECTED.items():
        assert outputs[name] == pytest.approx(expected, rel=1e-5, nan_ok=True), name
