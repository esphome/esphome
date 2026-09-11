"""IR/RF transmit completion replies (API 1.18) and the client pacing built on them.

The transmitter is a host-only mock that takes a frame's real duration to
"send" it and reports completion from a scheduler timeout, like the ESP32 RMT
backend. The client is expected to hold the next frame until the device
replies, so the mock never sees an overlapping frame.
"""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import RadioFrequencyInfo
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

try:
    from aioesphomeapi.api_pb2 import InfraredRFTransmitCompleteResponse
except ImportError:  # aioesphomeapi older than the API 1.18 message
    InfraredRFTransmitCompleteResponse = None

pytestmark = pytest.mark.skipif(
    InfraredRFTransmitCompleteResponse is None,
    reason="needs an aioesphomeapi with InfraredRFTransmitCompleteResponse",
)

FRAME_COUNT = 5
# 20 marks and spaces of 500 us, sent twice: 40 ms per frame
TIMINGS = [500, -500] * 10
REPEAT = 2
MOCK_EVENT = re.compile(
    r"remote_transmitter_mock[^\]]*\]: (TX|Complete|Overlap)\b.*?seq=(\d+)"
)


@pytest.mark.asyncio
async def test_ir_rf_transmit_complete(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Frames are answered once they leave the transmitter, never overlap, and a refused
    request is answered at once. Two entities share the transmitter and each gets its own reply."""
    loop = asyncio.get_running_loop()
    events: list[tuple[str, int]] = []
    all_sent = loop.create_future()

    def line_callback(line: str) -> None:
        if (match := MOCK_EVENT.search(line)) is None:
            return
        events.append((match.group(1), int(match.group(2))))
        if (
            match.group(1) == "Complete"
            and not all_sent.done()
            and sum(kind == "Complete" for kind, _ in events) == FRAME_COUNT
        ):
            all_sent.set_result(None)

    completions: list[InfraredRFTransmitCompleteResponse] = []
    all_replied = loop.create_future()
    refused = loop.create_future()

    def on_complete(msg: InfraredRFTransmitCompleteResponse) -> None:
        if not msg.success:
            refused.set_result(msg)
            return
        completions.append(msg)
        if len(completions) == FRAME_COUNT and not all_replied.done():
            all_replied.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        rf = find_entity(entities, "rf_transmitter", RadioFrequencyInfo)
        rf_b = find_entity(entities, "rf_transmitter_b", RadioFrequencyInfo)
        assert rf is not None and rf_b is not None, "RF transmitter entities not found"

        client._connection.add_message_callback(
            on_complete, (InfraredRFTransmitCompleteResponse,)
        )
        # alternate between the two entities sharing the transmitter
        keys = [rf.key if i % 2 == 0 else rf_b.key for i in range(FRAME_COUNT)]
        for key in keys:
            client.radio_frequency_transmit_raw_timings(
                key, 433920000, TIMINGS, repeat_count=REPEAT
            )

        await asyncio.wait_for(all_replied, timeout=10)
        await asyncio.wait_for(all_sent, timeout=10)

        # A request the entity refuses is answered right away with success false;
        # no timings, so the proxy rejects it before it reaches the transmitter
        client.radio_frequency_transmit_raw_timings(rf.key, 433920000, [])
        refused_msg = await asyncio.wait_for(refused, timeout=10)

    assert [msg.key for msg in completions] == keys
    assert refused_msg.key == rf.key

    # The mock saw one frame at a time: every transmit follows the previous completion
    kinds = [kind for kind, _ in events]
    assert "Overlap" not in kinds
    assert kinds == ["TX", "Complete"] * FRAME_COUNT, events
    seqs = [seq for _, seq in events]
    assert seqs == [seq for seq in seqs[::2] for _ in range(2)], events
