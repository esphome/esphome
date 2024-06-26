"""Tests for the packages component."""

import pytest


from esphome.const import (
    CONF_DOMAIN,
    CONF_ESPHOME,
    CONF_FILTERS,
    CONF_ID,
    CONF_MULTIPLY,
    CONF_NAME,
    CONF_OFFSET,
    CONF_PACKAGES,
    CONF_PASSWORD,
    CONF_PLATFORM,
    CONF_SENSOR,
    CONF_SSID,
    CONF_UPDATE_INTERVAL,
    CONF_WIFI,
)
from esphome.components.packages import do_packages_pass
from esphome.config_helpers import Extend, Remove
import esphome.config_validation as cv

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
TEST_SENSOR_ID_1 = "test_sensor_id_1"
TEST_SENSOR_ID_2 = "test_sensor_id_2"
TEST_SENSOR_UPDATE_INTERVAL = "test_sensor_update_interval"


@pytest.fixture(name="basic_wifi")
def fixture_basic_wifi():
    return {
        CONF_SSID: TEST_PACKAGE_WIFI_SSID,
        CONF_PASSWORD: TEST_PACKAGE_WIFI_PASSWORD,
    }


@pytest.fixture(name="basic_esphome")
def fixture_basic_esphome():
    return {CONF_NAME: TEST_DEVICE_NAME, CONF_PLATFORM: TEST_PLATFORM}


def test_package_unused(basic_esphome, basic_wifi):
    """
    Ensures do_package_pass does not change a config if packages aren't used.
    """
    config = {CONF_ESPHOME: basic_esphome, CONF_WIFI: basic_wifi}

    actual = do_packages_pass(config)
    assert actual == config


def test_package_invalid_dict(basic_esphome, basic_wifi):
    """
    Ensures an error is raised if packages is not valid.

    """
    config = {CONF_ESPHOME: basic_esphome, CONF_PACKAGES: basic_wifi}

    with pytest.raises(cv.Invalid):
        do_packages_pass(config)


def test_package_include(basic_wifi, basic_esphome):
    """
    Tests the simple case where an independent config present in a package is added to the top-level config as is.

    In this test, the CONF_WIFI config is expected to be simply added to the top-level config.
    """
    config = {
        CONF_ESPHOME: basic_esphome,
        CONF_PACKAGES: {"network": {CONF_WIFI: basic_wifi}},
    }

    expected = {CONF_ESPHOME: basic_esphome, CONF_WIFI: basic_wifi}

    actual = do_packages_pass(config)
    assert actual == expected


def test_package_append(basic_wifi, basic_esphome):
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

    actual = do_packages_pass(config)
    assert actual == expected


def test_package_override(basic_wifi, basic_esphome):
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

    actual = do_packages_pass(config)
    assert actual == expected


def test_multiple_package_order():
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

    actual = do_packages_pass(config)
    assert actual == expected


def test_package_list_merge():
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
            {CONF_PLATFORM: TEST_SENSOR_PLATFORM_2, CONF_NAME: TEST_SENSOR_NAME_1},
            {CONF_PLATFORM: TEST_SENSOR_PLATFORM_2, CONF_NAME: TEST_SENSOR_NAME_2},
        ],
    }

    expected = {
        CONF_SENSOR: [
            {CONF_PLATFORM: TEST_SENSOR_PLATFORM_1, CONF_NAME: TEST_SENSOR_NAME_1},
            {CONF_PLATFORM: TEST_SENSOR_PLATFORM_1, CONF_NAME: TEST_SENSOR_NAME_2},
            {CONF_PLATFORM: TEST_SENSOR_PLATFORM_2, CONF_NAME: TEST_SENSOR_NAME_1},
            {CONF_PLATFORM: TEST_SENSOR_PLATFORM_2, CONF_NAME: TEST_SENSOR_NAME_2},
        ]
    }

    actual = do_packages_pass(config)
    assert actual == expected


def test_package_list_merge_by_id():
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
            {CONF_PLATFORM: TEST_SENSOR_PLATFORM_2, CONF_NAME: TEST_SENSOR_NAME_2},
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
            {CONF_PLATFORM: TEST_SENSOR_PLATFORM_2, CONF_NAME: TEST_SENSOR_NAME_2},
        ]
    }

    actual = do_packages_pass(config)
    assert actual == expected


def test_package_merge_by_id_with_list():
    """
    Ensures that components with matching IDs are merged correctly when their configuration contains lists.

    For example, a sensor with filters defined in both a package and the top level config should be merged.
    """

    config = {
        CONF_PACKAGES: {
            "sensors": {
                CONF_SENSOR: [
                    {CONF_ID: TEST_SENSOR_ID_1, CONF_FILTERS: [{CONF_MULTIPLY: 42.0}]}
                ]
            }
        },
        CONF_SENSOR: [
            {CONF_ID: Extend(TEST_SENSOR_ID_1), CONF_FILTERS: [{CONF_OFFSET: 146.0}]}
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

    actual = do_packages_pass(config)
    assert actual == expected


def test_package_merge_by_missing_id():
    """
    Ensures that components with missing IDs are not merged.
    """

    config = {
        CONF_PACKAGES: {
            "sensors": {
                CONF_SENSOR: [
                    {CONF_ID: TEST_SENSOR_ID_1, CONF_FILTERS: [{CONF_MULTIPLY: 42.0}]},
                ]
            }
        },
        CONF_SENSOR: [
            {CONF_ID: TEST_SENSOR_ID_1, CONF_FILTERS: [{CONF_MULTIPLY: 10.0}]},
            {CONF_ID: Extend(TEST_SENSOR_ID_2), CONF_FILTERS: [{CONF_OFFSET: 146.0}]},
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
            {
                CONF_ID: Extend(TEST_SENSOR_ID_2),
                CONF_FILTERS: [{CONF_OFFSET: 146.0}],
            },
        ]
    }

    actual = do_packages_pass(config)
    assert actual == expected


def test_package_list_remove_by_id():
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

    actual = do_packages_pass(config)
    assert actual == expected


def test_multiple_package_list_remove_by_id():
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

    actual = do_packages_pass(config)
    assert actual == expected


def test_package_dict_remove_by_id(basic_wifi, basic_esphome):
    """
    Ensures that components with missing IDs are removed from dict.
    """
    """
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

    actual = do_packages_pass(config)
    assert actual == expected


def test_package_remove_by_missing_id():
    """
    Ensures that components with missing IDs are not merged.
    """

    config = {
        CONF_PACKAGES: {
            "sensors": {
                CONF_SENSOR: [
                    {CONF_ID: TEST_SENSOR_ID_1, CONF_FILTERS: [{CONF_MULTIPLY: 42.0}]},
                ]
            }
        },
        "missing_key": Remove(),
        CONF_SENSOR: [
            {CONF_ID: TEST_SENSOR_ID_1, CONF_FILTERS: [{CONF_MULTIPLY: 10.0}]},
            {CONF_ID: Remove(TEST_SENSOR_ID_2), CONF_FILTERS: [{CONF_OFFSET: 146.0}]},
        ],
    }

    expected = {
        "missing_key": Remove(),
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
                CONF_FILTERS: [{CONF_MULTIPLY: 42.0}],
            },
            {
                CONF_ID: TEST_SENSOR_ID_1,
                CONF_FILTERS: [{CONF_MULTIPLY: 10.0}],
            },
            {
                CONF_ID: Remove(TEST_SENSOR_ID_2),
                CONF_FILTERS: [{CONF_OFFSET: 146.0}],
            },
        ],
    }

    actual = do_packages_pass(config)
    assert actual == expected
