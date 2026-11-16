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


@pytest.mark.parametrize("fixture", ["implicit", "esp32-default"])
@pytest.mark.parametrize("domain", ["sensor", "binary_sensor", "number", "switch"])
def test_valid_entity_endpoint(tmp_path: Path, domain: str, fixture: str) -> None:
    config = yaml.safe_load((FIXTURES / f"{fixture}.yaml").read_text())
    entity = config[domain][0]
    config = {
        key: value
        for key, value in config.items()
        if key in ("esphome", "nrf52", "esp32", "zigbee")
    }
    entity["endpoint"] = 17
    config[domain] = [entity]
    cpp = generate_config(tmp_path, config)
    if fixture == "implicit":
        assert_endpoint_ids(cpp, [17])
        assert entity_endpoints(cpp) == {entity["id"]: 17}
    else:
        assert "radio->create_default_cluster(17," in cpp
        assert "zigbee::ZigbeeAttribute(radio, 17," in cpp


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


@pytest.mark.parametrize(
    "fixture,sensor_endpoint", [("esp32", 17), ("esp32-default", 1)]
)
def test_esp32_numbering(tmp_path: Path, fixture: str, sensor_endpoint: int) -> None:
    config = yaml.safe_load((FIXTURES / f"{fixture}.yaml").read_text())
    cpp = generate_config(tmp_path, config)
    assert [
        int(x) for x in re.findall(r"radio->create_default_cluster\((\d+),", cpp)
    ] == ([17, 1] if sensor_endpoint == 17 else [1])
    bindings = [
        (str(sensor_endpoint), "ANALOG_INPUT"),
        ("1", "BINARY_INPUT"),
        ("1", "ANALOG_OUTPUT"),
        ("1", "BINARY_OUTPUT"),
    ]
    assert (
        re.findall(
            r"zigbee::ZigbeeAttribute\(radio, (\d+), ::EZB_ZCL_CLUSTER_ID_(\w+),", cpp
        )
        == bindings
    )
    assert re.findall(
        r"radio->add_cluster\((\d+), ::EZB_ZCL_CLUSTER_ID_(\w+), EZB_ZCL_CLUSTER_SERVER\)",
        cpp,
    ) == [*bindings, ("1", "TIME")]
    assert re.findall(
        r"radio->add_cluster\((\d+), ::EZB_ZCL_CLUSTER_ID_TIME, EZB_ZCL_CLUSTER_(\w+)\)",
        cpp,
    ) == [("1", "CLIENT"), ("1", "SERVER")]
    assert "zigbee::ZigbeeTime(radio, 1)" in cpp


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


@pytest.mark.parametrize("fixture", ["implicit", "esp32-default"])
def test_boolean_endpoint_wiring(
    tmp_path: Path, capsys: pytest.CaptureFixture, fixture: str
) -> None:
    config = yaml.safe_load((FIXTURES / f"{fixture}.yaml").read_text())
    generate_config(tmp_path, config)
    capsys.readouterr()
    config["sensor"][0]["endpoint"] = True
    with pytest.raises(AssertionError):
        generate_config(tmp_path, config)
    assert "Endpoint must be an integer, not a boolean" in capsys.readouterr().out


def test_duplicate_endpoints(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    config["sensor"][0]["endpoint"] = 17
    config["number"][0]["endpoint"] = 17
    with pytest.raises(AssertionError):
        generate_config(tmp_path, config)
    assert "Duplicate Zigbee endpoint: 17" in capsys.readouterr().out


@pytest.mark.parametrize("count", [8, 9])
def test_endpoint_capacity(
    tmp_path: Path, count: int, capsys: pytest.CaptureFixture
) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    config = {key: config[key] for key in ("esphome", "nrf52", "zigbee")}
    config["sensor"] = [
        {"platform": "template", "name": f"Sample {i}", "endpoint": 17 + i}
        for i in range(count)
    ]
    if count == 9:
        with pytest.raises(AssertionError):
            generate_config(tmp_path, config)
        assert "Maximum number of end points is 8" in capsys.readouterr().out
    else:
        cpp = generate_config(tmp_path, config)
        assert_endpoint_ids(cpp, list(range(17, 17 + count)))
        assert [
            d.as_macro for d in CORE.defines if d.name == "ZIGBEE_ENDPOINTS_COUNT"
        ] == ["#define ZIGBEE_ENDPOINTS_COUNT 8"]


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
