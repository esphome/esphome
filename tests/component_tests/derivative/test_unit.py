import pytest

from esphome.components.derivative.sensor import inherit_unit_of_measurement


@pytest.mark.parametrize(
    ("uom", "time_unit", "expected"),
    [
        ("m", "s", "m/s"),
        ("kWh", "h", "kW"),
        ("Ah", "h", "A"),
        ("Wh", "s", "Wh/s"),
        ("km/h", "h", "km/h/h"),
        ("ms", "s", "ms/s"),
        ("h", "h", "h/h"),
        ("L/min", "min", "L/min/min"),
        ("mAh", "h", "mA"),
        ("mph", "h", "mph/h"),
        ("lbs", "s", "lbs/s"),
        ("rad", "d", "rad/d"),
        ("kvarh", "h", "kvar"),
        ("VAh", "h", "VA"),
    ],
)
def test_inherit_unit_of_measurement(uom: str, time_unit: str, expected: str) -> None:
    assert inherit_unit_of_measurement(uom, {"time_unit": time_unit}) == expected
