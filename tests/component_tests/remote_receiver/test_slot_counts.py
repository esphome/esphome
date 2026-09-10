"""Listener and dumper StaticVector sizes come from codegen slot counts."""

from collections.abc import Callable
from pathlib import Path

from ..helpers import get_define_value


def test_dumper_and_listener_counts(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("receiver_with_dumpers.yaml"))
    # nec and rc_switch dumpers
    assert get_define_value("REMOTE_BASE_DUMPER_COUNT") == "2"
    # on_nec trigger plus the remote_receiver binary sensor
    assert get_define_value("REMOTE_BASE_LISTENER_COUNT") == "2"


def test_bare_receiver_emits_no_counts(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("receiver_bare.yaml"))
    assert get_define_value("REMOTE_BASE_DUMPER_COUNT") is None
    assert get_define_value("REMOTE_BASE_LISTENER_COUNT") is None


def test_proxy_receivers_count_as_listeners(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("receiver_with_proxies.yaml"))
    # infrared and radio_frequency ir_rf_proxy platforms each listen
    assert get_define_value("REMOTE_BASE_LISTENER_COUNT") == "2"
    assert get_define_value("REMOTE_BASE_DUMPER_COUNT") is None
