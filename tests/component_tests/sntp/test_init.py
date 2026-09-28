"""Tests for SNTP time configuration validation."""

from __future__ import annotations

import logging
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.sntp.time import CONF_SNTP, _sntp_final_validate
from esphome.const import CONF_ID, CONF_PLATFORM, CONF_SERVERS, CONF_TIME
from esphome.core import ID
import esphome.final_validate as fv


@pytest.mark.parametrize(
    ("time_configs", "expected_count", "expected_servers", "warning_messages"),
    [
        pytest.param(
            [
                {
                    CONF_PLATFORM: CONF_SNTP,
                    CONF_ID: ID("sntp_time", is_manual=False),
                    CONF_SERVERS: ["192.168.1.1", "pool.ntp.org"],
                }
            ],
            1,
            ["192.168.1.1", "pool.ntp.org"],
            [],
            id="single_instance_no_merge",
        ),
        pytest.param(
            [
                {
                    CONF_PLATFORM: CONF_SNTP,
                    CONF_ID: ID("sntp_time_1", is_manual=False),
                    CONF_SERVERS: ["192.168.1.1", "pool.ntp.org"],
                },
                {
                    CONF_PLATFORM: CONF_SNTP,
                    CONF_ID: ID("sntp_time_2", is_manual=False),
                    CONF_SERVERS: ["192.168.1.2"],
                },
            ],
            1,
            ["192.168.1.1", "pool.ntp.org", "192.168.1.2"],
            ["Found and merged 2 SNTP time configurations into one instance"],
            id="two_instances_merged",
        ),
        pytest.param(
            [
                {
                    CONF_PLATFORM: CONF_SNTP,
                    CONF_ID: ID("sntp_time_1", is_manual=False),
                    CONF_SERVERS: ["192.168.1.1", "pool.ntp.org"],
                },
                {
                    CONF_PLATFORM: CONF_SNTP,
                    CONF_ID: ID("sntp_time_2", is_manual=False),
                    CONF_SERVERS: ["pool.ntp.org", "192.168.1.2"],
                },
            ],
            1,
            ["192.168.1.1", "pool.ntp.org", "192.168.1.2"],
            ["Found and merged 2 SNTP time configurations into one instance"],
            id="deduplication_preserves_order",
        ),
        pytest.param(
            [
                {
                    CONF_PLATFORM: CONF_SNTP,
                    CONF_ID: ID("sntp_time_1", is_manual=False),
                    CONF_SERVERS: ["192.168.1.1", "pool.ntp.org"],
                },
                {
                    CONF_PLATFORM: CONF_SNTP,
                    CONF_ID: ID("sntp_time_2", is_manual=False),
                    CONF_SERVERS: ["192.168.1.2", "pool2.ntp.org"],
                },
                {
                    CONF_PLATFORM: CONF_SNTP,
                    CONF_ID: ID("sntp_time_3", is_manual=False),
                    CONF_SERVERS: ["pool3.ntp.org"],
                },
            ],
            1,
            ["192.168.1.1", "pool.ntp.org", "192.168.1.2"],
            [
                "SNTP supports maximum 3 servers. Dropped excess server(s): ['pool2.ntp.org', 'pool3.ntp.org']",
                "Found and merged 3 SNTP time configurations into one instance",
            ],
            id="three_instances_drops_excess_servers",
        ),
        pytest.param(
            [
                {
                    CONF_PLATFORM: CONF_SNTP,
                    CONF_ID: ID("sntp_time_1", is_manual=False),
                    CONF_SERVERS: [
                        "192.168.1.1",
                        "pool.ntp.org",
                        "pool.ntp.org",
                        "192.168.1.1",
                    ],
                },
                {
                    CONF_PLATFORM: CONF_SNTP,
                    CONF_ID: ID("sntp_time_2", is_manual=False),
                    CONF_SERVERS: ["pool.ntp.org", "192.168.1.2"],
                },
            ],
            1,
            ["192.168.1.1", "pool.ntp.org", "192.168.1.2"],
            ["Found and merged 2 SNTP time configurations into one instance"],
            id="deduplication_multiple_duplicates",
        ),
    ],
)
def test_sntp_instance_merging(
    time_configs: list[dict[str, Any]],
    expected_count: int,
    expected_servers: list[str],
    warning_messages: list[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test SNTP instance merging behavior."""
    # Create a mock full config with time configs
    full_conf = {CONF_TIME: time_configs.copy()}

    # Set the context var
    token = fv.full_config.set(full_conf)
    try:
        with caplog.at_level(logging.WARNING):
            _sntp_final_validate({})

        # Get the updated config
        updated_conf = fv.full_config.get()

        # Check if merging occurred
        if len(time_configs) > 1:
            # Verify only one SNTP instance remains
            sntp_instances = [
                tc
                for tc in updated_conf[CONF_TIME]
                if tc.get(CONF_PLATFORM) == CONF_SNTP
            ]
            assert len(sntp_instances) == expected_count

            # Verify server list
            assert sntp_instances[0][CONF_SERVERS] == expected_servers

            # Verify warnings
            for expected_msg in warning_messages:
                assert any(
                    expected_msg in record.message for record in caplog.records
                ), f"Expected warning message '{expected_msg}' not found in log"
        else:
            # Single instance should not trigger merging or warnings
            assert len(caplog.records) == 0
            # Config should be unchanged
            assert updated_conf[CONF_TIME] == time_configs
    finally:
        fv.full_config.reset(token)


def test_sntp_inconsistent_manual_ids() -> None:
    """Test that inconsistent manual IDs raise an error."""
    # Create configs with manual IDs that are inconsistent
    time_configs = [
        {
            CONF_PLATFORM: CONF_SNTP,
            CONF_ID: ID("sntp_time_1", is_manual=True),
            CONF_SERVERS: ["192.168.1.1"],
        },
        {
            CONF_PLATFORM: CONF_SNTP,
            CONF_ID: ID("sntp_time_2", is_manual=True),
            CONF_SERVERS: ["192.168.1.2"],
        },
    ]

    full_conf = {CONF_TIME: time_configs}

    token = fv.full_config.set(full_conf)
    try:
        with pytest.raises(
            cv.Invalid,
            match="Found multiple SNTP configurations but id is inconsistent",
        ):
            _sntp_final_validate({})
    finally:
        fv.full_config.reset(token)


def test_sntp_with_other_time_platforms(caplog: pytest.LogCaptureFixture) -> None:
    """Test that SNTP merging doesn't affect other time platforms."""
    time_configs = [
        {
            CONF_PLATFORM: CONF_SNTP,
            CONF_ID: ID("sntp_time_1", is_manual=False),
            CONF_SERVERS: ["192.168.1.1"],
        },
        {
            CONF_PLATFORM: "homeassistant",
            CONF_ID: ID("homeassistant_time", is_manual=False),
        },
        {
            CONF_PLATFORM: CONF_SNTP,
            CONF_ID: ID("sntp_time_2", is_manual=False),
            CONF_SERVERS: ["192.168.1.2"],
        },
    ]

    full_conf = {CONF_TIME: time_configs.copy()}

    token = fv.full_config.set(full_conf)
    try:
        with caplog.at_level(logging.WARNING):
            _sntp_final_validate({})

        updated_conf = fv.full_config.get()

        # Should have 2 time platforms: 1 merged SNTP + 1 homeassistant
        assert len(updated_conf[CONF_TIME]) == 2

        # Find the platforms
        platforms = {tc[CONF_PLATFORM] for tc in updated_conf[CONF_TIME]}
        assert platforms == {CONF_SNTP, "homeassistant"}

        # Verify SNTP was merged
        sntp_instances = [
            tc for tc in updated_conf[CONF_TIME] if tc[CONF_PLATFORM] == CONF_SNTP
        ]
        assert len(sntp_instances) == 1
        assert sntp_instances[0][CONF_SERVERS] == ["192.168.1.1", "192.168.1.2"]
    finally:
        fv.full_config.reset(token)
