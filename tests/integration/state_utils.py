"""Shared utilities for ESPHome integration tests - state handling."""

from __future__ import annotations

import asyncio
from collections.abc import Callable
import logging
from typing import TypeVar

from aioesphomeapi import (
    APIClient,
    BinarySensorState,
    ButtonInfo,
    EntityInfo,
    EntityState,
    SensorState,
    TextSensorState,
)

_LOGGER = logging.getLogger(__name__)

T = TypeVar("T", bound=EntityInfo)
S = TypeVar("S", bound=EntityState)


async def wait_for_state(
    client: APIClient,
    predicate: Callable[[EntityState], bool],
    timeout: float = 5.0,
) -> EntityState:
    """Subscribe to states and wait for one matching ``predicate``.

    Resolves with the first :class:`EntityState` for which ``predicate``
    returns ``True``. Useful when a component publishes multiple states
    during setup (e.g. before sensor readings arrive) and the test needs
    to wait for the state to converge to expected values rather than
    capturing whichever state happens to arrive first.

    Args:
        client: Connected API client.
        predicate: Callable invoked for every received state; the first
            state for which it returns ``True`` is returned.
        timeout: Maximum time to wait in seconds.

    Returns:
        The first state matching ``predicate``.

    Raises:
        asyncio.TimeoutError: If no matching state arrives within ``timeout``.
    """
    future: asyncio.Future[EntityState] = asyncio.get_running_loop().create_future()

    def on_state(state: EntityState) -> None:
        if not future.done() and predicate(state):
            future.set_result(state)

    client.subscribe_states(on_state)
    return await asyncio.wait_for(future, timeout=timeout)


def find_entity(
    entities: list[EntityInfo],
    object_id_substring: str,
    entity_type: type[T] | None = None,
) -> T | EntityInfo | None:
    """Find an entity by object_id substring and optionally by type.

    Args:
        entities: List of entity info objects from the API
        object_id_substring: Substring to search for in object_id (case-insensitive)
        entity_type: Optional entity type to filter by (e.g., BinarySensorInfo)

    Returns:
        The first matching entity, or None if not found

    Example:
        binary_sensor = find_entity(entities, "test_binary_sensor", BinarySensorInfo)
        button = find_entity(entities, "set_true")  # Any entity type
    """
    substring_lower = object_id_substring.lower()
    for entity in entities:
        if substring_lower in entity.object_id.lower() and (
            entity_type is None or isinstance(entity, entity_type)
        ):
            return entity
    return None


def require_entity(
    entities: list[EntityInfo],
    object_id_substring: str,
    entity_type: type[T] | None = None,
    description: str | None = None,
) -> T | EntityInfo:
    """Find an entity or raise AssertionError if not found.

    Args:
        entities: List of entity info objects from the API
        object_id_substring: Substring to search for in object_id (case-insensitive)
        entity_type: Optional entity type to filter by (e.g., BinarySensorInfo)
        description: Human-readable description for error message

    Returns:
        The first matching entity

    Raises:
        AssertionError: If no matching entity is found

    Example:
        binary_sensor = require_entity(entities, "test_sensor", BinarySensorInfo)
        button = require_entity(entities, "set_true", description="Set True button")
    """
    entity = find_entity(entities, object_id_substring, entity_type)
    if entity is None:
        desc = description or f"entity with '{object_id_substring}' in object_id"
        type_info = f" of type {entity_type.__name__}" if entity_type else ""
        raise AssertionError(f"{desc}{type_info} not found in entities")
    return entity


def build_key_to_entity_mapping(
    entities: list[EntityInfo], entity_names: list[str]
) -> dict[int, str]:
    """Build a mapping from entity keys to entity names.

    Args:
        entities: List of entity info objects from the API
        entity_names: List of entity names to match exactly against object_ids

    Returns:
        Dictionary mapping entity keys to entity names
    """
    key_to_entity: dict[int, str] = {}
    for entity in entities:
        obj_id = entity.object_id.lower()
        for entity_name in entity_names:
            if entity_name == obj_id:
                key_to_entity[entity.key] = entity_name
                break
    return key_to_entity


class InitialStateHelper:
    """Helper to wait for initial states before processing test states.

    When an API client connects, ESPHome sends the current state of all entities.
    This helper wraps the user's state callback and swallows the first state for
    each entity, then forwards all subsequent states to the user callback.

    Usage:
        entities, services = await client.list_entities_services()
        helper = InitialStateHelper(entities)
        client.subscribe_states(helper.on_state_wrapper(user_callback))
        await helper.wait_for_initial_states()
        # Access initial states via helper.initial_states[key]
    """

    def __init__(self, entities: list[EntityInfo]) -> None:
        """Initialize the helper.

        Args:
            entities: All entities from list_entities_services()
        """
        # Set of (device_id, key) tuples waiting for initial state
        # Buttons are stateless, so exclude them
        self._wait_initial_states = {
            (entity.device_id, entity.key)
            for entity in entities
            if not isinstance(entity, ButtonInfo)
        }
        # Keep entity info for debugging - use (device_id, key) tuple
        self._entities_by_id = {
            (entity.device_id, entity.key): entity for entity in entities
        }
        # Store initial states by key for test access
        self.initial_states: dict[int, EntityState] = {}

        # Log all entities
        _LOGGER.debug(
            "InitialStateHelper: Found %d total entities: %s",
            len(entities),
            [(type(e).__name__, e.object_id) for e in entities],
        )

        # Log which ones we're waiting for
        _LOGGER.debug(
            "InitialStateHelper: Waiting for %d entities (excluding ButtonInfo): %s",
            len(self._wait_initial_states),
            [self._entities_by_id[k].object_id for k in self._wait_initial_states],
        )

        # Log which ones we're NOT waiting for
        not_waiting = {
            (e.device_id, e.key) for e in entities
        } - self._wait_initial_states
        if not_waiting:
            not_waiting_info = [
                f"{type(self._entities_by_id[k]).__name__}:{self._entities_by_id[k].object_id}"
                for k in not_waiting
            ]
            _LOGGER.debug(
                "InitialStateHelper: NOT waiting for %d entities: %s",
                len(not_waiting),
                not_waiting_info,
            )

        # Create future in the running event loop
        self._initial_states_received = asyncio.get_running_loop().create_future()
        # If no entities to wait for, mark complete immediately
        if not self._wait_initial_states:
            self._initial_states_received.set_result(True)

    def on_state_wrapper(self, user_callback):
        """Wrap a user callback to track initial states.

        Args:
            user_callback: The user's state callback function

        Returns:
            Wrapped callback that swallows first state per entity, forwards rest
        """

        def wrapper(state: EntityState) -> None:
            """Swallow initial state per entity, forward subsequent states."""
            # Create entity identifier tuple
            entity_id = (state.device_id, state.key)

            # Log which entity is sending state
            if entity_id in self._entities_by_id:
                entity = self._entities_by_id[entity_id]
                _LOGGER.debug(
                    "Received state for %s (type: %s, device_id: %s, key: %d)",
                    entity.object_id,
                    type(entity).__name__,
                    state.device_id,
                    state.key,
                )

            # If this entity is waiting for initial state
            if entity_id in self._wait_initial_states:
                # Store the initial state for test access
                self.initial_states[state.key] = state

                # Remove from waiting set
                self._wait_initial_states.discard(entity_id)

                _LOGGER.debug(
                    "Swallowed initial state for %s, %d entities remaining",
                    self._entities_by_id[entity_id].object_id
                    if entity_id in self._entities_by_id
                    else entity_id,
                    len(self._wait_initial_states),
                )

                # Check if we've now seen all entities
                if (
                    not self._wait_initial_states
                    and not self._initial_states_received.done()
                ):
                    _LOGGER.debug("All initial states received")
                    self._initial_states_received.set_result(True)

                # Don't forward initial state to user
                return

            # Forward subsequent states to user callback
            _LOGGER.debug("Forwarding state to user callback")
            user_callback(state)

        return wrapper

    async def wait_for_initial_states(self, timeout: float = 5.0) -> None:
        """Wait for all initial states to be received.

        Args:
            timeout: Maximum time to wait in seconds

        Raises:
            asyncio.TimeoutError: If initial states aren't received within timeout
        """
        await asyncio.wait_for(self._initial_states_received, timeout=timeout)


class SensorStateCollector:
    """Collects sensor, binary sensor, and text sensor state updates with wait helpers.

    Usage:
        collector = SensorStateCollector(
            sensor_names=["moving_distance", "still_distance"],
            binary_sensor_names=["has_target"],
            text_sensor_names=["direction"],
        )
        # Use collector.on_state as the callback (or wrap it)
        client.subscribe_states(helper.on_state_wrapper(collector.on_state))

        # Wait for all sensors to have at least one value
        await collector.wait_for_all(timeout=3.0)

        # Access collected states
        assert collector.sensor_states["moving_distance"][0] == approx(100.0)
        assert collector.text_sensor_states["direction"][0] == "Approaching"
    """

    def __init__(
        self,
        sensor_names: list[str],
        binary_sensor_names: list[str] | None = None,
        text_sensor_names: list[str] | None = None,
        entities: list[EntityInfo] | None = None,
    ) -> None:
        self.sensor_states: dict[str, list[float]] = {name: [] for name in sensor_names}
        self.binary_states: dict[str, list[bool]] = {
            name: [] for name in (binary_sensor_names or [])
        }
        self.text_sensor_states: dict[str, list[str]] = {
            name: [] for name in (text_sensor_names or [])
        }
        self._key_to_sensor: dict[int, str] = {}
        self._waiters: list[tuple[Callable[[], bool], asyncio.Future[bool]]] = []

        if entities is not None:
            self.build_key_mapping(entities)

    def build_key_mapping(self, entities: list[EntityInfo]) -> None:
        """Build key-to-name mapping from entities. Sorted by descending length."""
        all_names = (
            list(self.sensor_states.keys())
            + list(self.binary_states.keys())
            + list(self.text_sensor_states.keys())
        )
        all_names.sort(key=len, reverse=True)
        self._key_to_sensor = build_key_to_entity_mapping(entities, all_names)

    def on_state(self, state: EntityState) -> None:
        """Process a state update."""
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = self._key_to_sensor.get(state.key)
            if sensor_name and sensor_name in self.sensor_states:
                self.sensor_states[sensor_name].append(state.state)
                self._check_waiters()
        elif isinstance(state, BinarySensorState):
            sensor_name = self._key_to_sensor.get(state.key)
            if sensor_name and sensor_name in self.binary_states:
                self.binary_states[sensor_name].append(state.state)
                self._check_waiters()
        elif isinstance(state, TextSensorState) and not state.missing_state:
            sensor_name = self._key_to_sensor.get(state.key)
            if sensor_name and sensor_name in self.text_sensor_states:
                self.text_sensor_states[sensor_name].append(state.state)
                self._check_waiters()

    def _check_waiters(self) -> None:
        """Check all pending waiters and resolve any whose condition is met."""
        for condition, future in self._waiters:
            if not future.done() and condition():
                future.set_result(True)

    def _all_have_values(self) -> bool:
        """Check if all sensor, binary sensor, and text sensor lists have at least one value."""
        return (
            all(len(v) >= 1 for v in self.sensor_states.values())
            and all(len(v) >= 1 for v in self.binary_states.values())
            and all(len(v) >= 1 for v in self.text_sensor_states.values())
        )

    async def wait_for_all(self, timeout: float = 3.0) -> None:
        """Wait until all sensors and binary sensors have at least one value."""
        if self._all_have_values():
            return
        future: asyncio.Future[bool] = asyncio.get_running_loop().create_future()
        self._waiters.append((self._all_have_values, future))
        await asyncio.wait_for(future, timeout=timeout)

    def add_waiter(self, condition: Callable[[], bool]) -> asyncio.Future[bool]:
        """Add a custom waiter that resolves when condition returns True.

        Returns:
            A future that resolves when the condition is met.
        """
        future: asyncio.Future[bool] = asyncio.get_running_loop().create_future()
        if condition():
            future.set_result(True)
        else:
            self._waiters.append((condition, future))
        return future


class SensorTracker:
    """Data-driven sensor state tracker with expected-value futures.

    Tracks sensor state updates and resolves futures when sensors report
    specific expected values. Eliminates per-sensor future boilerplate.

    Usage::

        tracker = SensorTracker(["reg_u_word", "reg_s_word"])
        futures = tracker.expect_all({"reg_u_word": 99, "reg_s_word": -99})
        # ... subscribe_states with tracker.on_state, start scenario ...
        await tracker.await_all(futures)
    """

    def __init__(self, sensor_names: list[str]) -> None:
        self.sensor_states: dict[str, list[float]] = {name: [] for name in sensor_names}
        self.key_to_sensor: dict[int, str] = {}
        self._expectations: dict[str, list[tuple[object, asyncio.Future]]] = {}

    _ANY = object()  # Sentinel: match any value

    def expect(self, name: str, value: object) -> asyncio.Future:
        """Register an expected value for *name* and return a future for it."""
        future: asyncio.Future = asyncio.get_running_loop().create_future()
        self._expectations.setdefault(name, []).append((value, future))
        return future

    def expect_any(self, name: str) -> asyncio.Future:
        """Register a future that resolves on *any* state update for *name*."""
        return self.expect(name, self._ANY)

    def expect_all(self, expected: dict[str, object]) -> dict[str, asyncio.Future]:
        """Call ``expect`` for every entry and return a dict of futures."""
        return {name: self.expect(name, value) for name, value in expected.items()}

    def on_state(self, state: EntityState) -> None:
        """State callback suitable for ``subscribe_states``."""
        if not isinstance(state, SensorState) or state.missing_state:
            return
        sensor_name = self.key_to_sensor.get(state.key)
        if not sensor_name or sensor_name not in self.sensor_states:
            return
        self.sensor_states[sensor_name].append(state.state)
        for expected_value, future in self._expectations.get(sensor_name, []):
            if not future.done() and (
                expected_value is self._ANY or state.state == expected_value
            ):
                future.set_result(True)
                break

    async def await_change(
        self, future: asyncio.Future, name: str, timeout: float = 2.0
    ) -> None:
        """Wait for a sensor future to resolve; fail the test on timeout."""
        try:
            await asyncio.wait_for(future, timeout=timeout)
        except TimeoutError:
            import pytest

            pytest.fail(
                f"Timeout waiting for {name} change. Received sensor states:\n"
                f"  {name}: {self.sensor_states[name]}\n"
            )

    async def await_must_not_change(
        self, future: asyncio.Future, name: str, timeout: float = 2.0
    ) -> None:
        """Assert a sensor future does NOT resolve within the timeout."""
        try:
            await asyncio.wait_for(future, timeout=timeout)
        except TimeoutError:
            return  # Expected
        import pytest

        pytest.fail(
            f"{name} change should not have been triggered, but was. "
            f"Received sensor states:\n  {name}: {self.sensor_states[name]}\n"
        )

    async def await_all(
        self, futures: dict[str, asyncio.Future], timeout: float = 2.0
    ) -> None:
        """Await every future in *futures*, failing with per-sensor diagnostics."""
        for name, future in futures.items():
            await self.await_change(future, name, timeout=timeout)

    async def setup_and_start_scenario(self, client) -> list:
        """Wire up subscriptions, wait for initial states, press Start Scenario."""
        entities, _ = await client.list_entities_services()
        self.key_to_sensor.update(
            build_key_to_entity_mapping(entities, list(self.sensor_states.keys()))
        )
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(self.on_state))
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            import pytest

            pytest.fail("Timeout waiting for initial states")
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)
        return entities
