"""Integration test for GetTimeResponse parsed_timezone presence handling."""

from __future__ import annotations

from aioesphomeapi import connection as api_connection
from aioesphomeapi.api_pb2 import GetTimeResponse
import pytest

from .state_utils import SensorTracker, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction

# 2024-01-01 00:00:00 UTC
EPOCH = 1704067200
# POSIX offsets are positive west of UTC, so UTC+7 is -25200 and UTC-5 is 18000
UTC_PLUS_7 = -25200
UTC_MINUS_5 = 18000


@pytest.mark.asyncio
async def test_api_get_time_response_timezone(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A present parsed_timezone is applied even when all zero; an absent one is ignored."""
    # The client answers the device's own GetTimeRequest with the host timezone;
    # strip the parsed field from that reply so only the messages sent below
    # can change the device timezone.
    monkeypatch.setattr(api_connection, "_build_parsed_tz_proto", lambda tz: None)

    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        tracker = SensorTracker(["tz_offset"])
        tracker.key_to_sensor = build_key_to_entity_mapping(entities, ["tz_offset"])
        client.subscribe_states(tracker.on_state)

        await tracker.await_change(tracker.expect_any("tz_offset"), "tz_offset")
        initial = tracker.sensor_states["tz_offset"][-1]
        # Pick a zone that differs from the codegen default so the change is visible
        target = UTC_PLUS_7 if initial != UTC_PLUS_7 else UTC_MINUS_5

        # Present, non-zero: applied
        future = tracker.expect("tz_offset", target)
        resp = GetTimeResponse(epoch_seconds=EPOCH)
        resp.parsed_timezone.std_offset_seconds = target
        resp.parsed_timezone.dst_offset_seconds = target
        client._connection.send_messages((resp,))
        await tracker.await_change(future, "tz_offset")

        # Absent (legacy client with only the deprecated string): ignored, and in
        # particular not mistaken for an all-zero UTC zone
        future = tracker.expect("tz_offset", 0)
        resp = GetTimeResponse(epoch_seconds=EPOCH, timezone="UTC0")
        client._connection.send_messages((resp,))
        await tracker.await_must_not_change(future, "tz_offset", timeout=1.0)
        assert tracker.sensor_states["tz_offset"][-1] == target
        # Retire the expectation so it cannot swallow the first matching state
        # meant for the next phase
        future.cancel()

        # Present but all zero (genuine UTC): applied
        future = tracker.expect("tz_offset", 0)
        resp = GetTimeResponse(epoch_seconds=EPOCH)
        resp.parsed_timezone.SetInParent()
        client._connection.send_messages((resp,))
        await tracker.await_change(future, "tz_offset")
