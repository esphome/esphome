"""Tests for speaker media player conditional PSRAM loading."""

import pytest

from esphome.components.speaker.media_player import _get_auto_load
from esphome.core import CORE


@pytest.fixture(autouse=True)
def reset_core_config():
    """Reset CORE.raw_config before each test."""
    original_config = getattr(CORE, 'raw_config', None)
    CORE.raw_config = {}
    yield
    CORE.raw_config = original_config


def test_auto_load_no_media_player():
    """Test that PSRAM is not loaded when no media player is configured."""
    CORE.raw_config = {
        'esphome': {'name': 'test'},
        'wifi': {'ssid': 'test'},
    }
    
    result = _get_auto_load()
    assert result == ['audio']


def test_auto_load_speaker_with_codec_support():
    """Test that PSRAM is loaded when speaker media player has codec support enabled."""
    CORE.raw_config = {
        'media_player': [{
            'platform': 'speaker',
            'announcement_pipeline': {'speaker': 'test_speaker'},
            'codec_support_enabled': True,
        }]
    }
    
    result = _get_auto_load()
    assert result == ['audio', 'psram']


def test_auto_load_speaker_with_task_stack_in_psram():
    """Test that PSRAM is loaded when speaker media player has task stack in PSRAM enabled."""
    CORE.raw_config = {
        'media_player': [{
            'platform': 'speaker',
            'announcement_pipeline': {'speaker': 'test_speaker'},
            'task_stack_in_psram': True,
        }]
    }
    
    result = _get_auto_load()
    assert result == ['audio', 'psram']


def test_auto_load_speaker_with_both_disabled():
    """Test that PSRAM is not loaded when speaker media player has both features disabled."""
    CORE.raw_config = {
        'media_player': [{
            'platform': 'speaker',
            'announcement_pipeline': {'speaker': 'test_speaker'},
            'codec_support_enabled': False,
            'task_stack_in_psram': False,
        }]
    }
    
    result = _get_auto_load()
    assert result == ['audio']


def test_auto_load_speaker_with_defaults():
    """Test that PSRAM is loaded when speaker media player uses default values (codec_support_enabled defaults to True)."""
    CORE.raw_config = {
        'media_player': [{
            'platform': 'speaker',
            'announcement_pipeline': {'speaker': 'test_speaker'},
            # codec_support_enabled defaults to True, task_stack_in_psram defaults to False
        }]
    }
    
    result = _get_auto_load()
    assert result == ['audio', 'psram']


def test_auto_load_non_speaker_platform():
    """Test that PSRAM is not loaded for non-speaker media player platforms."""
    CORE.raw_config = {
        'media_player': [{
            'platform': 'some_other_platform',
            'codec_support_enabled': True,
            'task_stack_in_psram': True,
        }]
    }
    
    result = _get_auto_load()
    assert result == ['audio']


def test_auto_load_multiple_media_players():
    """Test that PSRAM is loaded if any speaker media player needs it."""
    CORE.raw_config = {
        'media_player': [
            {
                'platform': 'speaker',
                'announcement_pipeline': {'speaker': 'test_speaker1'},
                'codec_support_enabled': False,
                'task_stack_in_psram': False,
            },
            {
                'platform': 'speaker', 
                'announcement_pipeline': {'speaker': 'test_speaker2'},
                'codec_support_enabled': True,  # This should trigger PSRAM loading
            }
        ]
    }
    
    result = _get_auto_load()
    assert result == ['audio', 'psram']


def test_auto_load_mixed_platforms():
    """Test PSRAM loading with mixed speaker and non-speaker platforms."""
    CORE.raw_config = {
        'media_player': [
            {
                'platform': 'some_other_platform',
                'codec_support_enabled': True,
            },
            {
                'platform': 'speaker',
                'announcement_pipeline': {'speaker': 'test_speaker'},
                'codec_support_enabled': False,
                'task_stack_in_psram': True,  # This should trigger PSRAM loading
            }
        ]
    }
    
    result = _get_auto_load()
    assert result == ['audio', 'psram']


def test_auto_load_single_config_dict():
    """Test that it works when media_player is a single dict instead of a list."""
    CORE.raw_config = {
        'media_player': {
            'platform': 'speaker',
            'announcement_pipeline': {'speaker': 'test_speaker'},
            'codec_support_enabled': True,
        }
    }
    
    result = _get_auto_load()
    assert result == ['audio', 'psram']


def test_auto_load_empty_media_player_list():
    """Test that it works with an empty media_player list."""
    CORE.raw_config = {
        'media_player': []
    }
    
    result = _get_auto_load()
    assert result == ['audio']