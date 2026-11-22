"""Exercise endpoint allocation through real validation and code generation."""

from pathlib import Path
import re

import pytest
import yaml

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE

FIXTURES = Path(__file__).parent / "fixtures"


def generate_config(tmp_path: Path, config: dict) -> str:
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    # Avoid the import-time default model depending on a previous CORE.name.
    config["zigbee"].setdefault("model", "endpoint-test")
    CORE.config_path.write_text(yaml.safe_dump(config, sort_keys=False))
    CORE.config = read_config({})
    assert CORE.config is not None
    generate_cpp_contents(CORE.config)
    return CORE.cpp_global_section + CORE.cpp_main_section


def assert_endpoint_ids(cpp: str, expected: list[int]) -> None:
    """Descriptors and entity wrappers must agree on every wire ID."""
    assert [int(x) for x in re.findall(r"->set_endpoint\((\d+)\)", cpp)] == expected
    assert [
        int(x) for x in re.findall(r"DECLARE_EP\(zigbee_ep\d+_ep, (\d+),", cpp)
    ] == expected


def entity_endpoints(cpp: str) -> dict[str, int]:
    """Resolve entity IDs through their generated wrappers, not emission order."""
    wrappers = dict(re.findall(r"new\((\w+)\) zigbee::Zigbee\w+\((\w+)\);", cpp))
    return {
        wrappers.get(wrapper, wrapper): int(endpoint)
        for wrapper, endpoint in re.findall(r"(\w+)->set_endpoint\((\d+)\)", cpp)
    }


@pytest.mark.parametrize("domain", ["sensor", "binary_sensor", "number", "switch"])
def test_internal_endpoint_does_not_reserve(tmp_path: Path, domain: str) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    visible = config[domain][0]
    visible["endpoint"] = 17
    internal = {**visible, "id": "internal_entity", "internal": True}
    config[domain].append(internal)
    cpp = generate_config(tmp_path, config)
    assert entity_endpoints(cpp)[visible["id"]] == 17
    assert "internal_entity" not in entity_endpoints(cpp)
    assert len(entity_endpoints(cpp)) == 5
    del visible["endpoint"]
    internal["endpoint"] = 1
    # Internal explicit IDs must not displace automatic IDs either.
    assert_endpoint_ids(generate_config(tmp_path, config), [1, 2, 3, 4, 5])


def test_common_mixed_endpoints(tmp_path: Path) -> None:
    """The shared build fixture exercises sparse IDs within the eight-slot limit."""
    common = Path(__file__).parents[2] / "components" / "zigbee" / "common.yaml"
    config = yaml.safe_load(common.read_text())
    config["esphome"]["name"] = "endpoint-test"
    config["nrf52"] = {"board": "xiao_ble"}
    config["zigbee"] = {"id": "radio"}
    cpp = generate_config(tmp_path, config)
    assert_endpoint_ids(cpp, [2, 3, 4, 17, 5, 1, 239, 6])
    assert [d.as_macro for d in CORE.defines if d.name == "ZIGBEE_ENDPOINTS_COUNT"] == [
        "#define ZIGBEE_ENDPOINTS_COUNT 8"
    ]


def test_default_nrf_numbering(tmp_path: Path) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    cpp = generate_config(tmp_path, config)
    assert_endpoint_ids(cpp, [1, 2, 3, 4, 5])
    assert entity_endpoints(cpp) == {
        "sample": 1,
        "contact": 2,
        "level": 3,
        "toggle": 4,
        "test_time": 5,
    }


def test_esp32_explicit_endpoint(tmp_path: Path) -> None:
    """ESP32 keeps its own allocation; the schema now only lifts the esp32 requirement."""
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    del config["nrf52"]
    config["esp32"] = {"board": "esp32-c6-devkitc-1", "framework": {"type": "esp-idf"}}
    config["sensor"][0]["endpoint"] = 17
    cpp = generate_config(tmp_path, config)
    assert [
        int(x) for x in re.findall(r"radio->create_default_cluster\((\d+),", cpp)
    ] == [17, 1]
    assert (
        "zigbee::ZigbeeAttribute(radio, 17, ::EZB_ZCL_CLUSTER_ID_ANALOG_INPUT," in cpp
    )


@pytest.mark.parametrize(
    "domain,endpoint,expected",
    [
        ("sensor", 1, [1, 2, 3, 4, 5]),
        ("binary_sensor", 1, [2, 1, 3, 4, 5]),
        ("number", 1, [2, 3, 1, 4, 5]),
        ("switch", 1, [2, 3, 4, 1, 5]),
        ("sensor", 17, [17, 1, 2, 3, 4]),
    ],
)
def test_reservations_and_time(
    tmp_path: Path, domain: str, endpoint: int, expected: list[int]
) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    config[domain][0]["endpoint"] = endpoint
    cpp = generate_config(tmp_path, config)
    assert_endpoint_ids(cpp, expected)
    assert entity_endpoints(cpp)[config[domain][0]["id"]] == endpoint
    assert f"test_time->set_endpoint({expected[-1]});" in cpp


def test_duplicate_endpoints(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    config["sensor"][0]["endpoint"] = 17
    config["number"][0]["endpoint"] = 17
    with pytest.raises(AssertionError):
        generate_config(tmp_path, config)
    assert "Duplicate Zigbee endpoint: 17" in capsys.readouterr().out


def test_explicit_ids_survive_reordering_and_insertion(tmp_path: Path) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    config = {key: config[key] for key in ("esphome", "nrf52", "zigbee")}
    first = {"platform": "template", "id": "first", "name": "First", "endpoint": 17}
    last = {"platform": "template", "id": "last", "name": "Last", "endpoint": 239}
    implicit = {"platform": "template", "id": "middle", "name": "Middle"}
    for sensors, expected in [
        ([first, last], {"first": 17, "last": 239}),
        ([last, implicit, first], {"last": 239, "middle": 1, "first": 17}),
    ]:
        config["sensor"] = sensors
        cpp = generate_config(tmp_path, config)
        assert_endpoint_ids(cpp, list(expected.values()))
        assert entity_endpoints(cpp) == expected


def test_write_handlers_follow_endpoints(tmp_path: Path) -> None:
    """Only switches, numbers and time handle writes, each matched by its own endpoint."""
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    config["switch"][0]["endpoint"] = 17
    cpp = generate_config(tmp_path, config)
    handlers = {
        wrapper: int(endpoint)
        for endpoint, wrapper in re.findall(
            r"esphome__arg_endpoint == (\d+) && \((\w+)->on_zcl_device_event", cpp
        )
    }
    endpoints = entity_endpoints(cpp)
    wrappers = dict(re.findall(r"new\((\w+)\) zigbee::Zigbee\w+\((\w+)\);", cpp))
    named = {wrappers.get(wrapper, wrapper): ep for wrapper, ep in handlers.items()}
    assert named == {name: endpoints[name] for name in ("level", "toggle", "test_time")}
    assert endpoints["toggle"] == 17
