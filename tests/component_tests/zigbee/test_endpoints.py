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
    # Avoid the import-time default model depending on a previous CORE.name.
    config["zigbee"].setdefault("model", "endpoint-test")
    CORE.config_path.write_text(yaml.safe_dump(config, sort_keys=False))
    CORE.config = read_config({})
    assert CORE.config is not None
    generate_cpp_contents(CORE.config)
    return CORE.cpp_global_section + CORE.cpp_main_section


def generate(tmp_path: Path, endpoint: str | float | None = None) -> str:
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


def assert_endpoint_ids(cpp: str, expected: list[int]) -> None:
    """Descriptors and entity wrappers must agree on every wire ID."""
    assert [int(x) for x in re.findall(r"->set_endpoint\((\d+)\)", cpp)] == expected
    assert [
        int(x) for x in re.findall(r"DECLARE_EP\(zigbee_ep\d+_ep, (\d+),", cpp)
    ] == expected


@pytest.mark.parametrize("domain", ["sensor", "binary_sensor", "number", "switch"])
@pytest.mark.parametrize("endpoint", [1, 17, 239])
def test_valid_entity_endpoint(tmp_path: Path, domain: str, endpoint: int) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    entity = config[domain][0]
    config = {key: config[key] for key in ("esphome", "nrf52", "zigbee")}
    entity["endpoint"] = endpoint
    config[domain] = [entity]
    assert_endpoint_ids(generate_config(tmp_path, config), [endpoint])


@pytest.mark.parametrize("endpoint", ["17", "0x11", 17.0])
def test_existing_integer_normalization(tmp_path: Path, endpoint: str | float) -> None:
    assert_endpoint_ids(generate(tmp_path, endpoint), [17])


@pytest.mark.parametrize("domain", ["sensor", "binary_sensor", "number", "switch"])
def test_esp32_boolean_endpoint_unchanged(tmp_path: Path, domain: str) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    del config["nrf52"]
    del config["time"]
    config["esp32"] = {"board": "esp32-c6-devkitc-1", "framework": {"type": "esp-idf"}}
    config[domain][0]["endpoint"] = True
    generate_config(tmp_path, config)
    assert CORE.config[domain][0]["endpoint"] is True
    assert "endpoint_ids" not in CORE.data.get("zigbee", {})


@pytest.mark.parametrize("endpoint", [1, 17])
def test_internal_endpoint_does_not_reserve(tmp_path: Path, endpoint: int) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    config["sensor"][1]["endpoint"] = endpoint  # Internal entity.
    config["sensor"][0]["endpoint"] = endpoint  # Visible entity with the same ID.
    expected = [1, 2, 3, 4, 5] if endpoint == 1 else [17, 1, 2, 3, 4]
    assert_endpoint_ids(generate_config(tmp_path, config), expected)
    del config["sensor"][0]["endpoint"]
    # Internal explicit IDs also must not displace automatic IDs.
    assert_endpoint_ids(generate_config(tmp_path, config), [1, 2, 3, 4, 5])


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


@pytest.mark.parametrize("domain", ["sensor", "binary_sensor", "number", "switch"])
@pytest.mark.parametrize("value", ["true", "false", "yes", "no"])
def test_boolean_endpoint(
    tmp_path: Path, capsys: pytest.CaptureFixture, domain: str, value: str
) -> None:
    config = yaml.safe_load((FIXTURES / "implicit.yaml").read_text())
    # Prove this fixture is valid before testing the rejected public YAML value.
    generate_config(tmp_path, config)
    capsys.readouterr()
    config[domain][0]["endpoint"] = yaml.safe_load(value)
    with pytest.raises(AssertionError):
        generate_config(tmp_path, config)
    assert "Endpoint must be an integer, not a boolean" in capsys.readouterr().out


@pytest.mark.parametrize("endpoint", [0, -1, *range(240, 256), 256, 1.5])
def test_invalid_endpoint(
    tmp_path: Path, endpoint: float, capsys: pytest.CaptureFixture
) -> None:
    generate(tmp_path, 17)
    capsys.readouterr()
    with pytest.raises(AssertionError):
        generate(tmp_path, endpoint)
    expected = (
        "must be at least 1"
        if endpoint < 1
        else "must be at most 239"
        if endpoint > 239
        else "only accepts integers with no fractional part"
    )
    output = capsys.readouterr().out
    assert expected in output
    assert "endpoint:" in output


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
        ([first, last], [17, 239]),
        ([last, implicit, first], [239, 1, 17]),
    ]:
        config["sensor"] = sensors
        cpp = generate_config(tmp_path, config)
        assert_endpoint_ids(cpp, expected)
