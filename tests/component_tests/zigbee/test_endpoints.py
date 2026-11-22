"""Exercise endpoint allocation through real validation and code generation."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest
import yaml

from esphome.config import read_config
from esphome.core import CORE

Config = dict
Build = Callable[[Config], str]

WRAPPER = re.compile(r"new\((\w+)\) zigbee::Zigbee\w+\((\w+)\);")


@pytest.fixture
def fixture_config(component_fixture_path: Callable[[str], Path]) -> Config:
    return yaml.safe_load(component_fixture_path("implicit.yaml").read_text())


@pytest.fixture
def build(tmp_path: Path, generate_main: Callable[[Path], str]) -> Build:
    def _build(config: Config) -> str:
        path = tmp_path / "test.yaml"
        path.write_text(yaml.safe_dump(config, sort_keys=False))
        return generate_main(path)

    return _build


def assert_endpoint_ids(cpp: str, expected: list[int]) -> None:
    """Descriptors and entity wrappers must agree on every wire ID."""
    assert [int(x) for x in re.findall(r"->set_endpoint\((\d+)\)", cpp)] == expected
    assert [
        int(x) for x in re.findall(r"DECLARE_EP\(zigbee_ep\d+_ep, (\d+),", cpp)
    ] == expected


def by_entity(cpp: str, pattern: str) -> dict[str, int]:
    """Map ``pattern``'s (variable, endpoint) matches to entity IDs through their wrappers."""
    wrappers = dict(WRAPPER.findall(cpp))
    return {
        wrappers.get(var, var): int(endpoint)
        for var, endpoint in re.findall(pattern, cpp)
    }


def entity_endpoints(cpp: str) -> dict[str, int]:
    return by_entity(cpp, r"(\w+)->set_endpoint\((\d+)\)")


@pytest.mark.parametrize("domain", ["sensor", "binary_sensor", "number", "switch"])
@pytest.mark.parametrize(
    "visible_endpoint,internal_endpoint,expected",
    [(17, None, [17, 1, 2, 3, 4]), (None, 1, [1, 2, 3, 4, 5])],
)
def test_internal_endpoint_does_not_reserve(
    fixture_config: Config,
    build: Build,
    domain: str,
    visible_endpoint: int | None,
    internal_endpoint: int | None,
    expected: list[int],
) -> None:
    visible = fixture_config[domain][0]
    internal = {**visible, "id": "internal_entity", "internal": True}
    if visible_endpoint is not None:
        visible["endpoint"] = visible_endpoint
    if internal_endpoint is not None:
        internal["endpoint"] = internal_endpoint
    fixture_config[domain].append(internal)
    cpp = build(fixture_config)
    endpoints = entity_endpoints(cpp)
    assert "internal_entity" not in endpoints
    assert sorted(endpoints.values()) == sorted(expected)
    if visible_endpoint is not None:
        assert endpoints[visible["id"]] == visible_endpoint


def test_common_mixed_endpoints(build: Build) -> None:
    """The shared build fixture exercises sparse IDs within the eight-slot limit."""
    common = Path(__file__).parents[2] / "components" / "zigbee" / "common.yaml"
    config = yaml.safe_load(common.read_text())
    config["esphome"]["name"] = "endpoint-test"
    config["nrf52"] = {"board": "xiao_ble"}
    config["zigbee"] = {"id": "radio", "model": "endpoint-test"}
    assert_endpoint_ids(build(config), [2, 3, 4, 17, 5, 1, 239, 6])


def test_default_nrf_numbering(fixture_config: Config, build: Build) -> None:
    cpp = build(fixture_config)
    assert_endpoint_ids(cpp, [1, 2, 3, 4, 5])
    assert entity_endpoints(cpp) == {
        "sample": 1,
        "contact": 2,
        "level": 3,
        "toggle": 4,
        "test_time": 5,
    }


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
    fixture_config: Config,
    build: Build,
    domain: str,
    endpoint: int,
    expected: list[int],
) -> None:
    fixture_config[domain][0]["endpoint"] = endpoint
    cpp = build(fixture_config)
    assert_endpoint_ids(cpp, expected)
    assert entity_endpoints(cpp)[fixture_config[domain][0]["id"]] == endpoint
    assert f"test_time->set_endpoint({expected[-1]});" in cpp


def test_duplicate_endpoints(
    fixture_config: Config, tmp_path: Path, capsys: pytest.CaptureFixture
) -> None:
    fixture_config["sensor"][0]["endpoint"] = 17
    fixture_config["number"][0]["endpoint"] = 17
    CORE.config_path = tmp_path / "test.yaml"
    CORE.config_path.write_text(yaml.safe_dump(fixture_config, sort_keys=False))
    assert read_config({}) is None
    assert "Duplicate Zigbee endpoint: 17" in capsys.readouterr().out


FIRST = {"platform": "template", "id": "first", "name": "First", "endpoint": 17}
LAST = {"platform": "template", "id": "last", "name": "Last", "endpoint": 239}
MIDDLE = {"platform": "template", "id": "middle", "name": "Middle"}


@pytest.mark.parametrize(
    "sensors,expected",
    [
        ([FIRST, LAST], {"first": 17, "last": 239}),
        ([LAST, MIDDLE, FIRST], {"last": 239, "middle": 1, "first": 17}),
    ],
)
def test_explicit_ids_survive_reordering_and_insertion(
    fixture_config: Config,
    build: Build,
    sensors: list[Config],
    expected: dict[str, int],
) -> None:
    config = {key: fixture_config[key] for key in ("esphome", "nrf52", "zigbee")}
    config["sensor"] = [dict(sensor) for sensor in sensors]
    cpp = build(config)
    assert_endpoint_ids(cpp, list(expected.values()))
    assert entity_endpoints(cpp) == expected


def test_write_handlers_follow_endpoints(fixture_config: Config, build: Build) -> None:
    """Only switches, numbers and time handle writes, each matched by its own endpoint."""
    fixture_config["switch"][0]["endpoint"] = 17
    cpp = build(fixture_config)
    handlers = {
        var: endpoint
        for endpoint, var in re.findall(
            r"esphome__arg_endpoint == (\d+) && \((\w+)->on_zcl_device_event", cpp
        )
    }
    endpoints = entity_endpoints(cpp)
    wrappers = dict(WRAPPER.findall(cpp))
    assert {wrappers.get(var, var): int(ep) for var, ep in handlers.items()} == {
        name: endpoints[name] for name in ("level", "toggle", "test_time")
    }
    assert endpoints["toggle"] == 17


def test_boolean_endpoint_generates_valid_cpp(
    fixture_config: Config, build: Build
) -> None:
    fixture_config["switch"][0]["endpoint"] = True
    cpp = build(fixture_config)
    assert not re.search(r"\bTrue\b", cpp)
    assert entity_endpoints(cpp)["toggle"] == 1
