"""Tests for the canopen component."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.canopen import CONFIG_SCHEMA, SDO_WRITE_SCHEMA
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError


def _config(**kwargs):
    return {"canbus_id": "can_bus", "node_id": 0x10, **kwargs}


def test_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Node options become setters, triggers become callbacks."""
    main_cpp = generate_main(component_config_path("canopen.yaml"))

    assert "canopen::CANopenComponent(can_bus, 16)" in main_cpp
    assert "canopen_node->set_heartbeat_interval(2000);" in main_cpp
    assert "canopen_node->set_start_operational(false);" in main_cpp
    for callback in (
        "add_on_nmt_state_callback",
        "add_on_heartbeat_callback",
        "add_on_emcy_callback",
    ):
        assert f"canopen_node->{callback}(" in main_cpp
    defines = {define.name for define in CORE.defines}
    assert {"USE_CANOPEN_HEARTBEAT", "USE_CANOPEN_EMCY"} <= defines
    assert "canopen_node->set_response_timeout(500);" in main_cpp
    # identity: unset fields are 0; device name / software version keep their C++ defaults
    assert "canopen_node->set_identity(0x12345678, 0x2A, 0x00, 0x00);" in main_cpp
    assert 'canopen_node->set_hardware_version("rev B");' in main_cpp
    assert "set_device_name(" not in main_cpp
    assert "set_software_version(" not in main_cpp
    # sdo_write: a negative value is passed as is, write_sdo() masks it to `size` bytes; no on_success: nullptr
    assert (
        "::canopen_node->write_sdo(1, 0x1017, 0x00, -1, 2, canopen::sdo_write_reply"
        "<nullptr, esphome__lambdaaction_id_4__fn>, nullptr);"
    ) in main_cpp


def test_defaults() -> None:
    config = CONFIG_SCHEMA(_config())
    assert config["heartbeat_interval"].total_milliseconds == 5000
    assert config["nmt_startup"] == "operational"
    assert config["identity"] == {
        "vendor_id": 0,
        "product_code": 0,
        "revision_number": 0,
        "serial_number": 0,
    }


@pytest.mark.parametrize("node_id", [0, 128])
def test_node_id_range(node_id: int) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_config(node_id=node_id))


@pytest.mark.parametrize(("timeout", "valid"), [("65535ms", True), ("65536ms", False)])
def test_response_timeout_fits_uint16(timeout: str, valid: bool) -> None:
    config = {"canbus_id": "can_bus", "response_timeout": timeout}
    if valid:
        CONFIG_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match="at most"):
            CONFIG_SCHEMA(config)


@pytest.mark.parametrize(
    ("interval", "error"),
    [("99ms", "at least"), ("100ms", None), ("65535ms", None), ("65536ms", "at most")],
)
def test_heartbeat_interval_range(interval: str, error: str | None) -> None:
    """At least 100 ms (the heartbeat is sent from loop()); 0x1017 holds uint16 milliseconds."""
    config = _config(heartbeat_interval=interval)
    if error is None:
        CONFIG_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match=error):
            CONFIG_SCHEMA(config)


SDO_WRITE = {"id": "canopen_node", "node_id": 1, "index": 0x1017}


@pytest.mark.parametrize(
    ("value", "size", "valid"),
    [
        (255, 1, True),
        (256, 1, False),
        (-128, 1, True),
        (-129, 1, False),
        (65535, 2, True),
        (0xFFFFFFFF, 4, True),
    ],
)
def test_sdo_write_value_fits_size(value: int, size: int, valid: bool) -> None:
    config = {**SDO_WRITE, "value": value, "size": size}
    if valid:
        SDO_WRITE_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match="doesn't fit"):
            SDO_WRITE_SCHEMA(config)


def test_local_nodes(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Nodes on the same canbus are linked into a ring (each receives the others' frames), per canbus."""
    main_cpp = generate_main(component_config_path("local.yaml"))

    links = [
        line.strip() for line in main_cpp.splitlines() if "set_next_local(" in line
    ]
    # in order; the last call per node wins: a -> b -> c -> a
    assert links == [
        "node_a->set_next_local(node_b);",
        "node_b->set_next_local(node_a);",
        "node_b->set_next_local(node_c);",
        "node_c->set_next_local(node_a);",
    ]
    assert "node_d->set_next_local(" not in main_cpp


def test_local_nodes_same_id(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Two nodes on one canbus can't share a node id."""
    CORE.config_path = component_config_path("local_same_id.yaml")
    assert read_config({}) is None
    assert "node_id 16 is also used by canopen" in capsys.readouterr().out


def test_client_only_hubs_same_bus(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Two hubs without node_id on one canbus: their SDO clients would use the same COB-IDs."""
    CORE.config_path = component_config_path("client_only_same_bus.yaml")
    assert read_config({}) is None
    assert "one SDO client only hub per canbus" in capsys.readouterr().out


def test_client_only_hub_per_bus(component_config_path: Callable[[str], Path]) -> None:
    """Nodes and one hub without node_id per canbus are fine."""
    CORE.config_path = component_config_path("client_only_per_bus.yaml")
    assert read_config({}) is not None


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def test_client_only_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without node_id the hub is an SDO client only: no node setters, the client compiled in."""
    main_cpp = generate_main(component_config_path("client_only.yaml"))
    assert "canopen::CANopenComponent(can_bus, 0)" in main_cpp
    assert "canopen_client->set_response_timeout(200);" in main_cpp
    assert "canopen_client->add_on_heartbeat_callback(" in main_cpp
    for setter in (
        "set_heartbeat_interval(",
        "set_identity(",
        "set_start_operational(",
    ):
        assert setter not in main_cpp
    assert "USE_CANOPEN_SDO_CLIENT" in _defines()
    # no own node id: no local transfers
    assert "-DCO_CONFIG_SDO_CLI=0x03" in CORE.build_flags
    assert "-DCO_CONFIG_SDO_CLI_BUFFER_SIZE=7" in CORE.build_flags
    # no hub with a node id: the node part isn't compiled (nor linked from CANopenNode)
    assert "USE_CANOPEN_NODE" not in _defines()
    # the actions call the hub; without automations no reply function
    assert (
        "::canopen_client->read_sdo(32, 0x1018, 0x01, 4, canopen::sdo_read_reply"
        "<esphome__lambdaaction_id__fn, esphome__lambdaaction_id_2__fn>, nullptr);"
    ) in main_cpp
    assert (
        "::canopen_client->write_sdo(32, 0x2000, 0x00, 1, 1, nullptr, nullptr);"
        in main_cpp
    )


@pytest.mark.parametrize(
    "option",
    [
        {"heartbeat_interval": "1s"},
        {"nmt_startup": "pre_operational"},
        {"identity": {"vendor_id": 1}},
        {"device_name": "x"},
    ],
)
def test_client_only_rejects_node_options(option: dict) -> None:
    with pytest.raises(cv.Invalid, match="needs a CANopen node"):
        CONFIG_SCHEMA({"canbus_id": "can_bus", **option})


def test_client_only_sdo_client_false() -> None:
    with pytest.raises(cv.Invalid, match="sdo_client: false without node_id"):
        CONFIG_SCHEMA({"canbus_id": "can_bus", "sdo_client": False})


def test_client_only_defaults() -> None:
    """The node options get their defaults only with node_id."""
    config = CONFIG_SCHEMA({"canbus_id": "can_bus"})
    assert not set(config) & {
        "node_id",
        "heartbeat_interval",
        "nmt_startup",
        "identity",
    }
    config = CONFIG_SCHEMA(_config())
    assert config["heartbeat_interval"].total_milliseconds == 5000


@pytest.mark.parametrize(
    ("yaml", "message"),
    [
        (
            "sdo_client_false_client_only.yaml",
            "sdo_client applies to every canopen hub of the firmware",
        ),
    ],
)
def test_client_only_invalid(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
    yaml: str,
    message: str,
) -> None:
    CORE.config_path = component_config_path(yaml)
    assert read_config({}) is None
    assert message in capsys.readouterr().out


@pytest.mark.parametrize(
    ("yaml", "message"),
    [
        (
            "sdo_client_false_used.yaml",
            "has sdo_client: false, but the SDO client is used",
        ),
    ],
)
def test_codegen_errors(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    yaml: str,
    message: str,
) -> None:
    with pytest.raises(EsphomeError, match=message):
        generate_main(component_config_path(yaml))


def test_sdo_client_auto(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A node compiles the client only if something uses it."""
    generate_main(component_config_path("node.yaml"))
    assert "USE_CANOPEN_SDO_CLIENT" not in _defines()


def test_sdo_client_true(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("sdo_client_true.yaml"))
    assert "USE_CANOPEN_SDO_CLIENT" in _defines()
    assert "USE_CANOPEN_NODE" in _defines()
    # a node serves requests to its own id from its OD
    assert "-DCO_CONFIG_SDO_CLI=0x0B" in CORE.build_flags
