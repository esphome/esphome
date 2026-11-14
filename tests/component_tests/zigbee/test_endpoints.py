"""Exercise nRF endpoint allocation through real validation and code generation."""

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
    CORE.config_path.write_text(yaml.safe_dump(config, sort_keys=False))
    CORE.config = read_config({})
    assert CORE.config is not None
    generate_cpp_contents(CORE.config)
    return CORE.cpp_global_section + CORE.cpp_main_section


def generate(tmp_path: Path, endpoint: int | None = None) -> str:
    """Generate a simple sensor without any explicit cluster declarations."""
    sensor = {"platform": "template", "id": "sample", "name": "Sample"}
    if endpoint is not None:
        sensor["endpoint"] = endpoint
    config = {
        "esphome": {"name": "endpoint-test"},
        "nrf52": {"board": "xiao_ble"},
        "zigbee": {"id": "radio"},
        "sensor": [sensor],
    }
    return generate_config(tmp_path, config)


def test_implicit_sensor_endpoint(tmp_path: Path) -> None:
    cpp = generate(tmp_path)
    assert "ESPHOME_ZB_HA_DECLARE_EP(zigbee_ep1_ep, 1," in cpp
    assert re.findall(r"->set_endpoint\((\d+)\)", cpp) == ["1"]


def test_explicit_sensor_endpoint(tmp_path: Path) -> None:
    cpp = generate(tmp_path, 17)
    assert "ESPHOME_ZB_HA_DECLARE_EP(zigbee_ep1_ep, 17," in cpp
    assert re.findall(r"->set_endpoint\((\d+)\)", cpp) == ["17"]


def test_legacy_numbering_golden(tmp_path: Path) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    cpp = generate_config(tmp_path, config)
    lines = [
        line
        for line in cpp.splitlines()
        if any(
            token in line
            for token in (
                "ESPHOME_ZB_HA_DECLARE_EP(",
                "ZBOSS_DECLARE_DEVICE_CTX_EP_VA(",
                "->set_endpoint(",
                "->set_cluster_attributes(",
            )
        )
    ]
    assert "\n".join(lines) + "\n" == (FIXTURES / "implicit_endpoints.txt").read_text()


@pytest.mark.parametrize("fixture", ["esp32", "esp32-default"])
def test_esp32_numbering_golden(tmp_path: Path, fixture: str) -> None:
    config = yaml.safe_load((FIXTURES / f"{fixture}.yaml").read_text())
    cpp = generate_config(tmp_path, config)
    lines = [
        line
        for line in cpp.splitlines()
        if line.startswith(("radio->create_default_cluster(", "radio->add_"))
        or "zigbee::ZigbeeAttribute(radio," in line
        or "zigbee::ZigbeeTime(radio," in line
    ]
    assert (
        "\n".join(lines) + "\n" == (FIXTURES / f"{fixture}_endpoints.txt").read_text()
    )


@pytest.mark.parametrize("domain", ["sensor", "binary_sensor", "number", "switch"])
def test_explicit_entity_and_implicit_time(tmp_path: Path, domain: str) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    config[domain][0]["endpoint"] = 1
    cpp = generate_config(tmp_path, config)
    # Explicit reservation happens before any implicit entity is emitted.
    expected = {
        "sensor": [1, 2, 3, 4, 5],
        "binary_sensor": [2, 1, 3, 4, 5],
        "number": [2, 3, 1, 4, 5],
        "switch": [2, 3, 4, 1, 5],
    }[domain]
    assert [int(x) for x in re.findall(r"->set_endpoint\((\d+)\)", cpp)] == expected
    assert [
        int(x) for x in re.findall(r"DECLARE_EP\(zigbee_ep\d+_ep, (\d+),", cpp)
    ] == expected
    assert "test_time->set_endpoint(5);" in cpp


def test_time_uses_resolved_id(tmp_path: Path) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    config["sensor"][0]["endpoint"] = 17
    cpp = generate_config(tmp_path, config)
    assert re.findall(r"->set_endpoint\((\d+)\)", cpp) == ["17", "1", "2", "3", "4"]
    assert "ESPHOME_ZB_HA_DECLARE_EP(zigbee_ep5_ep, 4," in cpp
    assert "test_time->set_endpoint(4);" in cpp


@pytest.mark.parametrize("endpoint", [0, -1, *range(240, 256), 256, 1.5])
def test_invalid_endpoint(tmp_path: Path, endpoint: float) -> None:
    with pytest.raises(AssertionError):
        generate(tmp_path, endpoint)


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
        assert re.findall(r"->set_endpoint\((\d+)\)", cpp) == [
            str(17 + i) for i in range(count)
        ]


def test_explicit_ids_survive_reordering_and_insertion(tmp_path: Path) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    config = {key: config[key] for key in ("esphome", "nrf52", "zigbee")}
    first = {"platform": "template", "id": "first", "name": "First", "endpoint": 17}
    last = {"platform": "template", "id": "last", "name": "Last", "endpoint": 239}
    implicit = {"platform": "template", "id": "middle", "name": "Middle"}
    for sensors, expected in [
        ([first, last], [17, 239]),
        ([last, implicit, first], [239, 1, 17]),
    ]:
        config["sensor"] = sensors
        cpp = generate_config(tmp_path, config)
        assert re.findall(r"->set_endpoint\((\d+)\)", cpp) == list(map(str, expected))
