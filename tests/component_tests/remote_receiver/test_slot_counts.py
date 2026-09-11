"""Listener and dumper StaticVector sizes come from codegen slot counts."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.automation import ACTION_REGISTRY
from esphome.components import remote_base
import esphome.config_validation as cv

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
    # one proxy entity listens on each of the two receivers; every receiver's list gets the
    # capacity of the busiest one, so this is the largest per receiver count, not the sum
    assert get_define_value("REMOTE_BASE_LISTENER_COUNT") == "1"
    assert get_define_value("REMOTE_BASE_DUMPER_COUNT") is None


def test_only_used_protocol_sources_are_compiled(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("receiver_with_dumpers.yaml"))
    excluded = set(remote_base.FILTER_SOURCE_FILES())
    assert "nec_protocol.cpp" not in excluded
    assert "rc_switch_protocol.cpp" not in excluded
    assert "sony_protocol.cpp" in excluded
    assert "remote_base.cpp" not in excluded


def test_every_registry_name_maps_to_a_protocol_source() -> None:
    """A registry name must resolve to a source file or request_protocol rejects it."""
    names = (
        set(remote_base.BINARY_SENSOR_REGISTRY)
        | set(remote_base.DUMPER_REGISTRY)
        | {key.removeprefix("on_") for key in remote_base.TRIGGER_REGISTRY}
        | {
            key.removeprefix("remote_transmitter.transmit_")
            for key in ACTION_REGISTRY
            if key.startswith("remote_transmitter.transmit_")
        }
    )
    assert len(names) > 40
    for name in names:
        assert remote_base._protocol_stem(name) in remote_base._PROTOCOL_STEMS, name


def test_request_protocol_rejects_unknown_names() -> None:
    """A misspelled protocol would otherwise surface only as a link error."""
    with pytest.raises(ValueError, match="Unknown remote protocol 'toshiba'"):
        remote_base.request_protocol("toshiba")


def test_dump_list_is_deduplicated_across_forms() -> None:
    dumpers = remote_base.validate_dumpers(["raw", {"raw": None}, "nec", "nec"])
    assert [
        next(k for k in entry if k in remote_base.DUMPER_REGISTRY) for entry in dumpers
    ] == ["raw", "nec"]


@pytest.mark.parametrize("bad", [["nec", None], [5]])
def test_dump_list_rejects_invalid_entries_with_a_validation_error(bad: list) -> None:
    with pytest.raises(cv.Invalid):
        remote_base.validate_dumpers(bad)
