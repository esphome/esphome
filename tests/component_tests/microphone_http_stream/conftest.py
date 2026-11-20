"""Fixtures for the microphone HTTP stream tests."""

from collections.abc import Generator

import pytest

from esphome.config import Config
import esphome.final_validate as fv


@pytest.fixture
def full_config() -> Generator[Config, None, None]:
    config = Config()
    token = fv.full_config.set(config)
    yield config
    fv.full_config.reset(token)
