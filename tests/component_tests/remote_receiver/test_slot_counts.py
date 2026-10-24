"""Listener and dumper StaticVector sizes come from codegen slot counts."""

from collections.abc import Callable, Generator
from pathlib import Path
import re
import sys

import pytest

from esphome import loader
from esphome.automation import ACTION_REGISTRY
from esphome.components import remote_base
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.cpp_generator import MockObj

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
    main_cpp = generate_main(component_config_path("receiver_bare.yaml"))
    # the RMT ring is sized in setup() unless buffer_size is set
    assert "set_buffer_size" not in main_cpp
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


@pytest.fixture
def restore_protocol_registries() -> Generator[None]:
    """Loading an external protocol component adds to module-level registries; undo that.

    The loader caches the component too, so drop it or a second load would skip the
    decorators and leave the restored registries without the external names.
    """
    registries = (
        remote_base.BINARY_SENSOR_REGISTRY,
        remote_base.TRIGGER_REGISTRY,
        remote_base.DUMPER_REGISTRY,
        ACTION_REGISTRY,
    )
    saved = [dict(registry) for registry in registries]
    yield
    for registry, entries in zip(registries, saved, strict=True):
        registry.clear()
        registry.update(entries)
    loader._COMPONENT_CACHE.pop("fake_protocol", None)
    sys.modules.pop("esphome.components.fake_protocol", None)


@pytest.mark.usefixtures("restore_protocol_registries")
def test_external_protocols_register_without_a_remote_base_source(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """An external protocol goes through all four decorators without a source file here, so no define is emitted."""
    main_cpp = generate_main(
        component_config_path("receiver_with_external_protocol.yaml")
    )
    defines = {define.name for define in CORE.defines}
    assert "USE_REMOTE_PROTOCOL_NEC" in defines
    assert "USE_REMOTE_PROTOCOL_FAKE" not in defines
    for cls in ("FakeBinarySensor", "FakeTrigger", "FakeDumper", "FakeAction"):
        assert f"fake_protocol::{cls}" in main_cpp, cls
    # fake and nec dumpers; on_fake and on_nec triggers plus the fake binary sensor
    assert get_define_value("REMOTE_BASE_DUMPER_COUNT") == "2"
    assert get_define_value("REMOTE_BASE_LISTENER_COUNT") == "3"


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


def test_dumper_count_is_the_busiest_receivers(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("receiver_two_dumper_lists.yaml"))
    # nec, rc_switch and raw on one receiver, rc_switch on the other
    assert get_define_value("REMOTE_BASE_DUMPER_COUNT") == "3"


@pytest.mark.usefixtures("restore_protocol_registries")
def test_external_dumpers_are_constructed_in_ram(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A dumper registered outside remote_base may hold state."""
    main_cpp = generate_main(
        component_config_path("receiver_with_external_protocol.yaml")
    )
    assert re.search(r"new\(\w+\) fake_protocol::FakeDumper\(\);", main_cpp)


def test_each_receiver_gets_its_own_dumper_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("receiver_two_dumper_lists.yaml"))
    assert "register_dumper" not in main_cpp
    assert re.search(r"ir_rcvr->set_dumpers\(remote_dumpers\w*, 3\);", main_cpp)
    assert re.search(r"rf_rcvr->set_dumpers\(remote_dumpers\w*, 1\);", main_cpp)


def test_add_dumper_after_the_tables_were_emitted_raises() -> None:
    """The tables are written once at FINAL; a later dumper would otherwise be dropped."""
    remote_base._get_dumper_tables().emitted = True
    try:
        with pytest.raises(ValueError, match="already emitted"):
            remote_base.add_dumper(MockObj("rcvr", "->"), MockObj("dumper", "->"))
    finally:
        CORE.data.pop(remote_base.DOMAIN)
