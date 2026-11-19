"""canbus on_frame matching on can_id, can_id_mask, use_extended_id and remote_transmission_request.

A host-only canbus platform reads every sent frame back, so the frames sent at boot
reach the on_frame automations, which log the arguments their lambdas receive.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .types import RunCompiledFunction

ON_FRAME = re.compile(
    r"on_frame (\w+) (id=0x[0-9A-F]+ rtr=\d len=\d+ first=0x[0-9A-F]{2})"
)

# (entry, can_id rtr len first) in the order the device must log them
EXPECTED = [
    ("std_100", "id=0x100 rtr=0 len=1 first=0x01"),
    ("ext_100", "id=0x100 rtr=0 len=1 first=0x02"),
    ("std_12x", "id=0x125 rtr=0 len=1 first=0x03"),
    # std 0x135 is outside the mask and ext 0x125 is not a standard frame: nothing fires
    ("rtr_200", "id=0x200 rtr=1 len=0 first=0x00"),
    ("any_200", "id=0x200 rtr=1 len=0 first=0x00"),
    ("data_200", "id=0x200 rtr=0 len=2 first=0x07"),
    ("any_200", "id=0x200 rtr=0 len=2 first=0x07"),
    # can_id is the full received id, not the masked one; 0x1ABCDF42 misses the mask
    ("ext_masked", "id=0x1ABCDE42 rtr=0 len=1 first=0x08"),
    ("done", "id=0x7FF rtr=0 len=1 first=0xAA"),
]


@pytest.mark.asyncio
async def test_canbus_on_frame(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """Each frame fires exactly the on_frame entries whose filters match, in config order."""
    waiter = LineWaiter()
    async with run_compiled(yaml_config, line_callback=waiter.callback):
        await waiter.wait_count("on_frame done", 1, 10, "last frame not received")

    seen = [m.groups() for line in waiter.lines if (m := ON_FRAME.search(line))]
    assert seen == EXPECTED
