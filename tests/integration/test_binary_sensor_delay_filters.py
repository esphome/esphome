"""Pin the output of the binary sensor delay and lambda filters for one input sequence.

The source goes ON 0 ms, OFF 30 ms, ON 60 ms, OFF 560 ms, ON 710 ms, OFF 1210 ms.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

OUT_PATTERN = re.compile(r"OUT (\w+)=(ON|OFF) @(\d+)")
SRC_PATTERN = re.compile(r"SRC (ON|OFF) @(\d+)")
# How late an output may land after its due time, for scheduler and CI jitter; well below
# the 100 ms shortest delay, so a skipped delay fails
LATE_MS = 60
EARLY_MS = 2

EXPECTED: dict[str, list[str]] = {
    # Short pulses are swallowed; each edge lands 100 ms late
    "on_off_single": ["ON", "OFF", "ON", "OFF"],
    # The 300 ms off delay outlasts the 150 ms OFF at 560 ms, so only the last OFF shows
    "on_off_split": ["ON", "OFF"],
    "on_off_lambda": ["ON", "OFF"],
    # The delay lambda reads the binary sensor with id `x` (ON, so 100 ms), not the filter input
    "on_off_id_x": ["ON", "OFF", "ON", "OFF"],
    "delayed_on": ["OFF", "ON", "OFF", "ON", "OFF"],
    "delayed_on_lambda": ["OFF", "ON", "OFF", "ON", "OFF"],
    "delayed_off": ["ON", "OFF", "ON", "OFF"],
    # The lambda inverts and drops its second value; repeats do not reach the sensor
    "lambda_filter": ["OFF", "ON", "OFF", "ON"],
}

# For each output: the source edge (0 to 5) it follows and the delay in ms after that edge
TIMING: dict[str, list[tuple[int, int]]] = {
    "on_off_single": [(2, 100), (3, 100), (4, 100), (5, 100)],
    "on_off_split": [(2, 100), (5, 300)],
    "on_off_lambda": [(2, 100), (5, 300)],
    "delayed_on": [(1, 0), (2, 100), (3, 0), (4, 100), (5, 0)],
    "delayed_on_lambda": [(1, 0), (2, 100), (3, 0), (4, 100), (5, 0)],
    "delayed_off": [(0, 0), (3, 100), (4, 0), (5, 100)],
    "lambda_filter": [(0, 0), (3, 0), (4, 0), (5, 0)],
}


@pytest.mark.asyncio
async def test_binary_sensor_delay_filters(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each copy sensor runs one filter over the same timed inputs; every output is logged in order."""
    waiter = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        done = waiter.expect("FILTERS DONE")
        client.button_command(require_entity(entities, "run_filters").key)
        await waiter.wait(done, timeout=5.0)

    edges = [
        int(m.group(2)) for line in waiter.lines if (m := SRC_PATTERN.search(line))
    ]
    assert len(edges) == 6, edges
    outputs: dict[str, list[str]] = {name: [] for name in EXPECTED}
    times: dict[str, list[int]] = {name: [] for name in EXPECTED}
    for line in waiter.lines:
        if (match := OUT_PATTERN.search(line)) is not None:
            outputs[match.group(1)].append(match.group(2))
            times[match.group(1)].append(int(match.group(3)))
    assert outputs == EXPECTED
    for name, timing in TIMING.items():
        lateness = [
            at - (edges[edge] + delay)
            for at, (edge, delay) in zip(times[name], timing, strict=True)
        ]
        # The source edge is logged in the same millisecond the delay starts, or just after
        assert all(-EARLY_MS <= late <= LATE_MS for late in lateness), (name, lateness)
