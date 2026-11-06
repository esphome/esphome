"""Unit tests for the entity_transport component."""

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_PROVIDER, CONF_REMOTE_ID
from esphome.components.entity_transport import (
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    remote_entity_schema,
)
from esphome.components.sensor import sensor_schema
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_TIMEOUT,
    CONF_UPDATE_INTERVAL,
    SCHEDULER_DONT_RUN,
)
from esphome.core import ID
import esphome.final_validate as fv


def test_remote_id_required_without_manual_id(setup_core) -> None:
    """A remote entity needs remote_id unless its own id is given."""
    schema = remote_entity_schema(sensor_schema())
    with pytest.raises(cv.Invalid, match="remote_id"):
        schema({CONF_NAME: "Temp", CONF_PROVIDER: "remote-device"})
    assert schema(
        {CONF_NAME: "Temp 2", CONF_PROVIDER: "remote-device", CONF_REMOTE_ID: "temp"}
    )
    config = schema(
        {CONF_PROVIDER: "remote-device", CONF_ID: ID("temp", is_manual=True)}
    )
    assert config[CONF_ID].id == "temp"


def test_update_interval_never(setup_core) -> None:
    """update_interval accepts never, and is left out when not given."""
    base = {"packet_interface": "link"}
    assert CONF_UPDATE_INTERVAL not in CONFIG_SCHEMA(base)
    config = CONFIG_SCHEMA({**base, CONF_UPDATE_INTERVAL: "never"})
    assert config[CONF_UPDATE_INTERVAL].total_milliseconds == SCHEDULER_DONT_RUN
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**base, CONF_UPDATE_INTERVAL: "500ms"})


def test_timeout(setup_core) -> None:
    """The timeout is optional, and at least a second."""
    base = {"packet_interface": "link"}
    assert CONF_TIMEOUT not in CONFIG_SCHEMA(base)
    assert (
        CONFIG_SCHEMA({**base, CONF_TIMEOUT: "1min"})[CONF_TIMEOUT].total_seconds == 60
    )
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**base, CONF_TIMEOUT: "500ms"})


@pytest.mark.parametrize(
    ("layers", "allowed"),
    [({}, True), ({"reliability": None}, True), ({"session": {}}, False)],
)
def test_timeout_not_with_session(setup_core, layers: dict, allowed: bool) -> None:
    """With a session layer, unchanged states are never sent again, so a timeout would clear them on a working link."""
    config = CONFIG_SCHEMA({"packet_interface": "link", CONF_TIMEOUT: "1min"})
    token = fv.full_config.set(
        {"packet_interface": [{CONF_ID: ID("link", is_manual=True), **layers}]}
    )
    try:
        if allowed:
            FINAL_VALIDATE_SCHEMA(config)
        else:
            with pytest.raises(cv.Invalid, match="session") as error:
                FINAL_VALIDATE_SCHEMA(config)
            assert error.value.path == [CONF_TIMEOUT]
        # Without a timeout, any layers are fine
        FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA({"packet_interface": "link"}))
    finally:
        fv.full_config.reset(token)


def test_timeout_not_with_session_on_peer(setup_core) -> None:
    """The layers of an addressing peer are found by the peer's id."""
    peers = [
        {CONF_ID: ID("plain", is_manual=True)},
        {CONF_ID: ID("with_session", is_manual=True), "session": {}},
    ]
    token = fv.full_config.set(
        {
            "packet_interface": [
                {CONF_ID: ID("bus", is_manual=True), "addressing": {"peers": peers}}
            ]
        }
    )
    try:
        FINAL_VALIDATE_SCHEMA(
            CONFIG_SCHEMA({"packet_interface": "plain", CONF_TIMEOUT: "1min"})
        )
        with pytest.raises(cv.Invalid, match="session"):
            FINAL_VALIDATE_SCHEMA(
                CONFIG_SCHEMA(
                    {"packet_interface": "with_session", CONF_TIMEOUT: "1min"}
                )
            )
    finally:
        fv.full_config.reset(token)


def test_advertised_ids_unique(setup_core) -> None:
    """Two entities of one type cannot be sent with the same id."""
    base = {"packet_interface": "link"}
    assert CONFIG_SCHEMA({**base, "sensors": ["a", {"id": "b", "broadcast_id": "c"}]})
    with pytest.raises(cv.Invalid, match="'a' is sent for two entities"):
        CONFIG_SCHEMA({**base, "sensors": ["a", {"id": "b", "broadcast_id": "a"}]})
    # Sensors and binary sensors are numbered separately, so may share an id
    assert CONFIG_SCHEMA({**base, "sensors": ["a"], "binary_sensors": ["a"]})
