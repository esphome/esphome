"""pending and triggered never forward a code, so their schemas must not accept one."""

import pytest

from esphome import config_validation as cv
from esphome.automation import ACTION_REGISTRY
import esphome.components.alarm_control_panel  # noqa: F401


@pytest.mark.parametrize(
    "action", ["alarm_control_panel.pending", "alarm_control_panel.triggered"]
)
def test_code_is_rejected(action: str) -> None:
    with pytest.raises(cv.Invalid, match="code"):
        ACTION_REGISTRY[action].schema({"id": "acp1", "code": "1234"})


@pytest.mark.parametrize(
    "action",
    [
        "alarm_control_panel.arm_away",
        "alarm_control_panel.arm_home",
        "alarm_control_panel.arm_night",
        "alarm_control_panel.disarm",
    ],
)
def test_code_is_accepted(action: str) -> None:
    config = ACTION_REGISTRY[action].schema({"id": "acp1", "code": "1234"})
    assert config["code"] == "1234"
