"""lightwaverf.send_raw never used name; it is dropped with a warning until 2027.5.0."""

import pytest

from esphome.automation import ACTION_REGISTRY
import esphome.components.lightwaverf  # noqa: F401
from esphome.const import CONF_NAME, CONF_REPEAT

CODE = [0x04, 0x00, 0x00, 0x00, 0x0F, 0x03, 0x0D, 0x09, 0x08, 0x08]


def test_name_is_dropped_with_a_warning(caplog: pytest.LogCaptureFixture) -> None:
    config = ACTION_REGISTRY["lightwaverf.send_raw"].schema(
        {"code": CODE, CONF_NAME: "Sofa"}
    )
    assert CONF_NAME not in config
    assert "'name' on lightwaverf.send_raw has no effect" in caplog.text
    assert "2027.5.0" in caplog.text


def test_name_is_no_longer_required(caplog: pytest.LogCaptureFixture) -> None:
    config = ACTION_REGISTRY["lightwaverf.send_raw"].schema({"code": CODE})
    assert config[CONF_REPEAT] == 10
    assert "lightwaverf" not in caplog.text
