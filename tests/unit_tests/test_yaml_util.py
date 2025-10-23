from pathlib import Path
import shutil
from unittest.mock import patch

import pytest

from esphome import core, yaml_util
from esphome.components import substitutions
from esphome.core import EsphomeError
from esphome.util import OrderedDict


@pytest.fixture(autouse=True)
def clear_secrets_cache() -> None:
    """Clear the secrets cache before each test."""
    yaml_util._SECRET_VALUES.clear()
    yaml_util._SECRET_CACHE.clear()
    yield
    yaml_util._SECRET_VALUES.clear()
    yaml_util._SECRET_CACHE.clear()


def test_include_with_vars(fixture_path: Path) -> None:
    yaml_file = fixture_path / "yaml_util" / "includetest.yaml"

    actual = yaml_util.load_yaml(yaml_file)
    substitutions.do_substitution_pass(actual, None)
    assert actual["esphome"]["name"] == "original"
    assert actual["esphome"]["libraries"][0] == "Wire"
    assert actual["esp8266"]["board"] == "nodemcu"
    assert actual["wifi"]["ssid"] == "my_custom_ssid"


def test_loading_a_broken_yaml_file(fixture_path):
    """Ensure we fallback to pure python to give good errors."""
    yaml_file = fixture_path / "yaml_util" / "broken_includetest.yaml"

    try:
        yaml_util.load_yaml(yaml_file)
    except EsphomeError as err:
        assert "broken_included.yaml" in str(err)


def test_loading_a_yaml_file_with_a_missing_component(fixture_path):
    """Ensure we show the filename for a yaml file with a missing component."""
    yaml_file = fixture_path / "yaml_util" / "missing_comp.yaml"

    try:
        yaml_util.load_yaml(yaml_file)
    except EsphomeError as err:
        assert "missing_comp.yaml" in str(err)


def test_loading_a_missing_file(fixture_path):
    """We throw EsphomeError when loading a missing file."""
    yaml_file = fixture_path / "yaml_util" / "missing.yaml"

    try:
        yaml_util.load_yaml(yaml_file)
    except EsphomeError as err:
        assert "missing.yaml" in str(err)


def test_parsing_with_custom_loader(fixture_path):
    """Test custom loader used for vscode connection
    Default loader is tested in test_include_with_vars
    """
    yaml_file = fixture_path / "yaml_util" / "includetest.yaml"

    loader_calls: list[Path] = []

    def custom_loader(fname: Path):
        loader_calls.append(fname)

    with yaml_file.open(encoding="utf-8") as f_handle:
        yaml_util.parse_yaml(yaml_file, f_handle, custom_loader)

    assert len(loader_calls) == 3
    assert loader_calls[0].parts[-2:] == ("includes", "included.yaml")
    assert loader_calls[1].parts[-2:] == ("includes", "list.yaml")
    assert loader_calls[2].parts[-2:] == ("includes", "scalar.yaml")


def test_construct_secret_simple(fixture_path: Path) -> None:
    """Test loading a YAML file with !secret tags."""
    yaml_file = fixture_path / "yaml_util" / "test_secret.yaml"

    actual = yaml_util.load_yaml(yaml_file)

    # Check that secrets were properly loaded
    assert actual["wifi"]["password"] == "super_secret_wifi"
    assert actual["api"]["encryption"]["key"] == "0123456789abcdef"
    assert actual["sensor"][0]["id"] == "my_secret_value"


def test_construct_secret_missing(fixture_path: Path, tmp_path: Path) -> None:
    """Test that missing secrets raise proper errors."""
    # Create a YAML file with a secret that doesn't exist
    test_yaml = tmp_path / "test.yaml"
    test_yaml.write_text("""
esphome:
  name: test

wifi:
  password: !secret nonexistent_secret
""")

    # Create an empty secrets file
    secrets_yaml = tmp_path / "secrets.yaml"
    secrets_yaml.write_text("some_other_secret: value")

    with pytest.raises(EsphomeError, match="Secret 'nonexistent_secret' not defined"):
        yaml_util.load_yaml(test_yaml)


def test_construct_secret_no_secrets_file(tmp_path: Path) -> None:
    """Test that missing secrets.yaml file raises proper error."""
    # Create a YAML file with a secret but no secrets.yaml
    test_yaml = tmp_path / "test.yaml"
    test_yaml.write_text("""
wifi:
  password: !secret some_secret
""")

    # Mock CORE.config_path to avoid NoneType error
    with (
        patch.object(core.CORE, "config_path", tmp_path / "main.yaml"),
        pytest.raises(EsphomeError, match="secrets.yaml"),
    ):
        yaml_util.load_yaml(test_yaml)


def test_construct_secret_fallback_to_main_config_dir(
    fixture_path: Path, tmp_path: Path
) -> None:
    """Test fallback to main config directory for secrets."""
    # Create a subdirectory with a YAML file that uses secrets
    subdir = tmp_path / "subdir"
    subdir.mkdir()

    test_yaml = subdir / "test.yaml"
    test_yaml.write_text("""
wifi:
  password: !secret test_secret
""")

    # Create secrets.yaml in the main directory
    main_secrets = tmp_path / "secrets.yaml"
    main_secrets.write_text("test_secret: main_secret_value")

    # Mock CORE.config_path to point to main directory
    with patch.object(core.CORE, "config_path", tmp_path / "main.yaml"):
        actual = yaml_util.load_yaml(test_yaml)
        assert actual["wifi"]["password"] == "main_secret_value"


def test_construct_include_dir_named(fixture_path: Path, tmp_path: Path) -> None:
    """Test !include_dir_named directive."""
    # Copy fixture directory to temporary location
    src_dir = fixture_path / "yaml_util"
    dst_dir = tmp_path / "yaml_util"
    shutil.copytree(src_dir, dst_dir)

    # Create test YAML that uses include_dir_named
    test_yaml = dst_dir / "test_include_named.yaml"
    test_yaml.write_text("""
sensor: !include_dir_named named_dir
""")

    actual = yaml_util.load_yaml(test_yaml)
    actual_sensor = actual["sensor"]

    # Check that files were loaded with their names as keys
    assert isinstance(actual_sensor, OrderedDict)
    assert "sensor1" in actual_sensor
    assert "sensor2" in actual_sensor
    assert "sensor3" in actual_sensor  # Files from subdirs are included with basename

    # Check content of loaded files
    assert actual_sensor["sensor1"]["platform"] == "template"
    assert actual_sensor["sensor1"]["name"] == "Sensor 1"
    assert actual_sensor["sensor2"]["platform"] == "template"
    assert actual_sensor["sensor2"]["name"] == "Sensor 2"

    # Check that subdirectory files are included with their basename
    assert actual_sensor["sensor3"]["platform"] == "template"
    assert actual_sensor["sensor3"]["name"] == "Sensor 3 in subdir"

    # Check that hidden files and non-YAML files are not included
    assert ".hidden" not in actual_sensor
    assert "not_yaml" not in actual_sensor


def test_construct_include_dir_named_empty_dir(tmp_path: Path) -> None:
    """Test !include_dir_named with empty directory."""
    # Create empty directory
    empty_dir = tmp_path / "empty_dir"
    empty_dir.mkdir()

    test_yaml = tmp_path / "test.yaml"
    test_yaml.write_text("""
sensor: !include_dir_named empty_dir
""")

    actual = yaml_util.load_yaml(test_yaml)

    # Should return empty OrderedDict
    assert isinstance(actual["sensor"], OrderedDict)
    assert len(actual["sensor"]) == 0


def test_construct_include_dir_named_with_dots(tmp_path: Path) -> None:
    """Test that include_dir_named ignores files starting with dots."""
    # Create directory with various files
    test_dir = tmp_path / "test_dir"
    test_dir.mkdir()

    # Create visible file
    visible_file = test_dir / "visible.yaml"
    visible_file.write_text("key: visible_value")

    # Create hidden file
    hidden_file = test_dir / ".hidden.yaml"
    hidden_file.write_text("key: hidden_value")

    # Create hidden directory with files
    hidden_dir = test_dir / ".hidden_dir"
    hidden_dir.mkdir()
    hidden_subfile = hidden_dir / "subfile.yaml"
    hidden_subfile.write_text("key: hidden_subfile_value")

    test_yaml = tmp_path / "test.yaml"
    test_yaml.write_text("""
test: !include_dir_named test_dir
""")

    actual = yaml_util.load_yaml(test_yaml)

    # Should only include visible file
    assert "visible" in actual["test"]
    assert actual["test"]["visible"]["key"] == "visible_value"

    # Should not include hidden files or directories
    assert ".hidden" not in actual["test"]
    assert ".hidden_dir" not in actual["test"]


def test_find_files_recursive(fixture_path: Path, tmp_path: Path) -> None:
    """Test that _find_files works recursively through include_dir_named."""
    # Copy fixture directory to temporary location
    src_dir = fixture_path / "yaml_util"
    dst_dir = tmp_path / "yaml_util"
    shutil.copytree(src_dir, dst_dir)

    # This indirectly tests _find_files by using include_dir_named
    test_yaml = dst_dir / "test_include_recursive.yaml"
    test_yaml.write_text("""
all_sensors: !include_dir_named named_dir
""")

    actual = yaml_util.load_yaml(test_yaml)

    # Should find sensor1.yaml, sensor2.yaml, and subdir/sensor3.yaml (all flattened)
    assert len(actual["all_sensors"]) == 3
    assert "sensor1" in actual["all_sensors"]
    assert "sensor2" in actual["all_sensors"]
    assert "sensor3" in actual["all_sensors"]


def test_secret_values_tracking(fixture_path: Path) -> None:
    """Test that secret values are properly tracked for dumping."""
    yaml_file = fixture_path / "yaml_util" / "test_secret.yaml"

    yaml_util.load_yaml(yaml_file)

    # Check that secret values are tracked
    assert "super_secret_wifi" in yaml_util._SECRET_VALUES
    assert yaml_util._SECRET_VALUES["super_secret_wifi"] == "wifi_password"
    assert "0123456789abcdef" in yaml_util._SECRET_VALUES
    assert yaml_util._SECRET_VALUES["0123456789abcdef"] == "api_key"
