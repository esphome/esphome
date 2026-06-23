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
