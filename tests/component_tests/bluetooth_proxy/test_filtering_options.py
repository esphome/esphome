"""Advertisement-filter option validation, and the compile gate that keeps the
feature free for proxies that do not configure it."""

import pytest

from esphome import config_validation as cv
from esphome.components import bluetooth_proxy


class TestIrkValidation:
    """IRKs are 16 bytes; separators people paste are normalised away."""

    @pytest.mark.parametrize(
        "value",
        [
            "00112233445566778899aabbccddeeff",
            "00:11:22:33:44:55:66:77:88:99:aa:bb:cc:dd:ee:ff",
            "00-11-22-33-44-55-66-77-88-99-AA-BB-CC-DD-EE-FF",
            "00112233445566778899AABBCCDDEEFF",
        ],
    )
    def test_accepted_and_normalised(self, value: str) -> None:
        assert (
            bluetooth_proxy._validate_irk(value) == "00112233445566778899aabbccddeeff"
        )

    @pytest.mark.parametrize(
        "value",
        [
            "00112233445566778899aabbccddee",  # 15 bytes
            "00112233445566778899aabbccddeeffff",  # 17 bytes
            "00112233445566778899aabbccddeegg",  # not hex
        ],
    )
    def test_rejected(self, value: str) -> None:
        with pytest.raises(cv.Invalid):
            bluetooth_proxy._validate_irk(value)


class TestServiceUuidValidation:
    """The allowlist takes 16-bit shorts and full 128-bit UUIDs."""

    def test_short_uuid_stays_int(self) -> None:
        assert bluetooth_proxy._validate_service_uuid(0xFFF6) == 0xFFF6

    @pytest.mark.parametrize(
        "value",
        [
            "00467768-6228-2272-4663-277478268000",
            "00467768622822724663277478268000",
            "00467768:6228:2272:4663:277478268000",
        ],
    )
    def test_long_uuid_normalised_to_hex(self, value: str) -> None:
        assert (
            bluetooth_proxy._validate_service_uuid(value)
            == "00467768622822724663277478268000"
        )

    def test_bad_length_rejected(self) -> None:
        with pytest.raises(cv.Invalid):
            bluetooth_proxy._validate_service_uuid("00467768-6228-2272")


class TestFilteringGate:
    """USE_BLUETOOTH_PROXY_FILTERING must be emitted only when a filter is set,
    so an unconfigured proxy compiles byte-identically to before."""

    @staticmethod
    def _config(**overrides):
        base = {
            bluetooth_proxy.CONF_RSSI_THRESHOLD: -127,
            bluetooth_proxy.CONF_IRKS: [],
            bluetooth_proxy.CONF_NAME_BLOCKLIST: [],
            bluetooth_proxy.CONF_MAC_ALLOWLIST: [],
            bluetooth_proxy.CONF_MANUFACTURER_BLOCKLIST: [],
            bluetooth_proxy.CONF_SERVICE_UUID_ALLOWLIST: [],
            bluetooth_proxy.CONF_DROP_NON_RESOLVABLE: False,
        }
        base.update(overrides)
        return base

    def test_defaults_do_not_enable_filtering(self) -> None:
        assert not bluetooth_proxy._filtering_configured(self._config())

    @pytest.mark.parametrize(
        ("key", "value"),
        [
            (bluetooth_proxy.CONF_RSSI_THRESHOLD, -85),
            (bluetooth_proxy.CONF_IRKS, ["00112233445566778899aabbccddeeff"]),
            (bluetooth_proxy.CONF_NAME_BLOCKLIST, ["noisy"]),
            (bluetooth_proxy.CONF_MAC_ALLOWLIST, [0xAABBCCDDEEFF]),
            (bluetooth_proxy.CONF_MANUFACTURER_BLOCKLIST, [0x004C]),
            (bluetooth_proxy.CONF_SERVICE_UUID_ALLOWLIST, [0xFFF6]),
            (bluetooth_proxy.CONF_DROP_NON_RESOLVABLE, True),
        ],
    )
    def test_any_single_option_enables_filtering(self, key, value) -> None:
        assert bluetooth_proxy._filtering_configured(self._config(**{key: value}))
