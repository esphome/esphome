"""Tests for the packages component."""

import logging
from pathlib import Path
import re
from unittest.mock import MagicMock, patch

import pytest

from esphome.components.packages import (
    CONFIG_SCHEMA,
    _substitute_package_definition,
    _walk_packages,
    do_packages_pass,
    is_package_definition,
    merge_packages,
    resolve_packages,
)
from esphome.components.substitutions import ContextVars, do_substitution_pass
import esphome.config as config_module
from esphome.config import resolve_extend_remove
from esphome.config_helpers import Extend, Remove
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEFAULTS,
    CONF_DOMAIN,
    CONF_ESPHOME,
    CONF_FILES,
    CONF_FILTERS,
    CONF_ID,
    CONF_MULTIPLY,
    CONF_NAME,
    CONF_OFFSET,
    CONF_PACKAGES,
    CONF_PASSWORD,
    CONF_PATH,
    CONF_PLATFORM,
    CONF_REF,
    CONF_REFRESH,
    CONF_SENSOR,
    CONF_SSID,
    CONF_SUBSTITUTIONS,
    CONF_UPDATE_INTERVAL,
    CONF_URL,
    CONF_VARS,
    CONF_WIFI,
)
from esphome.core import CORE
from esphome.util import OrderedDict
from esphome.yaml_util import DocumentPath, IncludeFile, add_context, load_yaml

# Test strings
TEST_DEVICE_NAME = "test_device_name"
TEST_PLATFORM = "test_platform"
TEST_WIFI_SSID = "test_wifi_ssid"
TEST_PACKAGE_WIFI_SSID = "test_package_wifi_ssid"
TEST_PACKAGE_WIFI_PASSWORD = "test_package_wifi_password"
TEST_DOMAIN = "test_domain_name"
TEST_SENSOR_PLATFORM_1 = "test_sensor_platform_1"
TEST_SENSOR_PLATFORM_2 = "test_sensor_platform_2"
TEST_SENSOR_NAME_1 = "test_sensor_name_1"
TEST_SENSOR_NAME_2 = "test_sensor_name_2"
TEST_SENSOR_NAME_3 = "test_sensor_name_3"
TEST_SENSOR_ID_1 = "test_sensor_id_1"
TEST_SENSOR_ID_2 = "test_sensor_id_2"
TEST_SENSOR_UPDATE_INTERVAL = "test_sensor_update_interval"
TEST_YAML_FILENAME = "sensor1.yaml"


@pytest.fixture(name="basic_wifi")
def fixture_basic_wifi():
    return {
        CONF_SSID: TEST_PACKAGE_WIFI_SSID,
        CONF_PASSWORD: TEST_PACKAGE_WIFI_PASSWORD,
    }


@pytest.fixture(name="basic_esphome")
def fixture_basic_esphome():
    return {CONF_NAME: TEST_DEVICE_NAME, CONF_PLATFORM: TEST_PLATFORM}


def packages_pass(config):
    """Passes the config through the packages processing steps."""
    config = do_packages_pass(config)
    config = do_substitution_pass(config)
    config = merge_packages(config)
    resolve_extend_remove(config)
    return config


_INCLUDE_FILE = "INCLUDE_FILE"


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        # IncludeFile objects are package definitions
        (_INCLUDE_FILE, True),
        # Git URL shorthand strings are package definitions
        ("github://esphome/firmware/base.yaml@main", True),
        # Remote package dicts (with url key) are package definitions
        ({"url": "https://github.com/esphome/firmware", "file": "base.yaml"}, True),
        # Plain config dicts are NOT package definitions (they are config fragments)
        ({"wifi": {"ssid": "test"}}, False),
        # None is not a package definition
        (None, False),
        # Lists are not package definitions
        ([{"wifi": {"ssid": "test"}}], False),
        # Empty dicts are not package definitions
        ({}, False),
    ],
    ids=[
        "include_file",
        "git_shorthand",
        "remote_package",
        "config_fragment",
        "none",
        "list",
        "empty_dict",
    ],
)
def test_is_package_definition(value: object, expected: bool) -> None:
    """Test that is_package_definition correctly identifies package definitions."""
    if value is _INCLUDE_FILE:
        value = MagicMock(spec=IncludeFile)
    assert is_package_definition(value) is expected


def test_package_unused(basic_esphome, basic_wifi) -> None:
    """
    Ensures do_package_pass does not change a config if packages aren't used.
    """
    config = {CONF_ESPHOME: basic_esphome, CONF_WIFI: basic_wifi}

    actual = packages_pass(config)
    assert actual == config


def test_package_invalid_dict(basic_esphome, basic_wifi) -> None:
    """
    If a url: key is present, it's expected to be well-formed remote package spec. Ensure an error is raised if not.
    Any other simple dict passed as a package will be merged as usual but may fail later validation.

    """
    config = {CONF_ESPHOME: basic_esphome, CONF_PACKAGES: basic_wifi | {CONF_URL: ""}}

    with pytest.raises(cv.Invalid):
        packages_pass(config)


@pytest.mark.parametrize(
    "packages",
    [
        {"package1": "github://esphome/non-existant-repo/file1.yml@main"},
        {"package2": "github://esphome/non-existant-repo/file1.yml"},
        {"package3": "github://esphome/non-existant-repo/other-folder/file1.yml"},
        [
            "github://esphome/non-existant-repo/file1.yml@main",
            "github://esphome/non-existant-repo/file1.yml",
            "github://esphome/non-existant-repo/other-folder/file1.yml",
        ],
    ],
)
def test_package_shorthand(packages) -> None:
    CONFIG_SCHEMA(packages)


@pytest.mark.parametrize(
    "packages",
    [
        # not github
        {"package1": "someplace://esphome/non-existant-repo/file1.yml@main"},
        # missing repo
        {"package2": "github://esphome/file1.yml"},
        # missing file
        {"package3": "github://esphome/non-existant-repo/@main"},
        {"a": "invalid string, not shorthand"},
        "some string",
        3,
        False,
        {"a": 8},
        ["someplace://esphome/non-existant-repo/file1.yml@main"],
        ["github://esphome/file1.yml"],
        ["github://esphome/non-existant-repo/@main"],
        ["some string"],
        [True],
        [3],
    ],
)
def test_package_invalid(packages) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(packages)


def test_package_include(basic_wifi, basic_esphome) -> None:
    """
    Tests the simple case where an independent config present in a package is added to the top-level config as is.

    In this test, the CONF_WIFI config is expected to be simply added to the top-level config.
    """
    config = {
        CONF_ESPHOME: basic_esphome,
        CONF_PACKAGES: {"network": {CONF_WIFI: basic_wifi}},
    }

    expected = {CONF_ESPHOME: basic_esphome, CONF_WIFI: basic_wifi}

    actual = packages_pass(config)
    assert actual == expected


def test_single_package(
    basic_esphome,
    basic_wifi,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """
    Tests the simple case where a single package is added to the top-level config as is.
    In this test, the CONF_WIFI config is expected to be simply added to the top-level config.
    This tests the case where the user just put packages: !include package.yaml, not
    part of a list or mapping of packages.
    This behavior is deprecated, the test also checks if a warning is issued.
    """
    config = {CONF_ESPHOME: basic_esphome, CONF_PACKAGES: {CONF_WIFI: basic_wifi}}

    expected = {CONF_ESPHOME: basic_esphome, CONF_WIFI: basic_wifi}

    with caplog.at_level("WARNING"):
        actual = packages_pass(config)

    assert actual == expected

    assert "This method for including packages will go away in 2026.7.0" in caplog.text


def test_package_append(basic_wifi, basic_esphome) -> None:
    """
    Tests the case where a key is present in both a package and top-level config.

    In this test, CONF_WIFI is defined in a package, and CONF_DOMAIN is added to it at the top level.
    """
    config = {
        CONF_ESPHOME: basic_esphome,
        CONF_PACKAGES: {"network": {CONF_WIFI: basic_wifi}},
        CONF_WIFI: {CONF_DOMAIN: TEST_DOMAIN},
    }

    expected = {
        CONF_ESPHOME: basic_esphome,
        CONF_WIFI: {
            CONF_SSID: TEST_PACKAGE_WIFI_SSID,
            CONF_PASSWORD: TEST_PACKAGE_WIFI_PASSWORD,
            CONF_DOMAIN: TEST_DOMAIN,
        },
    }

    actual = packages_pass(config)
    assert actual == expected


def test_package_override(basic_wifi, basic_esphome) -> None:
    """
    Ensures that the top-level configuration takes precedence over duplicate keys defined in a package.

    In this test, CONF_SSID should be overwritten by that defined in the top-level config.
    """
    config = {
        CONF_ESPHOME: basic_esphome,
        CONF_PACKAGES: {"network": {CONF_WIFI: basic_wifi}},
        CONF_WIFI: {CONF_SSID: TEST_WIFI_SSID},
    }

    expected = {
        CONF_ESPHOME: basic_esphome,
        CONF_WIFI: {
            CONF_SSID: TEST_WIFI_SSID,
            CONF_PASSWORD: TEST_PACKAGE_WIFI_PASSWORD,
        },
    }

    actual = packages_pass(config)
    assert actual == expected


def test_multiple_package_order() -> None:
    """
    Ensures that mutiple packages are merged in order.
    """
    config = {
        CONF_PACKAGES: {
            "package1": {
                "logger": {
                    "level": "DEBUG",
                },
            },
            "package2": {
                "logger": {
                    "level": "VERBOSE",
                },
            },
        },
    }

    expected = {
        "logger": {
            "level": "VERBOSE",
        },
    }

    actual = packages_pass(config)
    assert actual == expected


def test_package_list_merge() -> None:
    """
    Ensures lists defined in both a package and the top-level config are merged correctly
    """
    config = {
        CONF_PACKAGES: {
            "package_sensors": {
                CONF_SENSOR: [
                    {
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_1,
                    },
                    {
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_2,
                    },
                ]
            }
        },
        CONF_SENSOR: [
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_2,
                CONF_NAME: TEST_SENSOR_NAME_1,
            },
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_2,
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
        ],
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_1,
            },
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_2,
                CONF_NAME: TEST_SENSOR_NAME_1,
            },
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_2,
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
        ]
    }

    actual = packages_pass(config)
    assert actual == expected


def test_package_list_merge_by_id() -> None:
    """
    Ensures that components with matching IDs are merged correctly.

    In this test, a sensor is defined in a package, and a CONF_UPDATE_INTERVAL is added at the top level,
    and a sensor name is overridden in another sensor.
    """
    config = {
        CONF_PACKAGES: {
            "package_sensors": {
                CONF_SENSOR: [
                    {
                        CONF_ID: TEST_SENSOR_ID_1,
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_1,
                    },
                    {
                        CONF_ID: TEST_SENSOR_ID_2,
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_2,
                    },
                ]
            },
            "package2": {
                CONF_SENSOR: [
                    {
                        CONF_ID: Extend(TEST_SENSOR_ID_1),
                        CONF_DOMAIN: "2",
                    }
                ],
            },
            "package3": {
                CONF_SENSOR: [
                    {
                        CONF_ID: Extend(TEST_SENSOR_ID_1),
                        CONF_DOMAIN: "3",
                    }
                ],
            },
        },
        CONF_SENSOR: [
            {
                CONF_ID: Extend(TEST_SENSOR_ID_1),
                CONF_UPDATE_INTERVAL: TEST_SENSOR_UPDATE_INTERVAL,
            },
            {CONF_ID: Extend(TEST_SENSOR_ID_2), CONF_NAME: TEST_SENSOR_NAME_1},
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_2,
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
        ],
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_1,
                CONF_UPDATE_INTERVAL: TEST_SENSOR_UPDATE_INTERVAL,
                CONF_DOMAIN: "3",
            },
            {
                CONF_ID: TEST_SENSOR_ID_2,
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_1,
            },
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_2,
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
        ]
    }

    actual = packages_pass(config)
    assert actual == expected


def test_package_merge_by_id_with_list() -> None:
    """
    Ensures that components with matching IDs are merged correctly when their configuration contains lists.

    For example, a sensor with filters defined in both a package and the top level config should be merged.
    """

    config = {
        CONF_PACKAGES: {
            "sensors": {
                CONF_SENSOR: [
                    {
                        CONF_ID: TEST_SENSOR_ID_1,
                        CONF_FILTERS: [{CONF_MULTIPLY: 42.0}],
                    }
                ]
            }
        },
        CONF_SENSOR: [
            {
                CONF_ID: Extend(TEST_SENSOR_ID_1),
                CONF_FILTERS: [{CONF_OFFSET: 146.0}],
            }
        ],
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
                CONF_FILTERS: [{CONF_MULTIPLY: 42.0}, {CONF_OFFSET: 146.0}],
            }
        ]
    }

    actual = packages_pass(config)
    assert actual == expected


def test_package_merge_by_missing_id() -> None:
    """
    Ensures that a validation error is thrown when trying to extend a missing ID.
    """

    config = {
        CONF_PACKAGES: {
            "sensors": {
                CONF_SENSOR: [
                    {
                        CONF_ID: TEST_SENSOR_ID_1,
                        CONF_FILTERS: [{CONF_MULTIPLY: 42.0}],
                    },
                ]
            }
        },
        CONF_SENSOR: [
            {CONF_ID: TEST_SENSOR_ID_1, CONF_FILTERS: [{CONF_MULTIPLY: 10.0}]},
            {
                CONF_ID: Extend(TEST_SENSOR_ID_2),
                CONF_FILTERS: [{CONF_OFFSET: 146.0}],
            },
        ],
    }

    error_raised = False
    try:
        packages_pass(config)
        assert False, "Expected validation error for missing ID"
    except cv.Invalid as err:
        error_raised = True
        assert err.path == [CONF_SENSOR, 2]

    assert error_raised


def test_package_list_remove_by_id() -> None:
    """
    Ensures that components with matching IDs are removed correctly.

    In this test, two sensors are defined in a package, and one of them is removed at the top level.
    """
    config = {
        CONF_PACKAGES: {
            "package_sensors": {
                CONF_SENSOR: [
                    {
                        CONF_ID: TEST_SENSOR_ID_1,
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_1,
                    },
                    {
                        CONF_ID: TEST_SENSOR_ID_2,
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_2,
                    },
                ]
            },
            # "package2": {
            #     CONF_SENSOR: [
            #         {
            #             CONF_ID: Remove(TEST_SENSOR_ID_1),
            #         }
            #     ],
            # },
        },
        CONF_SENSOR: [
            {
                CONF_ID: Remove(TEST_SENSOR_ID_1),
            },
        ],
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_2,
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
        ]
    }

    actual = packages_pass(config)
    assert actual == expected


def test_multiple_package_list_remove_by_id() -> None:
    """
    Ensures that components with matching IDs are removed correctly.

    In this test, two sensors are defined in a package, and one of them is removed in another package.
    """
    config = {
        CONF_PACKAGES: {
            "package_sensors": {
                CONF_SENSOR: [
                    {
                        CONF_ID: TEST_SENSOR_ID_1,
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_1,
                    },
                    {
                        CONF_ID: TEST_SENSOR_ID_2,
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_2,
                    },
                ]
            },
            "package2": {
                CONF_SENSOR: [
                    {
                        CONF_ID: Remove(TEST_SENSOR_ID_1),
                    }
                ],
            },
        },
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_2,
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
        ]
    }

    actual = packages_pass(config)
    assert actual == expected


def test_package_dict_remove_by_id(basic_wifi, basic_esphome) -> None:
    """
    Ensures that components with missing IDs are removed from dict.
    Ensures that the top-level configuration takes precedence over duplicate keys defined in a package.

    In this test, CONF_SSID should be overwritten by that defined in the top-level config.
    """
    config = {
        CONF_ESPHOME: basic_esphome,
        CONF_PACKAGES: {"network": {CONF_WIFI: basic_wifi}},
        CONF_WIFI: Remove(),
    }

    expected = {
        CONF_ESPHOME: basic_esphome,
    }

    actual = packages_pass(config)
    assert actual == expected


def test_package_remove_by_missing_id() -> None:
    """
    Ensures that components with missing IDs are not merged.
    """

    config = {
        CONF_PACKAGES: {
            "sensors": {
                CONF_SENSOR: [
                    {
                        CONF_ID: TEST_SENSOR_ID_1,
                        CONF_FILTERS: [{CONF_MULTIPLY: 42.0}],
                    },
                ]
            }
        },
        "missing_key": Remove(),
        CONF_SENSOR: [
            {CONF_ID: TEST_SENSOR_ID_1, CONF_FILTERS: [{CONF_MULTIPLY: 10.0}]},
            {
                CONF_ID: Remove(TEST_SENSOR_ID_2),
                CONF_FILTERS: [{CONF_OFFSET: 146.0}],
            },
        ],
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
                CONF_FILTERS: [{CONF_MULTIPLY: 42.0}],
            },
            {
                CONF_ID: TEST_SENSOR_ID_1,
                CONF_FILTERS: [{CONF_MULTIPLY: 10.0}],
            },
        ],
    }

    actual = packages_pass(config)
    assert actual == expected


@patch("esphome.yaml_util.load_yaml")
@patch("pathlib.Path.is_file")
@patch("esphome.git.clone_or_update")
def test_remote_packages_with_files_list(
    mock_clone_or_update, mock_is_file, mock_load_yaml
) -> None:
    """
    Ensures that packages are loaded as mixed list of dictionary and strings
    """
    # Mock the response from git.clone_or_update
    mock_revert = MagicMock()
    mock_clone_or_update.return_value = (Path("/tmp/noexists"), mock_revert)

    # Mock the response from pathlib.Path.is_file
    mock_is_file.return_value = True

    # Mock the response from esphome.yaml_util.load_yaml
    mock_load_yaml.side_effect = [
        OrderedDict(
            {
                CONF_SENSOR: [
                    {
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_1,
                    }
                ]
            }
        ),
        OrderedDict(
            {
                CONF_SENSOR: [
                    {
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_2,
                    }
                ]
            }
        ),
    ]

    # Define the input config
    config = {
        CONF_PACKAGES: {
            "package1": {
                CONF_URL: "https://github.com/esphome/non-existant-repo",
                CONF_REF: "main",
                CONF_FILES: [
                    {CONF_PATH: TEST_YAML_FILENAME},
                    "sensor2.yaml",
                ],
                CONF_REFRESH: "1d",
            }
        }
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_1,
            },
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
        ]
    }

    actual = packages_pass(config)
    assert actual == expected


@patch("esphome.yaml_util.load_yaml")
@patch("pathlib.Path.is_file")
@patch("esphome.git.clone_or_update")
def test_remote_packages_with_files_list_and_substitutions(
    mock_clone_or_update, mock_is_file, mock_load_yaml
) -> None:
    """
    Ensures that packages are loaded as mixed list of dictionary and strings
    """
    # Mock the response from git.clone_or_update
    mock_revert = MagicMock()
    mock_clone_or_update.return_value = (Path("/tmp/noexists"), mock_revert)

    # Mock the response from pathlib.Path.is_file
    mock_is_file.return_value = True

    # Mock the response from esphome.yaml_util.load_yaml
    mock_load_yaml.side_effect = [
        OrderedDict(
            {
                CONF_SENSOR: [
                    {
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_1,
                    }
                ]
            }
        ),
        OrderedDict(
            {
                CONF_SENSOR: [
                    {
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_2,
                    }
                ]
            }
        ),
    ]

    # Define the input config
    config = {
        CONF_PACKAGES: {
            "package1": add_context(
                {
                    CONF_URL: r"${url}",
                    CONF_REF: r"${branch}",
                    CONF_FILES: [
                        {CONF_PATH: r"$file"},
                        "sensor2.yaml",
                    ],
                    CONF_REFRESH: "1d",
                },
                {
                    "branch": "main",
                    "file": TEST_YAML_FILENAME,
                    "url": "https://github.com/esphome/non-existant-repo",
                },
            )
        }
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_1,
            },
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
        ]
    }

    actual = packages_pass(config)
    assert actual == expected


@patch("esphome.yaml_util.load_yaml")
@patch("pathlib.Path.is_file")
@patch("esphome.git.clone_or_update")
def test_remote_packages_with_files_and_vars(
    mock_clone_or_update, mock_is_file, mock_load_yaml
) -> None:
    """
    Ensures that packages are loaded as mixed list of dictionary and strings with vars
    """
    # Mock the response from git.clone_or_update
    mock_revert = MagicMock()
    mock_clone_or_update.return_value = (Path("/tmp/noexists"), mock_revert)

    # Mock the response from pathlib.Path.is_file
    mock_is_file.return_value = True

    # Mock the response from esphome.yaml_util.load_yaml
    mock_load_yaml.side_effect = [
        OrderedDict(
            {
                CONF_DEFAULTS: {CONF_NAME: TEST_SENSOR_NAME_1},
                CONF_SENSOR: [
                    {
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: "${name}",
                    }
                ],
            }
        ),
        OrderedDict(
            {
                CONF_DEFAULTS: {CONF_NAME: TEST_SENSOR_NAME_1},
                CONF_SENSOR: [
                    {
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: "${name}",
                    }
                ],
            }
        ),
        OrderedDict(
            {
                CONF_DEFAULTS: {CONF_NAME: TEST_SENSOR_NAME_1},
                CONF_SENSOR: [
                    {
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: "${name}",
                    }
                ],
            }
        ),
    ]

    # Define the input config
    config = {
        CONF_PACKAGES: {
            "package1": {
                CONF_URL: "https://github.com/esphome/non-existant-repo",
                CONF_REF: "main",
                CONF_FILES: [
                    {
                        CONF_PATH: TEST_YAML_FILENAME,
                        CONF_VARS: {CONF_NAME: TEST_SENSOR_NAME_2},
                    },
                    {
                        CONF_PATH: TEST_YAML_FILENAME,
                        CONF_VARS: {CONF_NAME: TEST_SENSOR_NAME_3},
                    },
                    {CONF_PATH: TEST_YAML_FILENAME},
                ],
                CONF_REFRESH: "1d",
            }
        }
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_3,
            },
            {
                CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                CONF_NAME: TEST_SENSOR_NAME_1,
            },
        ]
    }

    actual = packages_pass(config)
    assert actual == expected


def test_packages_merge_substitutions() -> None:
    """
    Tests that substitutions from packages in a complex package hierarchy
    are extracted and merged into the top-level config.
    """
    config = {
        CONF_SUBSTITUTIONS: {
            "a": 1,
            "b": 2,
            "c": 3,
        },
        CONF_PACKAGES: {
            "package1": {
                "logger": {
                    "level": "DEBUG",
                },
                CONF_PACKAGES: [
                    {
                        CONF_SUBSTITUTIONS: {
                            "a": 10,
                            "e": 5,
                        },
                        "sensor": [
                            {"platform": "template", "id": "sensor1"},
                        ],
                    },
                ],
                "sensor": [
                    {"platform": "template", "id": "sensor2"},
                ],
            },
            "package2": {
                "logger": {
                    "level": "VERBOSE",
                },
            },
            "package3": {
                CONF_PACKAGES: [
                    {
                        CONF_PACKAGES: [
                            {
                                CONF_SUBSTITUTIONS: {
                                    "b": 20,
                                    "d": 4,
                                },
                                "sensor": [
                                    {"platform": "template", "id": "sensor3"},
                                ],
                            },
                        ],
                        CONF_SUBSTITUTIONS: {
                            "b": 20,
                            "d": 6,
                        },
                        "sensor": [
                            {"platform": "template", "id": "sensor4"},
                        ],
                    },
                ],
            },
        },
    }

    expected = {
        CONF_SUBSTITUTIONS: {"a": 1, "e": 5, "b": 2, "d": 6, "c": 3},
        CONF_PACKAGES: {
            "package1": {
                "logger": {
                    "level": "DEBUG",
                },
                CONF_PACKAGES: [
                    {
                        "sensor": [
                            {"platform": "template", "id": "sensor1"},
                        ],
                    },
                ],
                "sensor": [
                    {"platform": "template", "id": "sensor2"},
                ],
            },
            "package2": {
                "logger": {
                    "level": "VERBOSE",
                },
            },
            "package3": {
                CONF_PACKAGES: [
                    {
                        CONF_PACKAGES: [
                            {
                                "sensor": [
                                    {"platform": "template", "id": "sensor3"},
                                ],
                            },
                        ],
                        "sensor": [
                            {"platform": "template", "id": "sensor4"},
                        ],
                    },
                ],
            },
        },
    }

    actual = do_packages_pass(config, command_line_substitutions={})
    assert actual == expected


def test_package_merge() -> None:
    """
    Tests that all packages are merged into the top-level config.
    """
    config = {
        CONF_SUBSTITUTIONS: {"a": 1, "e": 5, "b": 2, "d": 6, "c": 3},
        CONF_PACKAGES: {
            "package1": {
                "logger": {
                    "level": "DEBUG",
                },
                CONF_PACKAGES: [
                    {
                        "sensor": [
                            {"platform": "template", "id": "sensor1"},
                        ],
                    },
                ],
                "sensor": [
                    {"platform": "template", "id": "sensor2"},
                ],
            },
            "package2": {
                "logger": {
                    "level": "VERBOSE",
                },
            },
            "package3": {
                CONF_PACKAGES: [
                    {
                        CONF_PACKAGES: [
                            {
                                "sensor": [
                                    {"platform": "template", "id": "sensor3"},
                                ],
                            },
                        ],
                        "sensor": [
                            {"platform": "template", "id": "sensor4"},
                        ],
                    },
                ],
            },
        },
    }
    expected = {
        "sensor": [
            {"platform": "template", "id": "sensor1"},
            {"platform": "template", "id": "sensor2"},
            {"platform": "template", "id": "sensor3"},
            {"platform": "template", "id": "sensor4"},
        ],
        "logger": {"level": "VERBOSE"},
        CONF_SUBSTITUTIONS: {"a": 1, "e": 5, "b": 2, "d": 6, "c": 3},
    }
    actual = merge_packages(config)

    assert actual == expected


def test_packages_invalid_type_raises() -> None:
    """Packages that are not a dict or list raise cv.Invalid."""
    config = {
        CONF_PACKAGES: "not_a_dict_or_list",
    }
    with pytest.raises(
        cv.Invalid, match="Packages must be a key to value mapping or list"
    ):
        do_packages_pass(config)


@patch("esphome.components.packages.resolve_include")
def test_packages_include_file_resolves_to_list(mock_resolve_include) -> None:
    """When packages: is an IncludeFile that resolves to a list, it is processed correctly."""
    include_file = MagicMock(spec=IncludeFile)
    package_content = {CONF_WIFI: {CONF_SSID: TEST_PACKAGE_WIFI_SSID}}
    mock_resolve_include.return_value = [package_content]

    config = {CONF_PACKAGES: include_file}
    result = do_packages_pass(config)
    result = merge_packages(result)

    assert result == {CONF_WIFI: {CONF_SSID: TEST_PACKAGE_WIFI_SSID}}


@patch("esphome.components.packages.resolve_include")
def test_packages_include_file_resolves_to_dict(mock_resolve_include) -> None:
    """When packages: is an IncludeFile that resolves to a dict, it is processed correctly."""
    include_file = MagicMock(spec=IncludeFile)
    package_content = {CONF_WIFI: {CONF_SSID: TEST_PACKAGE_WIFI_SSID}}
    mock_resolve_include.return_value = {"network": package_content}

    config = {CONF_PACKAGES: include_file}
    result = do_packages_pass(config)
    result = merge_packages(result)

    assert result == {CONF_WIFI: {CONF_SSID: TEST_PACKAGE_WIFI_SSID}}


@patch("esphome.components.packages.resolve_include")
def test_packages_include_file_resolves_to_invalid_type_raises(
    mock_resolve_include,
) -> None:
    """When packages: is an IncludeFile that resolves to an invalid type, cv.Invalid is raised."""
    include_file = MagicMock(spec=IncludeFile)
    mock_resolve_include.return_value = "not_a_dict_or_list"

    config = {CONF_PACKAGES: include_file}
    with pytest.raises(
        cv.Invalid, match="Packages must be a key to value mapping or list"
    ) as exc_info:
        do_packages_pass(config)

    assert exc_info.value.path == [CONF_PACKAGES]


@pytest.mark.parametrize(
    "invalid_package",
    [
        6,
        "some string",
        True,
    ],
)
def test_invalid_package_contents_rejected(invalid_package: object) -> None:
    """Invalid package contents are rejected by PACKAGE_SCHEMA during do_packages_pass."""
    config = {
        CONF_PACKAGES: {
            "some_package": invalid_package,
        },
    }
    with pytest.raises(cv.Invalid):
        do_packages_pass(config)


@pytest.mark.xfail(
    reason="Deprecated single-package fallback swallows these errors. "
    "Remove xfail when single-package deprecation is removed (2026.7.0).",
    strict=True,
)
@pytest.mark.parametrize(
    "invalid_package",
    [
        None,
        ["some string"],
        {"some_component": 8},
        {3: 2},
    ],
)
def test_invalid_package_contents_masked_by_deprecation(
    invalid_package: object,
) -> None:
    """These invalid packages are swallowed by the deprecated single-package fallback."""
    config = {
        CONF_PACKAGES: {
            "some_package": invalid_package,
        },
    }
    with pytest.raises(cv.Invalid):
        do_packages_pass(config)


def test_named_dict_with_include_files_no_false_deprecation_warning(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Package errors in named dicts must not trigger the deprecated fallback."""
    good_include = MagicMock(spec=IncludeFile)
    bad_include = MagicMock(spec=IncludeFile)

    config = {
        CONF_PACKAGES: {
            "good_pkg": good_include,
            "bad_pkg": bad_include,
        },
    }

    call_count = 0

    def failing_callback(
        package_config: dict, context: object, path: DocumentPath | None = None
    ) -> dict:
        nonlocal call_count
        call_count += 1
        if call_count == 1:
            # First package processes fine
            return {CONF_WIFI: {CONF_SSID: "test"}}
        # Second package has an error (e.g. jinja syntax error)
        raise cv.Invalid("simulated jinja error in bad_pkg")

    with (
        caplog.at_level(logging.WARNING),
        pytest.raises(cv.Invalid, match="simulated jinja error"),
    ):
        _walk_packages(config, failing_callback)

    # Must NOT emit the deprecated single-package warning
    assert "deprecated" not in caplog.text.lower()


def test_validate_deprecated_false_raises_directly(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """With validate_deprecated=False, errors raise directly without fallback.

    This is the codepath used for remote packages where _process_remote_package
    returns already-resolved dicts that is_package_definition cannot detect.
    """
    config = {
        CONF_PACKAGES: {
            "pkg_a": {CONF_WIFI: {CONF_SSID: "test"}},
            "pkg_b": {CONF_WIFI: {CONF_SSID: "test2"}},
        },
    }

    call_count = 0

    def failing_callback(
        package_config: dict, context: object, path: DocumentPath | None = None
    ) -> dict:
        nonlocal call_count
        call_count += 1
        if call_count == 1:
            return package_config
        raise cv.Invalid("nested error")

    with (
        caplog.at_level(logging.WARNING),
        pytest.raises(cv.Invalid, match="nested error"),
    ):
        _walk_packages(config, failing_callback, validate_deprecated=False)

    assert "deprecated" not in caplog.text.lower()


def test_error_on_first_declared_package_still_detected() -> None:
    """When the first declared package errors, it's the last processed in reverse.

    All other entries are already resolved to dicts, but the failing entry
    retains its original IncludeFile value since assignment was skipped.
    """
    config = {
        CONF_PACKAGES: {
            "first_pkg": MagicMock(spec=IncludeFile),
            "second_pkg": MagicMock(spec=IncludeFile),
            "third_pkg": MagicMock(spec=IncludeFile),
        },
    }

    call_count = 0

    def fail_on_last(
        package_config: dict, context: object, path: DocumentPath | None = None
    ) -> dict:
        nonlocal call_count
        call_count += 1
        # Reverse iteration: third_pkg (1), second_pkg (2), first_pkg (3)
        if call_count < 3:
            return {CONF_WIFI: {CONF_SSID: "test"}}
        raise cv.Invalid("error in first_pkg")

    with pytest.raises(cv.Invalid, match="error in first_pkg"):
        _walk_packages(config, fail_on_last)


def test_deprecated_single_package_fallback_still_works(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The deprecated single-package form still falls back at the top level.

    When a dict's values are plain config fragments (not package definitions)
    and the callback fails, the deprecated fallback wraps the dict in a list
    and retries with a deprecation warning.
    """
    config = {
        CONF_PACKAGES: {
            CONF_WIFI: {CONF_SSID: "test", CONF_PASSWORD: "secret"},
        },
    }

    attempt = 0

    def fail_then_succeed(
        package_config: dict, context: object, path: DocumentPath | None = None
    ) -> dict:
        nonlocal attempt
        attempt += 1
        if attempt == 1:
            # First attempt: treating as named dict fails
            raise cv.Invalid("not a valid package")
        # Second attempt: after fallback wraps as list, succeeds
        return package_config

    with caplog.at_level(logging.WARNING):
        _walk_packages(config, fail_then_succeed)

    assert "deprecated" in caplog.text.lower()


def test_merge_packages_invalid_nested_type_raises() -> None:
    """Invalid nested packages type during merge raises cv.Invalid."""
    config = {
        CONF_PACKAGES: {
            "pkg": {
                CONF_PACKAGES: "invalid",
            },
        },
    }
    with pytest.raises(
        cv.Invalid, match="Packages must be a key to value mapping or list"
    ):
        merge_packages(config)


@patch("esphome.yaml_util.load_yaml")
@patch("pathlib.Path.is_file")
@patch("esphome.git.clone_or_update")
def test_remote_packages_no_revert(
    mock_clone_or_update, mock_is_file, mock_load_yaml
) -> None:
    """Remote packages with revert=None load without retry logic."""
    mock_clone_or_update.return_value = (Path("/tmp/noexists"), None)
    mock_is_file.return_value = True
    mock_load_yaml.return_value = OrderedDict(
        {CONF_SENSOR: [{CONF_PLATFORM: TEST_SENSOR_PLATFORM_1, CONF_NAME: "test"}]}
    )

    config = {
        CONF_PACKAGES: {
            "pkg": {
                CONF_URL: "https://github.com/esphome/repo",
                CONF_REF: "main",
                CONF_FILES: [{CONF_PATH: "file.yaml"}],
                CONF_REFRESH: "1d",
            }
        }
    }
    actual = packages_pass(config)
    assert actual[CONF_SENSOR] == [
        {CONF_PLATFORM: TEST_SENSOR_PLATFORM_1, CONF_NAME: "test"}
    ]


def test_raw_config_contains_merged_esphome_from_package(tmp_path) -> None:
    """Test that CORE.raw_config contains esphome section from merged package.

    This is a regression test for the bug where CORE.raw_config was set before
    packages were merged, causing KeyError when components accessed
    CORE.raw_config[CONF_ESPHOME] and the esphome section came from a package.
    """
    # Create a config where esphome section comes from a package
    test_config = OrderedDict()
    test_config[CONF_PACKAGES] = {
        "base": {
            CONF_ESPHOME: {CONF_NAME: TEST_DEVICE_NAME},
        }
    }
    test_config["esp32"] = {"board": "esp32dev"}

    # Set up CORE for the test
    test_yaml = tmp_path / "test.yaml"
    test_yaml.write_text("# test config")
    CORE.reset()
    CORE.config_path = test_yaml

    # Call validate_config - this should merge packages and set CORE.raw_config
    config_module.validate_config(test_config, {})

    # Verify that CORE.raw_config contains the esphome section from the package
    assert CONF_ESPHOME in CORE.raw_config, (
        "CORE.raw_config should contain esphome section after package merge"
    )
    assert CORE.raw_config[CONF_ESPHOME][CONF_NAME] == TEST_DEVICE_NAME


# ---------------------------------------------------------------------------
# _substitute_package_definition
# ---------------------------------------------------------------------------


def test_substitute_package_definition_local_dict_returned_unchanged() -> None:
    """A plain local config dict is not substituted and is returned as-is."""
    pkg = {CONF_WIFI: {CONF_SSID: "test"}}
    result = _substitute_package_definition(pkg, ContextVars())
    assert result is pkg


def test_substitute_package_definition_string_resolved_with_context() -> None:
    """A string package definition has its variables substituted."""
    ctx = ContextVars({"variant": "esp32"})
    result = _substitute_package_definition("device-${variant}.yaml", ctx)
    assert result == "device-esp32.yaml"


def test_substitute_package_definition_undefined_in_string() -> None:
    """An undefined variable in a package URL string raises cv.Invalid."""
    with pytest.raises(cv.Invalid, match="Undefined variable in package definition"):
        _substitute_package_definition(
            "github://org/repo/${undefined_var}/pkg.yaml", ContextVars()
        )


def test_substitute_package_definition_undefined_in_remote_dict_field() -> None:
    """An undefined variable inside a remote-dict field names the offending field."""
    with pytest.raises(cv.Invalid) as exc_info:
        _substitute_package_definition(
            {CONF_URL: "github://${typo}/repo"}, ContextVars()
        )
    err = str(exc_info.value)
    assert "'typo' is undefined" in err
    assert CONF_URL in err


def test_substitute_package_definition_undefined_in_remote_dict_non_first_field() -> (
    None
):
    """The field path joins correctly for non-first dict fields (e.g. ``ref``)."""
    with pytest.raises(cv.Invalid) as exc_info:
        _substitute_package_definition(
            {
                CONF_URL: "github://org/repo",
                CONF_REF: "branch-${branch_typo}",
            },
            ContextVars(),
        )
    err = str(exc_info.value)
    assert "'branch_typo' is undefined" in err
    assert CONF_REF in err


def test_substitute_package_definition_includes_source_location(tmp_path: Path) -> None:
    """A package loaded from YAML surfaces file/line/col in the cv.Invalid message.

    Line/column are rendered 1-based (matching config.line_info() and editor
    line numbering) and point at the offending scalar, not the enclosing dict.
    """
    yaml_file = tmp_path / "main.yaml"
    yaml_file.write_text(
        "packages:\n  broken: github://org/repo/${undefined_var}/pkg.yaml\n"
    )
    config = load_yaml(yaml_file)
    package_config = config[CONF_PACKAGES]["broken"]

    with pytest.raises(cv.Invalid) as exc_info:
        _substitute_package_definition(package_config, ContextVars())

    err = str(exc_info.value)
    assert "main.yaml" in err
    # The offending value lives on line 2 (1-based). Column depends on the YAML
    # loader, so we only pin line and check that a 1-based column is present.
    match = re.search(r"main\.yaml (\d+):(\d+)", err)
    assert match, err
    line, col = int(match.group(1)), int(match.group(2))
    assert line == 2, f"expected 1-based line 2, got {line} (err={err!r})"
    assert col >= 1, f"expected 1-based column ≥ 1, got {col} (err={err!r})"


def test_substitute_package_definition_vars_preserved_literally() -> None:
    """``vars:`` blocks in remote-package files are not substituted prematurely.

    Variable references inside ``vars:`` may resolve to substitutions
    contributed by sibling packages that have not yet been loaded, so they
    must be passed through untouched and resolved later by the package YAML.
    """
    pkg = {
        CONF_URL: "https://github.com/esphome/non-existant-repo",
        CONF_REF: "main",
        CONF_FILES: [
            {
                CONF_PATH: "common/somefile.yaml",
                CONF_VARS: {"pin": "${PIN}"},
            },
        ],
    }
    # Note: PIN is intentionally NOT in the context — it is meant to
    # be resolved later, when the package YAML is processed.
    result = _substitute_package_definition(pkg, ContextVars())

    assert result[CONF_FILES][0][CONF_VARS] == {"pin": "${PIN}"}


def test_substitute_package_definition_other_fields_still_substituted() -> None:
    """Marking ``vars:`` literal does not stop substitution of url/ref/path."""
    ctx = ContextVars({"branch": "release", "org": "esphome"})
    pkg = {
        CONF_URL: "https://github.com/${org}/firmware",
        CONF_REF: "${branch}",
        CONF_FILES: [
            {
                CONF_PATH: "common/sensor.yaml",
                CONF_VARS: {"pin": "${PIN}"},
            },
        ],
    }
    result = _substitute_package_definition(pkg, ctx)

    assert result[CONF_URL] == "https://github.com/esphome/firmware"
    assert result[CONF_REF] == "release"
    # vars passed through unchanged
    assert result[CONF_FILES][0][CONF_VARS] == {"pin": "${PIN}"}


def test_substitute_package_definition_without_vars_unaffected() -> None:
    """Files entries without a ``vars:`` block continue to work."""
    ctx = ContextVars({"branch": "main"})
    pkg = {
        CONF_URL: "https://github.com/esphome/firmware",
        CONF_REF: "${branch}",
        CONF_FILES: [
            {CONF_PATH: "file1.yaml"},
            "file2.yaml",
        ],
    }
    result = _substitute_package_definition(pkg, ctx)

    assert result[CONF_REF] == "main"
    assert result[CONF_FILES][0] == {CONF_PATH: "file1.yaml"}
    assert result[CONF_FILES][1] == "file2.yaml"


@patch("esphome.yaml_util.load_yaml")
@patch("pathlib.Path.is_file")
@patch("esphome.git.clone_or_update")
def test_remote_package_vars_resolved_against_sibling_package_substitutions(
    mock_clone_or_update, mock_is_file, mock_load_yaml
) -> None:
    """A ``vars:`` reference in one remote package can resolve to a
    substitution defined in a sibling remote package.

    A higher-priority package declares ``substitutions:`` (e.g. ``SENSOR_PIN: 5``) and a
    lower-priority package's ``files: -> vars:`` references that substitution.
    Because packages are processed highest-priority first and ``vars:`` is now
    preserved literally during package-definition processing, the substitution
    is resolved correctly when the package YAML itself is loaded.
    """
    mock_clone_or_update.return_value = (Path("/tmp/noexists"), MagicMock())
    mock_is_file.return_value = True

    # Two YAML files mocked from the "remote" repo:
    #   - platform.yaml exports a substitution ``SENSOR_PIN``
    #   - sensor.yaml uses ``${pin}`` (which is bound from ``vars:`` to
    #     ``${SENSOR_PIN}`` and resolved against the merged substitutions).
    mock_load_yaml.side_effect = [
        # Order matches reverse-priority traversal (highest priority first).
        OrderedDict(
            {
                CONF_SUBSTITUTIONS: {"SENSOR_PIN": "GPIO5"},
            }
        ),
        OrderedDict(
            {
                CONF_SENSOR: [
                    {
                        CONF_PLATFORM: TEST_SENSOR_PLATFORM_1,
                        CONF_NAME: TEST_SENSOR_NAME_1,
                        "pin": "${pin}",
                    }
                ],
            }
        ),
    ]

    config = {
        CONF_PACKAGES: {
            "special_sensor": {
                CONF_URL: "https://github.com/esphome/non-existant-repo",
                CONF_FILES: [
                    {
                        CONF_PATH: "sensor.yaml",
                        CONF_VARS: {"pin": "${SENSOR_PIN}"},
                    },
                ],
                CONF_REFRESH: "1d",
            },
            "platform": {
                CONF_URL: "https://github.com/esphome/non-existant-repo",
                CONF_FILES: ["platform.yaml"],
                CONF_REFRESH: "1d",
            },
        }
    }

    actual = packages_pass(config)

    assert actual[CONF_SENSOR][0]["pin"] == "GPIO5"


# ---------------------------------------------------------------------------
# resolve_packages — single-call wrapper around do_packages_pass + merge_packages
# ---------------------------------------------------------------------------


def test_resolve_packages_returns_config_unchanged_without_packages() -> None:
    """No ``packages:`` key → no-op, same dict back."""
    config = {CONF_ESPHOME: {CONF_NAME: "test"}, CONF_WIFI: {CONF_SSID: "x"}}
    result = resolve_packages(config)
    assert result is config
    assert CONF_PACKAGES not in result


def test_resolve_packages_loads_and_merges_in_one_call() -> None:
    """End-to-end: a config with one local-dict package gets its blocks flattened."""
    config = {
        CONF_ESPHOME: {CONF_NAME: "main"},
        CONF_PACKAGES: {
            "shared": {
                CONF_WIFI: {CONF_SSID: "from_package"},
                CONF_SENSOR: [
                    {CONF_PLATFORM: "template", CONF_NAME: "from_package_sensor"},
                ],
            }
        },
    }
    result = resolve_packages(config)
    # ``packages:`` is gone — it was consumed by the merge.
    assert CONF_PACKAGES not in result
    # Blocks contributed by the package are now top-level.
    assert result[CONF_WIFI][CONF_SSID] == "from_package"
    assert result[CONF_SENSOR][0][CONF_NAME] == "from_package_sensor"
    # The main config's own keys survive untouched.
    assert result[CONF_ESPHOME][CONF_NAME] == "main"


def test_resolve_packages_preserves_main_config_overrides() -> None:
    """Main-config values win over package values for the same key.

    Pinning the precedence ESPHome's compiler uses so any future
    refactor of the wrapper doesn't accidentally flip the order.
    """
    config = {
        CONF_ESPHOME: {CONF_NAME: "main"},
        CONF_WIFI: {CONF_SSID: "main_wins"},
        CONF_PACKAGES: {
            "shared": {CONF_WIFI: {CONF_SSID: "package_loses"}},
        },
    }
    result = resolve_packages(config)
    assert result[CONF_WIFI][CONF_SSID] == "main_wins"


def test_resolve_packages_forwards_command_line_substitutions() -> None:
    """``command_line_substitutions`` reaches the underlying ``do_packages_pass``.

    The wrapper exists so external tools have one stable seam; if
    that seam silently dropped a kwarg the underlying call accepts,
    callers would see surprising behaviour. This pins the
    pass-through.
    """
    config = {
        CONF_ESPHOME: {CONF_NAME: "main"},
        CONF_PACKAGES: {"shared": {CONF_WIFI: {CONF_SSID: "from_package"}}},
    }
    with patch(
        "esphome.components.packages.do_packages_pass",
        wraps=do_packages_pass,
    ) as spy:
        resolve_packages(config, command_line_substitutions={"foo": "bar"})
    spy.assert_called_once()
    _, kwargs = spy.call_args
    assert kwargs.get("command_line_substitutions") == {"foo": "bar"}


def test_resolve_packages_does_not_run_substitutions() -> None:
    """``${var}`` placeholders inside package content stay literal.

    The full ``validate_config`` pipeline runs ``do_substitution_pass``
    BETWEEN ``do_packages_pass`` and ``merge_packages``; this wrapper
    skips it on purpose. Pin that contract so a future refactor can't
    silently start resolving substitutions and break callers that
    deliberately compose the passes themselves.
    """
    config = {
        CONF_ESPHOME: {CONF_NAME: "main"},
        CONF_SUBSTITUTIONS: {"ssid_value": "resolved_ssid"},
        CONF_PACKAGES: {
            "shared": {CONF_WIFI: {CONF_SSID: "${ssid_value}"}},
        },
    }
    result = resolve_packages(config)
    # Without ``do_substitution_pass`` the placeholder is preserved.
    assert result[CONF_WIFI][CONF_SSID] == "${ssid_value}"


def test_resolve_packages_does_not_apply_extend_remove() -> None:
    """Top-level ``!remove`` / ``!extend`` markers stay in the merged dict.

    The full ``validate_config`` pipeline runs ``resolve_extend_remove``
    AFTER ``merge_packages``; this wrapper skips it on purpose. Pin
    that contract: a package-contributed block paired with a top-level
    ``!remove`` is left as-is for callers to handle (or for them to
    call ``resolve_extend_remove`` themselves).
    """
    config = {
        CONF_ESPHOME: {CONF_NAME: "main"},
        CONF_WIFI: Remove(),
        CONF_PACKAGES: {
            "shared": {CONF_WIFI: {CONF_SSID: "from_package"}},
        },
    }
    result = resolve_packages(config)
    # ``merge_packages`` keeps the top-level ``!remove`` (it wins
    # over the package value during merge), and the marker is not
    # resolved by this wrapper.
    assert isinstance(result[CONF_WIFI], Remove)
