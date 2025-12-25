"""Shared utilities for ESPHome integration tests - state handling."""

from __future__ import annotations

import asyncio
import logging

from aioesphomeapi import ButtonInfo, EntityInfo, EntityState

_LOGGER = logging.getLogger(__name__)


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
