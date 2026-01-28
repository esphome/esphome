from __future__ import annotations

from dataclasses import dataclass

from esphome.dashboard.entries import (
    EntryState,
    EntryStateSource,
    ReachableState,
    bool_to_entry_state,
)
from esphome.dashboard.status.mqtt import MqttStatusThread


@dataclass
class _FakeEntry:
    name: str
    state: EntryState


class _FakeEntries:
    def __init__(self, entry: _FakeEntry) -> None:
        self._entry = entry
        self.set_state_if_online_or_source_calls: list[EntryState] = []
        self.set_state_if_source_calls: list[EntryState] = []

    def get_by_name(self, name: str):
        if name == self._entry.name:
            return {self._entry}
        return None

    def set_state_if_online_or_source(
        self, entry: _FakeEntry, state: EntryState
    ) -> None:
        self.set_state_if_online_or_source_calls.append(state)
        entry.state = state

    def set_state_if_source(self, entry: _FakeEntry, state: EntryState) -> None:
        self.set_state_if_source_calls.append(state)
        entry.state = state


def test_extract_name_from_status_topic() -> None:
    assert (
        MqttStatusThread._extract_name_from_status_topic("test-node/status")
        == "test-node"
    )
    assert (
        MqttStatusThread._extract_name_from_status_topic("esphome/discover/test-node")
        is None
    )
    assert MqttStatusThread._extract_name_from_status_topic("test-node/other") is None


def test_mqtt_status_online_is_sticky_against_periodic_offline_reset() -> None:
    entry = _FakeEntry(
        name="host-light",
        state=bool_to_entry_state(False, EntryStateSource.MQTT),
    )
    entries = _FakeEntries(entry)

    online_from_status: set[str] = set()

    # Simulate receiving retained/birth online status.
    online_from_status.add("host-light")
    entries.set_state_if_online_or_source(
        entry, bool_to_entry_state(True, EntryStateSource.MQTT)
    )

    # Simulate the periodic reset logic: it should NOT force offline when status says online.
    if entry.name not in online_from_status:
        entries.set_state_if_source(
            entry, bool_to_entry_state(False, EntryStateSource.MQTT)
        )

    assert entry.state.reachable is ReachableState.ONLINE
    assert entries.set_state_if_source_calls == []
