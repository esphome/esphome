"""Tests for the packages component."""

import logging
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.components.packages import (
    CONFIG_SCHEMA,
    _walk_packages,
    do_packages_pass,
    is_package_definition,
    merge_packages,
)
from esphome.components.substitutions import do_substitution_pass
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
from esphome.yaml_util import IncludeFile, add_context

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

    def failing_callback(package_config: dict, context: object) -> dict:
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

    def failing_callback(package_config: dict, context: object) -> dict:
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

    def fail_on_last(package_config: dict, context: object) -> dict:
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

    def fail_then_succeed(package_config: dict, context: object) -> dict:
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
