"""Tests for network component."""

from collections.abc import Callable
from pathlib import Path

import pytest
from voluptuous import Invalid

from esphome.components.network import (
    CONF_ENABLE_IPV4,
    KEY_REQUIRE_IPV4,
    KEY_REQUIRE_IPV6,
    _final_validate,
    has_ipv4_requirement,
    require_ipv4,
)
from esphome.const import (
    CONF_ENABLE_IPV6,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_HOST,
)
from esphome.core import CORE, Version
import esphome.final_validate as fv


@pytest.fixture(autouse=True)
def _clear_core_data():
    """Wipe CORE.data and reset fv.full_config so each test starts clean."""
    CORE.data.clear()
    token = fv.full_config.set({})
    yield
    fv.full_config.reset(token)
    CORE.data.clear()


def test_has_ipv4_requirement_false_by_default() -> None:
    assert has_ipv4_requirement() is False


def test_require_ipv4_sets_requirement() -> None:
    config = {"foo": "bar"}
    result = require_ipv4(config)
    assert result is config
    assert has_ipv4_requirement() is True


def test_require_ipv4_is_idempotent() -> None:
    require_ipv4({})
    require_ipv4({})
    assert CORE.data[KEY_REQUIRE_IPV4] is True


def test_ipv4_dropped_when_explicitly_disabled(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """enable_ipv4 defaults to True; IPv4 only drops out of the build when the user
    explicitly writes 'enable_ipv4: false' and nothing requires it."""
    generate_main(component_config_path("ipv6_only_ipv4_explicit_disable.yaml"))
    define = next((d for d in CORE.defines if d.name == "USE_NETWORK_IPV4"), None)
    assert define is not None
    assert str(define.value) == "false"


def test_ipv4_stays_on_with_ipv6_enabled_by_default(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without an explicit 'enable_ipv4: false', IPv4 stays on even with IPv6 enabled --
    the user must opt into dropping it, it is never selected for them."""
    generate_main(component_config_path("ipv6_only.yaml"))
    define = next((d for d in CORE.defines if d.name == "USE_NETWORK_IPV4"), None)
    assert define is not None
    assert str(define.value) == "true"


def test_ipv4_stays_on_when_required(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A component that calls require_ipv4 (e.g. wifi) keeps IPv4 even with IPv6 enabled."""
    generate_main(component_config_path("ipv4_required_with_ipv6.yaml"))
    define = next((d for d in CORE.defines if d.name == "USE_NETWORK_IPV4"), None)
    assert define is not None
    assert str(define.value) == "true"


def test_ipv4_stays_on_by_default(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without IPv6 explicitly enabled, IPv4 is never dropped -- matches pre-existing behavior."""
    generate_main(component_config_path("wifi_only.yaml"))
    define = next((d for d in CORE.defines if d.name == "USE_NETWORK_IPV4"), None)
    assert define is not None
    assert str(define.value) == "true"


def test_ipv4_stays_on_when_ipv6_auto_enabled_by_requirement(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The openthread component calls require_ipv6() with no explicit 'network:' block --
    IPv6 auto-resolves to True, but IPv4 stays on since it is opt-out, not automatic."""
    generate_main(component_config_path("openthread_no_explicit_network.yaml"))
    defines = {d.name: d for d in CORE.defines}
    assert str(defines["USE_NETWORK_IPV6"].value) == "true"
    assert str(defines["USE_NETWORK_IPV4"].value) == "true"


def test_ipv4_turns_on_when_ipv6_explicitly_off_and_nothing_required(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Isolates the 'not enable_ipv6' branch: no component requires IPv4 or IPv6,
    and IPv6 is explicitly off -- IPv4 must stay on so there's still an IP stack."""
    generate_main(component_config_path("ipv6_off_nothing_required.yaml"))
    defines = {d.name: d for d in CORE.defines}
    assert str(defines["USE_NETWORK_IPV6"].value) == "false"
    assert str(defines["USE_NETWORK_IPV4"].value) == "true"


def test_ipv4_and_ipv6_both_stay_on_when_both_required(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The udp component requires IPv4 and openthread requires IPv6 in the same config -- both
    requirements must resolve independently, neither clobbering the other."""
    generate_main(component_config_path("ipv4_and_ipv6_both_required.yaml"))
    defines = {d.name: d for d in CORE.defines}
    assert str(defines["USE_NETWORK_IPV6"].value) == "true"
    assert str(defines["USE_NETWORK_IPV4"].value) == "true"


def test_wake_on_lan_keeps_ipv4_under_openthread(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """wake_on_lan unconditionally broadcasts over IPv4 (AF_INET, 255.255.255.255) --
    it must register require_ipv4 so an openthread-only config (which auto-enables
    IPv6 and would otherwise auto-drop IPv4) doesn't silently break it at runtime."""
    generate_main(component_config_path("openthread_with_wake_on_lan.yaml"))
    define = next((d for d in CORE.defines if d.name == "USE_NETWORK_IPV4"), None)
    assert define is not None
    assert str(define.value) == "true"


def test_final_validate_rejects_explicit_ipv6_disable_when_required() -> None:
    """A component's require_ipv6() conflicting with an explicit 'enable_ipv6: false'
    is a config violation, not a value to silently override. Once the schema no longer
    defaults enable_ipv6 to False, an explicit False and an absent/defaulted value are
    distinguishable directly on the config dict -- no separate raw-config detection pass
    is needed."""
    CORE.data[KEY_REQUIRE_IPV6] = True
    with pytest.raises(Invalid, match="explicitly disables it"):
        _final_validate({CONF_ENABLE_IPV6: False})


def test_final_validate_accepts_explicit_ipv6_disable_when_not_required() -> None:
    """An explicit 'enable_ipv6: false' is fine as long as nothing requires IPv6."""
    _final_validate({CONF_ENABLE_IPV6: False})  # must not raise


def test_final_validate_resolves_ipv6_default_when_required() -> None:
    """Backstop for component validation order: a require_ipv6() call from a
    component processed after 'network' still forces the effective value to True
    when the user never set 'enable_ipv6' at all (absent, not defaulted-false).

    The flip re-runs the framework-version gate, which reads CORE.data[KEY_CORE] --
    populate it with a platform/framework this feature is unconditionally
    supported on, since that gate isn't what this test is checking.
    """
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_HOST,
        KEY_TARGET_FRAMEWORK: "host",
        KEY_FRAMEWORK_VERSION: Version(0, 0, 0),
    }
    CORE.data[KEY_REQUIRE_IPV6] = True
    config = {}
    _final_validate(config)
    assert config[CONF_ENABLE_IPV6] is True


def test_final_validate_rejects_explicit_ipv4_disable_when_required() -> None:
    """A component's require_ipv4() conflicting with an explicit 'enable_ipv4: false'
    is a config violation -- the user must opt into disabling IPv4, but that choice
    still can't override an actual requirement."""
    CORE.data[KEY_REQUIRE_IPV4] = True
    with pytest.raises(Invalid, match="explicitly disables it"):
        _final_validate({CONF_ENABLE_IPV4: False})


def test_final_validate_accepts_explicit_ipv4_disable_when_not_required() -> None:
    """An explicit 'enable_ipv4: false' is fine as long as nothing requires IPv4."""
    _final_validate({CONF_ENABLE_IPV4: False})  # must not raise


def test_final_validate_accepts_ipv4_default_when_required() -> None:
    """The default 'enable_ipv4: true' never conflicts with a require_ipv4() call."""
    CORE.data[KEY_REQUIRE_IPV4] = True
    _final_validate({CONF_ENABLE_IPV4: True})  # must not raise


# Every non-nRF52 platform this PR touches calls network.require_ipv4() unconditionally
# from its own CONFIG_SCHEMA (esp8266, rp2, bk72xx/libretiny, esp32 when using Arduino).
# 'network' itself is only ever loaded when something pulls it in (wifi, ethernet,
# openthread, an explicit 'network:' block, or host's AUTO_LOAD) -- so on a config with
# none of that, require_ipv4() must be a pure no-op: 'network' never validates, so
# nothing ever reads the flag it set, and no USE_NETWORK_IPV4/IPV6 define is emitted.
# host is excluded here since it AUTO_LOADs network unconditionally -- there's no
# "no network" case to construct for it.
@pytest.mark.parametrize(
    "config_file",
    [
        "esp8266_no_network.yaml",
        "rp2_no_network.yaml",
        "bk72xx_no_network.yaml",
        "esp32_idf_no_network.yaml",
        "esp32_arduino_no_network.yaml",
        "nrf52_no_network.yaml",
    ],
)
def test_require_ipv4_is_a_noop_without_network_component(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    generate_main(component_config_path(config_file))  # must not raise
    names = {d.name for d in CORE.defines}
    assert "USE_NETWORK_IPV4" not in names
    assert "USE_NETWORK_IPV6" not in names


def test_esp32_arduino_keeps_ipv4_with_network_and_no_requirement(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """CONFIG_LWIP_IPV4 has no Arduino override the way CONFIG_LWIP_IPV6 does, so
    esp32/_require_ipv4_on_arduino() must keep IPv4 on even with nothing else
    requiring it -- regression test for the review finding that this used to break
    ip_address.h's Arduino-only conversion operators."""
    generate_main(component_config_path("esp32_arduino_ipv6_no_ipv4_requirement.yaml"))
    define = next((d for d in CORE.defines if d.name == "USE_NETWORK_IPV4"), None)
    assert define is not None
    assert str(define.value) == "true"


def test_host_keeps_ipv4_with_network_and_no_requirement(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """host/require_ipv4() must keep IPv4 on: there's no CONFIG_LWIP_IPV4-equivalent
    build-system wiring on host, so dropping it saves nothing and only breaks
    socket.cpp's IPv4 handling -- regression test for the review finding that
    USE_NETWORK_IPV4 used to silently flip false here."""
    generate_main(component_config_path("host_ipv6_no_ipv4_requirement.yaml"))
    define = next((d for d in CORE.defines if d.name == "USE_NETWORK_IPV4"), None)
    assert define is not None
    assert str(define.value) == "true"


def test_nrf52_defaults_to_ipv6_without_openthread(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """nrf52/require_ipv6() restores the platform's IPv6 invariant without depending
    on openthread specifically -- regression test for the review finding that removing
    the old SplitDefault(nrf52=True) + validate_ipv6 left any nRF52 config without
    openthread getting the inverse of its actual capability. IPv4 stays on by default
    since dropping it is opt-out, not automatic."""
    generate_main(component_config_path("nrf52_network_no_openthread.yaml"))
    defines = {d.name: d for d in CORE.defines}
    assert str(defines["USE_NETWORK_IPV6"].value) == "true"
    assert str(defines["USE_NETWORK_IPV4"].value) == "true"
