"""libretiny_pwm rejects LN882H: LibreTiny has no analogWrite for that family."""

import pytest

from esphome import config_validation as cv
from esphome.components.libretiny.const import FAMILY_LN882H, KEY_FAMILY, KEY_LIBRETINY
from esphome.components.libretiny_pwm.output import CONFIG_SCHEMA
from esphome.core import CORE


def test_ln882h_is_rejected() -> None:
    CORE.data[KEY_LIBRETINY] = {KEY_FAMILY: FAMILY_LN882H}
    with pytest.raises(cv.Invalid, match="not available on LN882H"):
        CONFIG_SCHEMA({"id": "pwm", "pin": "PA5"})
