"""Shared utilities for ESPHome integration tests - state handling."""

from __future__ import annotations

import asyncio
import logging
from typing import TypeVar

from aioesphomeapi import ButtonInfo, EntityInfo, EntityState

_LOGGER = logging.getLogger(__name__)

T = TypeVar("T", bound=EntityInfo)


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
        entity_names: List of entity names to search for in object_ids

    Returns:
        Dictionary mapping entity keys to entity names
    """
    key_to_entity: dict[int, str] = {}
    for entity in entities:
        obj_id = entity.object_id.lower()
        for entity_name in entity_names:
            if entity_name in obj_id:
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
