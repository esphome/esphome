"""Unit tests for script/merge_component_configs.py deduplication."""

from pathlib import Path
import sys

import pytest

# Add the script directory to Python path so we can import the module
sys.path.insert(0, str((Path(__file__).parent / ".." / ".." / "script").resolve()))

import merge_component_configs  # noqa: E402

from esphome import yaml_util  # noqa: E402

deduplicate_by_id = merge_component_configs.deduplicate_by_id
prepare_component_body = merge_component_configs.prepare_component_body


def test_identical_duplicate_ids_collapse() -> None:
    """Two identical items sharing an id collapse to one without error."""
    data = {
        "sensor": [
            {"id": "shared", "platform": "template", "name": "A"},
            {"id": "shared", "platform": "template", "name": "A"},
        ]
    }
    result = deduplicate_by_id(data)
    assert result["sensor"] == [{"id": "shared", "platform": "template", "name": "A"}]


def test_conflicting_duplicate_ids_raise() -> None:
    """Two different items sharing an id is a hard error naming the id."""
    data = {
        "sensor": [
            {"id": "dup", "platform": "template", "name": "A"},
            {"id": "dup", "platform": "template", "name": "B"},
        ]
    }
    with pytest.raises(ValueError, match="dup"):
        deduplicate_by_id(data)


def test_intentionally_shared_id_does_not_raise() -> None:
    """An allowlisted (section, id) may differ across components and collapse."""
    section, id_ = "time", "sntp_time"
    assert (section, id_) in merge_component_configs.INTENTIONALLY_SHARED_IDS
    data = {
        section: [
            {"id": id_, "platform": "sntp"},
            {"id": id_, "platform": "sntp", "servers": ["a"]},
        ]
    }
    result = deduplicate_by_id(data)
    # First occurrence wins, no error raised
    assert result[section] == [{"id": id_, "platform": "sntp"}]


def test_allowlisted_id_in_other_section_still_raises() -> None:
    """The allowlist is keyed on (section, id): the same id elsewhere conflicts."""
    data = {
        "sensor": [
            {"id": "sntp_time", "platform": "a"},
            {"id": "sntp_time", "platform": "b"},
        ]
    }
    with pytest.raises(ValueError, match="sntp_time"):
        deduplicate_by_id(data)


def test_items_without_id_are_preserved() -> None:
    """Items lacking an id are passed through untouched."""
    data = {"binary_sensor": [{"platform": "gpio"}, {"platform": "gpio"}]}
    result = deduplicate_by_id(data)
    assert result["binary_sensor"] == [{"platform": "gpio"}, {"platform": "gpio"}]


def test_comparison_is_type_sensitive() -> None:
    """Comparison matches the merge exactly: 5 and "5" are a conflict.

    The duplicate-id CI guard reuses this function, so a looser (e.g. string
    normalized) comparison would let the guard disagree with the build.
    """
    data = {
        "sensor": [
            {"id": "dup", "platform": "adc", "pin": 5},
            {"id": "dup", "platform": "adc", "pin": "5"},
        ]
    }
    with pytest.raises(ValueError, match="dup"):
        deduplicate_by_id(data)


def test_nested_lists_are_checked() -> None:
    """Conflicts nested inside dict values are also detected."""
    data = {
        "wrapper": {
            "sensor": [
                {"id": "dup", "value": 1},
                {"id": "dup", "value": 2},
            ]
        }
    }
    with pytest.raises(ValueError, match="dup"):
        deduplicate_by_id(data)


def test_nested_package_includes_are_fully_expanded(tmp_path: Path) -> None:
    """A package include that itself pulls in another package expands fully.

    Mirrors web_server's tests, where test.yaml includes common_v2, which
    includes common (holding the wifi/network config). Without recursive
    expansion the nested include is dropped and network config is lost.
    """
    (tmp_path / "common.yaml").write_text("wifi:\n  ssid: MySSID\n")
    (tmp_path / "common_v2.yaml").write_text(
        "packages:\n  device_base: !include common.yaml\nweb_server:\n  port: 8080\n"
    )
    (tmp_path / "test.yaml").write_text(
        "packages:\n  web_server: !include common_v2.yaml\n"
        "web_server:\n  auth:\n    username: admin\n"
    )

    comp_data = yaml_util.load_yaml(tmp_path / "test.yaml")
    result = prepare_component_body(comp_data, "web_server", tmp_path)

    assert "packages" not in result
    assert result["wifi"] == {"ssid": "MySSID"}
    assert result["web_server"] == {"port": 8080, "auth": {"username": "admin"}}


def test_common_bus_package_is_left_for_caller(tmp_path: Path) -> None:
    """Common bus packages are not expanded inline; the caller re-adds them."""
    comp_data = {
        "packages": {
            "i2c": {"sda": 21, "scl": 22},
            "device_base": {"wifi": {"ssid": "MySSID"}},
        },
    }
    result = prepare_component_body(comp_data, "mycomp", tmp_path)

    # The bus package's body must not be merged in, and the packages key is
    # dropped entirely for the caller to re-add the common bus package.
    assert "packages" not in result
    assert "sda" not in result
    assert result["wifi"] == {"ssid": "MySSID"}


def test_list_style_packages_are_expanded(tmp_path: Path) -> None:
    """List-style package includes are expanded and the key removed."""
    (tmp_path / "common.yaml").write_text("wifi:\n  ssid: MySSID\n")
    (tmp_path / "test.yaml").write_text("packages:\n  - !include common.yaml\n")

    comp_data = yaml_util.load_yaml(tmp_path / "test.yaml")
    result = prepare_component_body(comp_data, "mycomp", tmp_path)

    assert "packages" not in result
    assert result["wifi"] == {"ssid": "MySSID"}
