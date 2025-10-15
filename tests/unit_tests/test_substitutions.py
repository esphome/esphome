import glob
import logging
from pathlib import Path

from esphome import yaml_util
from esphome.components import substitutions
from esphome.const import CONF_PACKAGES, CONF_SUBSTITUTIONS
from esphome.util import OrderedDict

_LOGGER = logging.getLogger(__name__)

# Set to True for dev mode behavior
# This will generate the expected version of the test files.

DEV_MODE = False


def sort_dicts(obj):
    """Recursively sort dictionaries for order-insensitive comparison."""
    if isinstance(obj, dict):
        return {k: sort_dicts(obj[k]) for k in sorted(obj)}
    if isinstance(obj, list):
        # Lists are not sorted; we preserve order
        return [sort_dicts(i) for i in obj]
    return obj


def dict_diff(a, b, path=""):
    """Recursively find differences between two dict/list structures."""
    diffs = []
    if isinstance(a, dict) and isinstance(b, dict):
        a_keys = set(a)
        b_keys = set(b)
        diffs.extend(f"{path}/{key} only in actual" for key in a_keys - b_keys)
        diffs.extend(f"{path}/{key} only in expected" for key in b_keys - a_keys)
        for key in a_keys & b_keys:
            diffs.extend(dict_diff(a[key], b[key], f"{path}/{key}"))
    elif isinstance(a, list) and isinstance(b, list):
        min_len = min(len(a), len(b))
        for i in range(min_len):
            diffs.extend(dict_diff(a[i], b[i], f"{path}[{i}]"))
        if len(a) > len(b):
            diffs.extend(
                f"{path}[{i}] only in actual: {a[i]!r}" for i in range(min_len, len(a))
            )
        elif len(b) > len(a):
            diffs.extend(
                f"{path}[{i}] only in expected: {b[i]!r}"
                for i in range(min_len, len(b))
            )
    elif a != b:
        diffs.append(f"\t{path}: actual={a!r} expected={b!r}")
    return diffs


def write_yaml(path: Path, data: dict) -> None:
    path.write_text(yaml_util.dump(data), encoding="utf-8")


def test_substitutions_fixtures(fixture_path):
    base_dir = fixture_path / "substitutions"
    sources = sorted(glob.glob(str(base_dir / "*.input.yaml")))
    assert sources, f"No input YAML files found in {base_dir}"

    failures = []
    for source_path in sources:
        source_path = Path(source_path)
        try:
            expected_path = source_path.with_suffix("").with_suffix(".approved.yaml")
            test_case = source_path.with_suffix("").stem

            # Load using ESPHome's YAML loader
            config = yaml_util.load_yaml(source_path)

            if CONF_PACKAGES in config:
                from esphome.components.packages import do_packages_pass

                config = do_packages_pass(config)

            substitutions.do_substitution_pass(config, None)

            # Also load expected using ESPHome's loader, or use {} if missing and DEV_MODE
            if expected_path.is_file():
                expected = yaml_util.load_yaml(expected_path)
            elif DEV_MODE:
                expected = {}
            else:
                assert expected_path.is_file(), (
                    f"Expected file missing: {expected_path}"
                )

            # Sort dicts only (not lists) for comparison
            got_sorted = sort_dicts(config)
            expected_sorted = sort_dicts(expected)

            if got_sorted != expected_sorted:
                diff = "\n".join(dict_diff(got_sorted, expected_sorted))
                msg = (
                    f"Substitution result mismatch for {source_path.name}\n"
                    f"Diff:\n{diff}\n\n"
                    f"Got:      {got_sorted}\n"
                    f"Expected: {expected_sorted}"
                )
                # Write out the received file when test fails
                if DEV_MODE:
                    received_path = source_path.with_name(f"{test_case}.received.yaml")
                    write_yaml(received_path, config)
                    print(msg)
                    failures.append(msg)
                else:
                    raise AssertionError(msg)
        except Exception as err:
            _LOGGER.error("Error in test file %s", source_path)
            raise err

    if DEV_MODE and failures:
        print(f"\n{len(failures)} substitution test case(s) failed.")

    if DEV_MODE:
        _LOGGER.error("Tests passed, but Dev mode is enabled.")
    assert not DEV_MODE  # make sure DEV_MODE is disabled after you are finished.


def test_substitutions_with_command_line_maintains_ordered_dict() -> None:
    """Test that substitutions remain an OrderedDict when command line substitutions are provided.

    This is a regression test for https://github.com/esphome/esphome/issues/11182
    where the config would become a regular dict and fail when move_to_end() was called.
    """
    # Create an OrderedDict config with substitutions
    config = OrderedDict()
    config["esphome"] = {"name": "test"}
    config[CONF_SUBSTITUTIONS] = {"var1": "value1", "var2": "value2"}
    config["other_key"] = "other_value"

    # Command line substitutions that should override
    command_line_subs = {"var2": "override", "var3": "new_value"}

    # Call do_substitution_pass with command line substitutions
    substitutions.do_substitution_pass(config, command_line_subs)

    # Verify that config is still an OrderedDict
    assert isinstance(config, OrderedDict), "Config should remain an OrderedDict"

    # Verify substitutions are at the beginning (move_to_end with last=False)
    keys = list(config.keys())
    assert keys[0] == CONF_SUBSTITUTIONS, "Substitutions should be first key"

    # Verify substitutions were properly merged
    assert config[CONF_SUBSTITUTIONS]["var1"] == "value1"
    assert config[CONF_SUBSTITUTIONS]["var2"] == "override"
    assert config[CONF_SUBSTITUTIONS]["var3"] == "new_value"

    # Verify config[CONF_SUBSTITUTIONS] is also an OrderedDict
    assert isinstance(config[CONF_SUBSTITUTIONS], OrderedDict), (
        "Substitutions should be an OrderedDict"
    )


def test_substitutions_without_command_line_maintains_ordered_dict() -> None:
    """Test that substitutions work correctly without command line substitutions."""
    config = OrderedDict()
    config["esphome"] = {"name": "test"}
    config[CONF_SUBSTITUTIONS] = {"var1": "value1"}
    config["other_key"] = "other_value"

    # Call without command line substitutions
    substitutions.do_substitution_pass(config, None)

    # Verify that config is still an OrderedDict
    assert isinstance(config, OrderedDict), "Config should remain an OrderedDict"

    # Verify substitutions are at the beginning
    keys = list(config.keys())
    assert keys[0] == CONF_SUBSTITUTIONS, "Substitutions should be first key"


def test_substitutions_after_merge_config_maintains_ordered_dict() -> None:
    """Test that substitutions work after merge_config (packages scenario).

    This is a regression test for https://github.com/esphome/esphome/issues/11182
    where using packages would cause config to become a regular dict, breaking move_to_end().
    """
    from esphome.config_helpers import merge_config

    # Simulate what happens with packages - merge two OrderedDict configs
    base_config = OrderedDict()
    base_config["esphome"] = {"name": "base"}
    base_config[CONF_SUBSTITUTIONS] = {"var1": "value1"}

    package_config = OrderedDict()
    package_config["sensor"] = [{"platform": "template"}]
    package_config[CONF_SUBSTITUTIONS] = {"var2": "value2"}

    # Merge configs (simulating package merge)
    merged_config = merge_config(base_config, package_config)

    # Verify merged config is still an OrderedDict
    assert isinstance(merged_config, OrderedDict), (
        "Merged config should be an OrderedDict"
    )

    # Now try to run substitution pass on the merged config
    substitutions.do_substitution_pass(merged_config, None)

    # Should not raise AttributeError
    assert isinstance(merged_config, OrderedDict), (
        "Config should still be OrderedDict after substitution pass"
    )
    keys = list(merged_config.keys())
    assert keys[0] == CONF_SUBSTITUTIONS, "Substitutions should be first key"
