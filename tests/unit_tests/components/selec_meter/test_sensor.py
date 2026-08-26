"""Tests for selec_meter model/sensor validation and byte_order defaulting."""

from __future__ import annotations

import pytest

from esphome.components.selec_meter import sensor as selec_meter
import esphome.config_validation as cv


def test_validate_model_sensors_rejects_em4m_only_sensor_on_em2m() -> None:
    config = {
        selec_meter.CONF_MODEL: selec_meter.MODEL_EM2M,
        selec_meter.CONF_VOLTAGE_L1: {},
    }
    with pytest.raises(cv.Invalid, match="requires 'model: em4m'"):
        selec_meter._validate_model_sensors(config)


def test_validate_model_sensors_rejects_serial_number_on_em2m() -> None:
    config = {
        selec_meter.CONF_MODEL: selec_meter.MODEL_EM2M,
        selec_meter.CONF_SERIAL_NUMBER: {},
    }
    with pytest.raises(cv.Invalid, match="requires 'model: em4m'"):
        selec_meter._validate_model_sensors(config)


def test_validate_model_sensors_rejects_em2m_only_sensor_on_em4m() -> None:
    config = {
        selec_meter.CONF_MODEL: selec_meter.MODEL_EM4M,
        selec_meter.CONF_TOTAL_ACTIVE_ENERGY: {},
    }
    with pytest.raises(cv.Invalid, match="no non-DG equivalent"):
        selec_meter._validate_model_sensors(config)


def test_validate_model_sensors_allows_em4m_sensors_on_em4m() -> None:
    config = {
        selec_meter.CONF_MODEL: selec_meter.MODEL_EM4M,
        selec_meter.CONF_VOLTAGE_L1: {},
        selec_meter.CONF_SERIAL_NUMBER: {},
    }
    assert selec_meter._validate_model_sensors(config) == config


def test_validate_model_sensors_allows_shared_sensors_on_either_model() -> None:
    for model in (selec_meter.MODEL_EM2M, selec_meter.MODEL_EM4M):
        config = {
            selec_meter.CONF_MODEL: model,
            selec_meter.CONF_VOLTAGE: {},
        }
        assert selec_meter._validate_model_sensors(config) == config


def test_default_word_swap_matches_pre_pr_em2m_behavior() -> None:
    # EM2M defaulted to word-swapped decoding before byte_order existed -- this default
    # is what keeps existing EM2M configs decoding identically without setting byte_order.
    assert selec_meter.DEFAULT_WORD_SWAP[selec_meter.MODEL_EM2M] is True


def test_default_word_swap_em4m_matches_factory_default() -> None:
    assert selec_meter.DEFAULT_WORD_SWAP[selec_meter.MODEL_EM4M] is False


def test_byte_order_word_swap_mapping() -> None:
    assert selec_meter.BYTE_ORDER_WORD_SWAP[selec_meter.BYTE_ORDER_MSRF] is False
    assert selec_meter.BYTE_ORDER_WORD_SWAP[selec_meter.BYTE_ORDER_LSRF] is True
