import glob
import logging
import os

from esphome import yaml_util
from esphome.components import substitutions
from esphome.const import CONF_PACKAGES

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


def write_yaml(path, data):
    with open(path, "w", encoding="utf-8") as f:
        f.write(yaml_util.dump(data))


def test_substitutions_fixtures(fixture_path):
    base_dir = fixture_path / "substitutions"
    sources = sorted(glob.glob(str(base_dir / "*.input.yaml")))
    assert sources, f"No input YAML files found in {base_dir}"

    failures = []
    for source_path in sources:
        try:
            expected_path = source_path.replace(".input.yaml", ".approved.yaml")
            test_case = os.path.splitext(os.path.basename(source_path))[0].replace(
                ".input", ""
            )

            # Load using ESPHome's YAML loader
            config = yaml_util.load_yaml(source_path)

            if CONF_PACKAGES in config:
                from esphome.components.packages import do_packages_pass

                config = do_packages_pass(config)

            substitutions.do_substitution_pass(config, None)

            # Also load expected using ESPHome's loader, or use {} if missing and DEV_MODE
            if os.path.isfile(expected_path):
                expected = yaml_util.load_yaml(expected_path)
            elif DEV_MODE:
                expected = {}
            else:
                assert os.path.isfile(expected_path), (
                    f"Expected file missing: {expected_path}"
                )

            # Sort dicts only (not lists) for comparison
            got_sorted = sort_dicts(config)
            expected_sorted = sort_dicts(expected)

            if got_sorted != expected_sorted:
                diff = "\n".join(dict_diff(got_sorted, expected_sorted))
                msg = (
                    f"Substitution result mismatch for {os.path.basename(source_path)}\n"
                    f"Diff:\n{diff}\n\n"
                    f"Got:      {got_sorted}\n"
                    f"Expected: {expected_sorted}"
                )
                # Write out the received file when test fails
                if DEV_MODE:
                    received_path = os.path.join(
                        os.path.dirname(source_path), f"{test_case}.received.yaml"
                    )
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
