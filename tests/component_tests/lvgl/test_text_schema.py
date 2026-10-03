"""Regression tests for LVGL's `text:` schema validator (`_validate_text`).

`_validate_text` was reworked to let a `text:` dict use the `entity_state:`/`argument:`
lambda shorthand (previously any dict without `time_format:`/`mapping:` fell through to
the printf schema unconditionally, which would reject a shorthand dict as missing
`format:`). That rework must not regress the error for a dict that uses none of the
recognized keys at all: it should still point at the missing `format:` key rather than
report the generic "expected string" from `cv.templatable(cv.string)`.
"""

from __future__ import annotations

import pytest

from esphome.components.lvgl.schemas import _validate_text
from esphome.config_validation import Invalid, Lambda


def test_malformed_dict_reports_missing_format_key() -> None:
    """A dict with none of format/time_format/mapping/entity_state/argument must still
    raise a "required key not provided" error, not "expected string"."""
    with pytest.raises(Invalid, match=r"required key not provided.*'format'"):
        _validate_text({})


def test_argument_shorthand_still_reaches_templatable() -> None:
    result = _validate_text({"argument": "x"})
    assert isinstance(result, Lambda)
    assert result.value == "return x;"


def test_entity_state_shorthand_still_reaches_templatable() -> None:
    result = _validate_text({"entity_state": "some_text_sensor"})
    assert isinstance(result, Lambda)
    assert result.value == "return id(some_text_sensor).state;"
