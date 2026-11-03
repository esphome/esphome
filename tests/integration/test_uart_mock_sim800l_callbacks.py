"""sim800l on_* automations receive the values parsed from the modem's lines.

The fixture injects a call, a DTMF key, an SMS, a connected call and a USSD reply in one
write; each automation logs the arguments it receives.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .types import RunCompiledFunction

ANSI_PATTERN = re.compile(r"\x1b\[[0-9;]*m")
EVENT = re.compile(r"\]: (on_\w+.*)$")

EXPECTED = [
    "on_incoming_call caller_id=+15551234567",
    "on_dtmf_received key=7",
    "on_call_disconnected",
    "on_sms_received A sender=+15557654321 message=Hello there",
    "on_sms_received B sender=+15557654321 message=Hello there",
    "on_incoming_call caller_id=+15550000000",
    "on_call_connected",
    "on_call_disconnected",
    "on_ussd_received before=Balance 5",
    "on_ussd_received after=Balance 5",
]


@pytest.mark.asyncio
async def test_uart_mock_sim800l_callbacks(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """Every trigger fires once per event, in order, with the parsed arguments."""
    waiter = LineWaiter()
    async with run_compiled(yaml_config, line_callback=waiter.callback):
        await waiter.wait_count(
            "on_ussd_received after=", 1, 10, "USSD reply not received"
        )

    seen = [
        m.group(1)
        for line in waiter.lines
        if (m := EVENT.search(ANSI_PATTERN.sub("", line).rstrip()))
    ]
    assert seen == EXPECTED
