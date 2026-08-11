"""Tests for the ``rp2`` target-platform component.

``rp2`` is the canonical name for the Raspberry Pi RP-series target
platform. ``rp2040`` is a deprecated alias declared via
``ALIASES = ["rp2040"]`` on the rp2 component — the framework
(see ``esphome/loader.py`` and ``esphome/config.py``) handles both
Python-import aliasing (via a ``sys.meta_path`` finder) and YAML-key
aliasing (via a pre-pass in ``validate_config``), so there is no
hand-rolled shim in ``esphome/components/rp2040/``.

These tests pin down the canonical board helpers; the alias contract
itself (Python imports, YAML key rename, deprecation warning) is covered
by the framework tests under ``tests/unit_tests/``.
"""

from pathlib import Path


def test_board_id_has_wifi_for_known_wifi_board() -> None:
    """``rpipicow`` is the canonical Pico W → True."""
    from esphome.components import rp2

    assert rp2.board_id_has_wifi("rpipicow") is True


def test_board_id_has_wifi_for_known_non_wifi_board() -> None:
    """Plain ``rpipico`` has no CYW43 → False."""
    from esphome.components import rp2

    assert rp2.board_id_has_wifi("rpipico") is False


def test_board_id_has_wifi_for_rp2350_w_variant() -> None:
    """``rpipico2w`` is the RP2350 Pico 2 W → True."""
    from esphome.components import rp2

    assert rp2.board_id_has_wifi("rpipico2w") is True


def test_board_id_has_wifi_for_unknown_board_returns_true() -> None:
    """Unknown ids fail open so a custom board is not rejected.

    The validator falls back to ESPHome's compile-time check; the
    helper returning True here means the wizard emits a ``wifi:``
    block and any genuinely-unsupported config trips the existing
    "no CYW43" guard at compile time.
    """
    from esphome.components import rp2

    assert rp2.board_id_has_wifi("not-a-real-board-id") is True


def test_rp2_declares_rp2040_as_alias() -> None:
    """The framework-level deprecation hook is on the ``rp2`` component.

    The legacy ``rp2040:`` YAML key works because the rp2 component
    opts in via ``ALIASES``; without this declaration the rename
    framework wouldn't route legacy configs.
    """
    from esphome.components import rp2

    assert "rp2040" in rp2.ALIASES
    assert rp2.ALIAS_REMOVAL_VERSION == "2027.7.0"


def test_rp2040_python_import_resolves_to_rp2() -> None:
    """``from esphome.components import rp2040`` must work for external
    custom components and external tooling (device-builder, the dashboard
    wizard, etc.) that still import from the legacy module path.

    The ``_AliasFinder`` on ``sys.meta_path`` rewrites the lookup to
    the canonical module — both should be the same object.
    """
    from esphome.components import (
        rp2,
        rp2040,  # routed via _AliasFinder
    )

    assert rp2040 is rp2


def test_rp2040_submodule_imports_resolve_to_rp2_submodules() -> None:
    """Submodule imports (e.g. ``esphome.components.rp2040.boards``) must
    also route to the canonical equivalents — the board-generator script
    and the dashboard wizard both rely on this path.
    """
    from esphome.components.rp2 import (
        boards as rp2_boards,
        generate_boards as rp2_generate,
    )
    from esphome.components.rp2040 import (
        boards as rp2040_boards,
        generate_boards as rp2040_generate,
    )

    assert rp2040_boards is rp2_boards
    assert rp2040_generate is rp2_generate


def test_lwip_segment_pool_exceeds_per_pcb_queue() -> None:
    """The segment pool is global while the send queue is per-PCB.

    lwIP's sanity check only requires ``MEMP_NUM_TCP_SEG >= TCP_SND_QUEUELEN``,
    which is the floor for a *single* connection: at equality one busy PCB can
    drain the pool for every other PCB. Dropping back to that floor would
    rebuild the starvation this sizing exists to prevent, and nothing in the
    build would complain.
    """
    from esphome.components import rp2

    assert rp2.LWIP_MEMP_NUM_TCP_SEG >= 2 * rp2.LWIP_TCP_SND_QUEUELEN


def test_lwip_mem_size_keeps_mem_size_t_narrow() -> None:
    """``lwip/mem.h`` widens ``mem_size_t`` to ``u32_t`` on
    ``MEM_SIZE > 64000L``, growing the header on every heap block. Raising the
    heap past that bound is a real option, but it should be a deliberate one
    rather than a side effect of tuning.
    """
    from esphome.components import rp2

    assert rp2.LWIP_MEM_SIZE <= 64000


def test_lwip_defines_carry_the_sizing_into_the_header() -> None:
    """The constants above only matter if they reach the generated header.

    ``build_lwip_defines()`` is what feeds lwipopts.h.jinja, so assert on it
    rather than on the constants alone: dropping a key here would silently
    fall back to arduino-pico's own value while every other assertion in this
    file stayed green.
    """
    from esphome.components import rp2

    defines = rp2.build_lwip_defines(tcp_sockets=8, udp_sockets=6, listening_tcp=2)

    assert defines["MEM_SIZE"] == str(rp2.LWIP_MEM_SIZE)
    assert defines["MEMP_NUM_TCP_SEG"] == str(rp2.LWIP_MEMP_NUM_TCP_SEG)
    assert defines["TCP_SND_QUEUELEN"] == str(rp2.LWIP_TCP_SND_QUEUELEN)
    # Socket-derived counts pass through untouched.
    assert defines["MEMP_NUM_TCP_PCB"] == "8"
    assert defines["MEMP_NUM_UDP_PCB"] == "6"
    assert defines["MEMP_NUM_TCP_PCB_LISTEN"] == "2"


def test_lwipopts_template_placeholders_are_all_supplied() -> None:
    """Every ``{{ NAME }}`` in the template must have a value.

    A placeholder with no matching key renders empty, which turns into a bare
    ``#define FOO`` that compiles and quietly means something else.
    """
    import re

    from esphome.components import rp2

    template = (Path(rp2.__file__).parent / "lwipopts.h.jinja").read_text(
        encoding="utf-8"
    )
    placeholders = set(re.findall(r"{{\s*(\w+)\s*}}", template))
    supplied = set(
        rp2.build_lwip_defines(tcp_sockets=8, udp_sockets=6, listening_tcp=2)
    )

    assert placeholders <= supplied, f"unsupplied: {placeholders - supplied}"
