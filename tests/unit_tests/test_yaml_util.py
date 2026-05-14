import io
from pathlib import Path
import shutil
from unittest.mock import patch

import pytest

from esphome import core, yaml_util
from esphome.components import substitutions
from esphome.config_helpers import Extend, Remove
import esphome.config_validation as cv
from esphome.core import DocumentLocation, DocumentRange, EsphomeError
from esphome.util import OrderedDict
from esphome.yaml_util import (
    ESPHomeDataBase,
    ESPLiteralValue,
    format_path,
    make_data_base,
    make_literal,
)


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
    actual = substitutions.do_substitution_pass(actual, None)
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
        config = yaml_util.parse_yaml(yaml_file, f_handle, custom_loader)
        # substitute config to expand includes:
        substitutions.substitute(config, [], substitutions.ContextVars(), False)

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
    test_yaml.write_text(
        """
esphome:
  name: test

wifi:
  password: !secret nonexistent_secret
"""
    )

    # Create an empty secrets file
    secrets_yaml = tmp_path / "secrets.yaml"
    secrets_yaml.write_text("some_other_secret: value")

    with pytest.raises(EsphomeError, match="Secret 'nonexistent_secret' not defined"):
        yaml_util.load_yaml(test_yaml)


def test_construct_secret_no_secrets_file(tmp_path: Path) -> None:
    """Test that missing secrets.yaml file raises proper error."""
    # Create a YAML file with a secret but no secrets.yaml
    test_yaml = tmp_path / "test.yaml"
    test_yaml.write_text(
        """
wifi:
  password: !secret some_secret
"""
    )

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
    test_yaml.write_text(
        """
wifi:
  password: !secret test_secret
"""
    )

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
    test_yaml.write_text(
        """
sensor: !include_dir_named named_dir
"""
    )

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
    test_yaml.write_text(
        """
sensor: !include_dir_named empty_dir
"""
    )

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
    test_yaml.write_text(
        """
test: !include_dir_named test_dir
"""
    )

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
    test_yaml.write_text(
        """
all_sensors: !include_dir_named named_dir
"""
    )

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


def test_dump_sort_keys() -> None:
    """Test that dump with sort_keys=True produces sorted output."""
    # Create a dict with unsorted keys
    data = {
        "zebra": 1,
        "alpha": 2,
        "nested": {
            "z_key": "z_value",
            "a_key": "a_value",
        },
    }

    # Without sort_keys, keys are in insertion order
    unsorted = yaml_util.dump(data, sort_keys=False)
    lines_unsorted = unsorted.strip().split("\n")
    # First key should be "zebra" (insertion order)
    assert lines_unsorted[0].startswith("zebra:")

    # With sort_keys, keys are alphabetically sorted
    sorted_dump = yaml_util.dump(data, sort_keys=True)
    lines_sorted = sorted_dump.strip().split("\n")
    # First key should be "alpha" (alphabetical order)
    assert lines_sorted[0].startswith("alpha:")
    # nested keys should also be sorted
    assert "a_key:" in sorted_dump
    assert sorted_dump.index("a_key:") < sorted_dump.index("z_key:")


# ---------------------------------------------------------------------------
# track_yaml_loads
# ---------------------------------------------------------------------------


def test_track_yaml_loads_records_files(tmp_path: Path) -> None:
    """track_yaml_loads records every file loaded inside the context."""
    yaml_file = tmp_path / "test.yaml"
    yaml_file.write_text("key: value\n")

    with yaml_util.track_yaml_loads() as loaded:
        yaml_util.load_yaml(yaml_file)

    assert len(loaded) == 1
    assert loaded[0] == yaml_file.resolve()


def test_track_yaml_loads_records_includes(tmp_path: Path) -> None:
    """track_yaml_loads records nested !include files."""
    inc = tmp_path / "included.yaml"
    inc.write_text("included_key: 42\n")
    main = tmp_path / "main.yaml"
    main.write_text("child: !include included.yaml\n")

    with yaml_util.track_yaml_loads() as loaded:
        result = yaml_util.load_yaml(main)
        # !include is deferred; resolve it to trigger the nested load
        result["child"].load()

    resolved = [p.name for p in loaded]
    assert "main.yaml" in resolved
    assert "included.yaml" in resolved


def test_track_yaml_loads_empty_outside_context(tmp_path: Path) -> None:
    """Files loaded outside the context are not recorded."""
    yaml_file = tmp_path / "test.yaml"
    yaml_file.write_text("key: value\n")

    with yaml_util.track_yaml_loads() as loaded:
        pass  # load nothing inside

    yaml_util.load_yaml(yaml_file)
    assert loaded == []


def test_track_yaml_loads_cleanup_on_exception(tmp_path: Path) -> None:
    """Listener is removed even if the body raises."""
    before = len(yaml_util._load_listeners)

    with pytest.raises(RuntimeError), yaml_util.track_yaml_loads():
        raise RuntimeError("boom")

    assert len(yaml_util._load_listeners) == before


def test_track_yaml_loads_no_duplicate_load_on_top_level_include_failure(
    tmp_path: Path,
) -> None:
    """A failed top-level !include must not record any file twice in track_yaml_loads."""
    main = tmp_path / "main.yaml"
    main.write_text("!include missing.yaml\n")

    with yaml_util.track_yaml_loads() as loaded, pytest.raises(EsphomeError):
        yaml_util.load_yaml(main)

    assert len(loaded) == len(set(loaded)), (
        f"Files loaded more than once during a failed top-level include: {loaded}"
    )


@pytest.mark.parametrize(
    "data",
    [
        {
            "key1": "value1",
            "key2": 42,
        },
        [1, 2, 3],
        "simple string",
    ],
)
def test_config_context_mixin(data) -> None:
    """Test that ConfigContext mixin correctly stores and retrieves context vars in a dict."""

    context_vars = {
        "var1": "context_value1",
        "var2": 100,
    }

    # Add context to the data
    tagged_data = yaml_util.add_context(data, context_vars)

    # Check that tagged_data has ConfigContext and correct vars
    assert isinstance(tagged_data, type(data))
    assert isinstance(tagged_data, yaml_util.ConfigContext)
    assert tagged_data.vars == context_vars

    # Check that original data is preserved
    assert tagged_data == data


def test_config_context_mixin_no_context() -> None:
    """Test that add_context does not tag data when no context vars are provided."""
    data = {"key": "value"}

    # Add context with None
    tagged_data = yaml_util.add_context(data, None)

    # Should return original data without tagging
    assert tagged_data is data
    assert not isinstance(tagged_data, yaml_util.ConfigContext)


def test_config_context_mixin_merge_contexts() -> None:
    """Test that add_context merges new context vars with existing ones."""
    data = {"key": "value"}

    initial_context = {
        "var1": "initial_value",
    }

    # First, add initial context
    tagged_data = yaml_util.add_context(data, initial_context)

    assert isinstance(tagged_data, yaml_util.ConfigContext)
    assert tagged_data.vars == initial_context

    # Now, add more context vars
    new_context = {
        "var2": "new_value",
        "var1": "overridden_value",  # This should override the initial var1
    }

    merged_tagged_data = yaml_util.add_context(tagged_data, new_context)

    # Check that merged_tagged_data has merged context vars
    expected_context = {
        "var1": "overridden_value",
        "var2": "new_value",
    }
    assert isinstance(merged_tagged_data, yaml_util.ConfigContext)
    assert merged_tagged_data.vars == expected_context

    # Check that original data is preserved
    assert merged_tagged_data == data


@pytest.mark.parametrize("data", [42, 3.14, True, None])
def test_config_context_non_taggable(data) -> None:
    """Test that add_context ignores non-string scalar values."""

    context_vars = {
        "var1": "context_value",
    }

    # Add context to the scalar data
    tagged_data = yaml_util.add_context(data, context_vars)

    # Check that tagged_data has ConfigContext and correct vars
    assert not isinstance(tagged_data, yaml_util.ConfigContext)

    # Check that original data is preserved
    assert tagged_data == data


def test_config_context_defaults_only() -> None:
    """Test that defaults: key is popped and used as context vars when no explicit vars given."""
    data = {"defaults": {"x": "1", "y": "2"}, "key": "value"}
    tagged = yaml_util.add_context(data, None)

    assert isinstance(tagged, yaml_util.ConfigContext)
    assert tagged.vars == {"x": "1", "y": "2"}
    assert "defaults" not in tagged


def test_config_context_defaults_explicit_vars_override() -> None:
    """Test that explicit vars take precedence over defaults: values."""
    data = {"defaults": {"x": "default_x", "z": "default_z"}, "key": "value"}
    tagged = yaml_util.add_context(data, {"x": "explicit_x", "w": "explicit_w"})

    assert isinstance(tagged, yaml_util.ConfigContext)
    assert tagged.vars == {"x": "explicit_x", "z": "default_z", "w": "explicit_w"}
    assert "defaults" not in tagged


def test_represent_extend() -> None:
    """Test that Extend objects are dumped as plain !extend scalars."""
    assert yaml_util.dump({"key": Extend("my_id")}) == "key: !extend 'my_id'\n"


def test_represent_remove() -> None:
    """Test that Remove objects are dumped as plain !remove scalars."""
    assert yaml_util.dump({"key": Remove("my_id")}) == "key: !remove 'my_id'\n"


def test_represent_include_file() -> None:
    """Test that IncludeFile objects are dumped as !include scalars."""
    include = yaml_util.IncludeFile(
        Path("/fake/main.yaml"), "path/to/file.yaml", None, lambda _: {}
    )
    assert yaml_util.dump({"key": include}) == "key: !include 'path/to/file.yaml'\n"


def test_represent_include_file_with_vars() -> None:
    """Test that IncludeFile with vars is dumped as !include mapping form."""
    include = yaml_util.IncludeFile(
        Path("/fake/main.yaml"),
        "path/to/file.yaml",
        {"key": "value"},
        lambda _: {},
    )
    result = yaml_util.dump({"key": include})
    assert "!include" in result
    assert "file: path/to/file.yaml" in result
    assert "key: value" in result


def test_represent_include_file_with_data_base_mixin() -> None:
    """Test that IncludeFile wrapped with ESPHomeDataBase mixin is also dumped correctly.

    The YAML loader wraps IncludeFile via add_class_to_obj, creating a dynamic
    subclass. add_multi_representer must match this subclass through the MRO.
    """
    include = yaml_util.IncludeFile(
        Path("/fake/main.yaml"), "common/spi.yaml", None, lambda _: {}
    )
    wrapped = yaml_util.make_data_base(include)
    assert isinstance(wrapped, yaml_util.ESPHomeDataBase)
    assert yaml_util.dump({"pkg": wrapped}) == "pkg: !include 'common/spi.yaml'\n"


# ── IncludeFile unit tests ──────────────────────────────────────────────────


def test_include_file_repr(tmp_path: Path) -> None:
    """repr() includes the filename so it appears usefully in error messages."""
    parent = tmp_path / "main.yaml"
    include = yaml_util.IncludeFile(parent, "some/nested.yaml", None, lambda _: {})
    assert repr(include) == "IncludeFile(some/nested.yaml)"


def test_include_file_load_caches_result(tmp_path: Path) -> None:
    """load() invokes the yaml_loader only once; subsequent calls return the cached object."""
    parent = tmp_path / "main.yaml"
    content = {"key": "value"}
    call_count = 0

    def counting_loader(_):
        nonlocal call_count
        call_count += 1
        return content

    include = yaml_util.IncludeFile(parent, "child.yaml", None, counting_loader)
    first = include.load()
    second = include.load()

    assert call_count == 1
    assert first is second


def test_include_file_load_caches_none_result(tmp_path: Path) -> None:
    """load() caches None content (empty YAML files) and does not re-invoke the loader."""
    parent = tmp_path / "main.yaml"
    call_count = 0

    def counting_loader(_):
        nonlocal call_count
        call_count += 1

    include = yaml_util.IncludeFile(parent, "empty.yaml", None, counting_loader)
    first = include.load()
    second = include.load()

    assert call_count == 1
    assert first is None
    assert second is None


def test_include_file_load_raises_on_unresolved_expressions(tmp_path: Path) -> None:
    """load() raises if the filename contains unresolved substitutions or expressions."""
    parent = tmp_path / "main.yaml"
    include = yaml_util.IncludeFile(parent, "${undefined_var}.yaml", None, lambda _: {})
    with pytest.raises(cv.Invalid, match="unresolved"):
        include.load()


@pytest.mark.parametrize(
    ("filename", "expected"),
    [
        ("device-${platform}.yaml", True),
        ("$platform.yaml", True),
        ("${a + b}.yaml", True),  # Jinja expression
        ("device.yaml", False),
        ("path/to/device.yaml", False),
        ("my$file.yaml", True),  # $file is a valid substitution
        ("price-100$.yaml", False),  # $ at end, not followed by valid substitution
    ],
)
def test_include_file_has_unresolved_expressions(
    tmp_path: Path, filename: str, expected: bool
) -> None:
    """has_unresolved_expressions() detects substitution patterns in the filename."""
    parent = tmp_path / "main.yaml"
    include = yaml_util.IncludeFile(parent, filename, None, lambda _: {})
    assert include.has_unresolved_expressions() == expected


def test_include_in_list_context() -> None:
    """!include of a file returning a list is handled correctly,
    including when that list itself contains a nested IncludeFile."""
    parent = Path("/fake/main.yaml")

    # The nested IncludeFile resolves to a plain string value
    inner = yaml_util.IncludeFile(parent, "inner.yaml", None, lambda _: "gamma")

    # The outer IncludeFile returns a list whose last element is itself an IncludeFile,
    # exercising the substitution pass's ability to recurse into loaded content.
    outer = yaml_util.IncludeFile(
        parent, "items.yaml", None, lambda _: ["alpha", "beta", inner]
    )

    config = OrderedDict({"values": outer})
    config = substitutions.do_substitution_pass(config)

    assert config["values"] == ["alpha", "beta", "gamma"]


def test_top_level_include_resolved_by_load_yaml(tmp_path: Path) -> None:
    """load_yaml resolves a top-level !include so callers always get a dict."""
    child = tmp_path / "child.yaml"
    child.write_text("key: value\n")
    main = tmp_path / "main.yaml"
    main.write_text("!include child.yaml\n")

    result = yaml_util.load_yaml(main)
    assert isinstance(result, dict)
    assert result["key"] == "value"


def test_include_plain_filename_loads_after_deferred_refactor() -> None:
    """!include with a plain filename (no $ expressions) still loads correctly.

    Regression guard: the deferred-loading refactor must not break the simple case.
    """
    parent = Path("/fake/main.yaml")
    include = yaml_util.IncludeFile(
        parent, "child.yaml", None, lambda _: {"answer": 42}
    )

    config = OrderedDict({"result": include})
    config = substitutions.do_substitution_pass(config)

    assert config["result"]["answer"] == 42


def test_yaml_merge_include_with_filename_substitution_raises() -> None:
    """<<: !include ${expr} raises a clear error — substitutions in merge-key filenames
    are not yet supported, and the error message must say so."""
    yaml_text = "base:\n  existing: value\n  <<: !include ${filename}.yaml\n"
    with pytest.raises(EsphomeError, match="not supported yet"):
        yaml_util.parse_yaml(
            Path("/fake/main.yaml"), io.StringIO(yaml_text), lambda _: {}
        )


def test_yaml_merge_list_include_with_filename_substitution_raises() -> None:
    """Substitutions in include filenames within merge-key lists raise a clear error."""
    yaml_text = "base:\n  existing: value\n  <<:\n    - !include ${filename}.yaml\n"
    with pytest.raises(EsphomeError, match="not supported yet"):
        yaml_util.parse_yaml(
            Path("/fake/main.yaml"), io.StringIO(yaml_text), lambda _: {}
        )


def test_yaml_merge_chain_include_resolves() -> None:
    """Chained includes in merge keys resolve through multiple IncludeFile layers."""
    parent = Path("/fake/main.yaml")

    inner = yaml_util.IncludeFile(parent, "inner.yaml", None, lambda _: {"x": 1})
    outer = yaml_util.IncludeFile(parent, "outer.yaml", None, lambda _: inner)

    yaml_text = "base:\n  existing: value\n  <<: !include outer.yaml\n"
    config = yaml_util.parse_yaml(parent, io.StringIO(yaml_text), lambda _: outer)
    config = substitutions.do_substitution_pass(config)

    assert config["base"]["x"] == 1
    assert config["base"]["existing"] == "value"


def test_yaml_merge_chain_include_depth_exceeded() -> None:
    """Chain includes in merge keys exceeding depth limit raise a clear error."""
    parent = Path("/fake/main.yaml")

    def self_referencing_loader(path: Path) -> yaml_util.IncludeFile:
        return yaml_util.IncludeFile(parent, path.name, None, self_referencing_loader)

    yaml_text = "base:\n  <<: !include loop.yaml\n"
    with pytest.raises(EsphomeError, match="Maximum include chain depth"):
        yaml_util.parse_yaml(parent, io.StringIO(yaml_text), self_referencing_loader)


def _located(value, doc: str, line: int, col: int):
    """Return *value* wrapped with a fake ESPHomeDataBase source location."""
    loc = DocumentLocation(doc, line, col)
    obj = make_data_base(value)
    if isinstance(obj, ESPHomeDataBase):
        obj._esp_range = DocumentRange(loc, loc)
    return obj


def test_format_path_no_location_info_returns_flat_path():
    """Plain path items with no esp_range produce a simple flat 'In:' line."""
    result = format_path(["wifi", "ssid"], None)
    assert result == "In: wifi->ssid"


def test_format_path_no_location_info_current_obj_adds_file():
    """When path has no location but current_obj does, its location is shown."""
    obj = _located("${var}", "main.yaml", 5, 10)
    result = format_path(["wifi", "ssid"], obj)
    assert result == "In: wifi->ssid in main.yaml 6:11"


def test_format_path_single_frame_no_include_boundary():
    """All located keys from the same document → single 'In:' line, no 'Included from'."""
    path = ["packages", _located("pkg1", "root.yaml", 5, 2)]
    result = format_path(path, None)
    assert result.startswith("In: packages->pkg1 in root.yaml 6:3")
    assert "Included from" not in result


def test_format_path_two_frames_shows_included_from():
    """Keys from two different documents produce 'In:' + one 'Included from' line."""
    path = [
        "packages",
        _located("device", "root.yaml", 10, 2),
        "packages",
        _located("inner", "hardware.yaml", 3, 2),
    ]
    result = format_path(path, None)
    assert "In: packages->inner in hardware.yaml 4:3" in result
    assert "Included from packages->device in root.yaml 11:3" in result


def test_format_path_three_frames_full_include_stack():
    """Three document levels produce two 'Included from' lines in correct order."""
    path = [
        "packages",
        _located("device", "root.yaml", 10, 2),
        "packages",
        _located("_wifi_", "hardware.yaml", 43, 2),
        "packages",
        _located("_roam_", "wifi.yaml", 25, 2),
    ]
    result = format_path(path, None)
    lines = result.splitlines()
    assert lines[0].startswith("In: packages->_roam_ in wifi.yaml")
    assert lines[1].startswith("  Included from packages->_wifi_ in hardware.yaml")
    assert lines[2].startswith("  Included from packages->device in root.yaml")


def test_format_path_current_obj_overrides_innermost_location():
    """current_obj's esp_range replaces the key's column for the 'In:' line."""
    path = ["packages", _located("pkg1", "root.yaml", 5, 2)]
    # Value (the expression) sits at column 10, not column 2 like the key
    value = _located("${undefined}", "root.yaml", 5, 10)
    result = format_path(path, value)
    assert "6:11" in result
    assert "6:3" not in result


def test_format_path_empty_path_with_no_location():
    """Empty path with no location info returns 'In: '."""
    result = format_path([], None)
    assert result == "In: "


def test_format_path_integer_path_items_formatted_as_subscript():
    """Integer indices are rendered as [n] subscripts in the flat fallback."""
    result = format_path(["packages", 0], None)
    assert result == "In: packages[0]"


def test_format_path_integer_list_index_attached_to_previous_frame():
    """A list index between two include boundaries attaches to the outer frame."""
    path = [
        "packages",
        _located("packages", "main.yaml", 5, 0),
        0,
        _located("packages", "level1.yaml", 2, 0),
        0,
        _located("esphome", "level2.yaml", 0, 0),
        _located("name", "level2.yaml", 1, 8),
    ]
    result = format_path(path, None)
    lines = result.splitlines()
    assert lines[0].startswith("In: esphome->name in level2.yaml")
    assert "packages[0]" in lines[1] and "level1.yaml" in lines[1]
    assert "packages[0]" in lines[2] and "main.yaml" in lines[2]


def test_format_path_trailing_unlocated_string_after_located_key():
    """Plain string keys after the last located key must still appear in output."""
    path = [_located("packages", "main.yaml", 5, 0), "sub", "key"]
    result = format_path(path, None)
    assert result == "In: packages->sub->key in main.yaml 6:1"


def test_format_path_trailing_unlocated_int_attaches_to_current_frame():
    """Trailing ints attach to the open frame's last key (subscript), strings
    buffer until end-of-path and then flush behind."""
    path = [_located("packages", "main.yaml", 5, 0), 0, "sub"]
    result = format_path(path, None)
    # Int attaches to 'packages' as [0] subscript; trailing 'sub' is flushed
    # at end and appears after.
    assert result == "In: packages[0]->sub in main.yaml 6:1"


def test_format_path_only_trailing_unlocated_strings_are_preserved():
    """Trailing pending items must not be silently dropped after the last frame."""
    path = [
        _located("packages", "main.yaml", 5, 0),
        _located("inner", "hardware.yaml", 3, 0),
        "tail1",
        "tail2",
    ]
    result = format_path(path, None)
    lines = result.splitlines()
    assert lines[0] == "In: inner->tail1->tail2 in hardware.yaml 4:1"
    assert lines[1] == "  Included from packages in main.yaml 6:1"


def test_format_path_leading_int_with_no_current_doc_goes_to_pending():
    """An int before any located key is buffered and shown in the first frame."""
    path = [0, _located("name", "main.yaml", 1, 0)]
    result = format_path(path, None)
    # Leading ints have no preceding name to subscript onto, so they render
    # as bare [n] in the formatted segment.
    assert result == "In: [0]->name in main.yaml 2:1"


def test_format_path_only_unlocated_int_returns_flat_fallback():
    """Path with only an int and no location info renders via the flat fallback."""
    result = format_path([0], None)
    assert result == "In: [0]"


def test_format_path_current_obj_in_different_doc_than_innermost_frame():
    """current_obj's location is preferred even when its document differs from the frame's."""
    path = [_located("packages", "root.yaml", 1, 0)]
    value = _located("${var}", "other.yaml", 9, 4)
    result = format_path(path, value)
    # Innermost line uses current_obj's mark (other.yaml 10:5), not the key's.
    assert result == "In: packages in other.yaml 10:5"


def test_format_path_current_obj_without_location_falls_back_to_key():
    """An ESPHomeDataBase current_obj with no esp_range falls back to the key's location."""

    class _NoRange(ESPHomeDataBase, str):
        pass

    obj = _NoRange.__new__(_NoRange, "value")
    str.__init__(obj)
    # No _esp_range set on this instance.
    assert obj.esp_range is None

    path = [_located("packages", "main.yaml", 5, 2)]
    result = format_path(path, obj)
    assert result == "In: packages in main.yaml 6:3"


def test_format_path_empty_path_with_located_current_obj():
    """An empty path with a located current_obj still surfaces the location."""
    obj = _located("${var}", "main.yaml", 0, 0)
    result = format_path([], obj)
    assert result == "In:  in main.yaml 1:1"


def test_make_literal_wraps_dict() -> None:
    """A dict is wrapped so it becomes an ESPLiteralValue instance."""
    value = {"key": "${var}"}
    result = make_literal(value)
    assert isinstance(result, ESPLiteralValue)
    assert isinstance(result, dict)
    assert result == {"key": "${var}"}


def test_make_literal_wraps_list() -> None:
    """A list is wrapped so it becomes an ESPLiteralValue instance."""
    value = ["${var}", "plain"]
    result = make_literal(value)
    assert isinstance(result, ESPLiteralValue)
    assert isinstance(result, list)
    assert result == ["${var}", "plain"]


def test_make_literal_wraps_string() -> None:
    """A string is wrapped so it becomes an ESPLiteralValue instance."""
    result = make_literal("${var}")
    assert isinstance(result, ESPLiteralValue)
    assert result == "${var}"


def test_make_literal_returns_already_wrapped_value_unchanged() -> None:
    """Wrapping a value that is already an ESPLiteralValue returns it as-is."""
    value = make_literal({"key": "value"})
    assert isinstance(value, ESPLiteralValue)
    result = make_literal(value)
    assert result is value


def test_make_literal_returns_none_unchanged() -> None:
    """Values whose class cannot be augmented (e.g. ``None``) are returned as-is."""
    result = make_literal(None)
    assert result is None


def test_make_literal_blocks_substitution() -> None:
    """A value wrapped with make_literal is skipped by the substitution pass."""
    value = make_literal({"pin": "${PIN}"})
    result = substitutions.substitute(
        value,
        path=[],
        parent_context=substitutions.ContextVars(),
        strict_undefined=False,
    )
    # The literal block must remain untouched, even though the variable is
    # undefined in the context.
    assert result == {"pin": "${PIN}"}
    assert isinstance(result, ESPLiteralValue)
