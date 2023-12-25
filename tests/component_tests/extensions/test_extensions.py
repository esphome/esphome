"""Tests for the extensions component."""

import pytest

import esphome.config_validation as cv

from esphome.const import (
    CONF_EXTENSIONS,
    CONF_ID,
    CONF_NAME,
    CONF_SENSOR,
)
from esphome.components.extensions import do_extensions_pass

# Test strings
TEST_OBJECT_NAME_1 = "test_object_name_1"
TEST_SENSOR_NAME_1 = "test_sensor_name_1"
TEST_SENSOR_NAME_2 = "test_sensor_name_2"
TEST_SENSOR_ID_1 = "test_sensor_id_1"
TEST_SENSOR_ID_2 = "test_sensor_id_2"


def test_basic():
    """
    Ensure that the name provided by the extension is added to the object.
    """
    config = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
            }
        ],
        CONF_EXTENSIONS: {
            TEST_SENSOR_ID_1: {
                CONF_NAME: TEST_SENSOR_NAME_1,
            }
        },
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
                CONF_NAME: TEST_SENSOR_NAME_1,
            }
        ],
    }

    actual = do_extensions_pass(config)
    assert actual == expected


def test_basic_override():
    """
    Ensure that the name provided by the extension overrides the one in the object.
    """
    config = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
                CONF_NAME: TEST_SENSOR_NAME_1,
            }
        ],
        CONF_EXTENSIONS: {
            TEST_SENSOR_ID_1: {
                CONF_NAME: TEST_SENSOR_NAME_2,
            }
        },
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
                CONF_NAME: TEST_SENSOR_NAME_2,
            }
        ],
    }

    actual = do_extensions_pass(config)
    assert actual == expected


def test_embedded():
    """
    Ensure that the name provided by the extension is added to the embedded object.
    """
    config = {
        CONF_SENSOR: [
            {
                TEST_OBJECT_NAME_1: {
                    CONF_ID: TEST_SENSOR_ID_1,
                },
            }
        ],
        CONF_EXTENSIONS: {
            TEST_SENSOR_ID_1: {
                CONF_NAME: TEST_SENSOR_NAME_1,
            }
        },
    }

    expected = {
        CONF_SENSOR: [
            {
                TEST_OBJECT_NAME_1: {
                    CONF_ID: TEST_SENSOR_ID_1,
                    CONF_NAME: TEST_SENSOR_NAME_1,
                },
            }
        ],
    }

    actual = do_extensions_pass(config)
    assert actual == expected


def test_two_pass_embedded():
    """
    Ensure that the name provided by an extension is added to the object added by a previous extension.
    """
    config = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
            }
        ],
        CONF_EXTENSIONS: {
            TEST_SENSOR_ID_1: {
                TEST_OBJECT_NAME_1: {
                    CONF_ID: TEST_SENSOR_ID_2,
                },
            },
            TEST_SENSOR_ID_2: {
                CONF_NAME: TEST_SENSOR_NAME_2,
            },
        },
    }

    expected = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
                TEST_OBJECT_NAME_1: {
                    CONF_ID: TEST_SENSOR_ID_2,
                    CONF_NAME: TEST_SENSOR_NAME_2,
                },
            }
        ],
    }

    actual = do_extensions_pass(config)
    assert actual == expected


def test_not_found():
    """
    Ensure that a non-matching ID is reported.
    """
    config = {
        CONF_SENSOR: [
            {
                CONF_ID: TEST_SENSOR_ID_1,
            }
        ],
        CONF_EXTENSIONS: {
            TEST_SENSOR_ID_2: {
                CONF_NAME: TEST_SENSOR_NAME_1,
            }
        },
    }

    with pytest.raises(cv.MultipleInvalid, match=f"extension ID {TEST_SENSOR_ID_2}"):
        do_extensions_pass(config)
