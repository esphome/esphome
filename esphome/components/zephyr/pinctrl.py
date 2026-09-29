"""I2C/UART/SPI pinctrl overlay generation for Zephyr.

Owns the whole family-specific macro/group-role resolution so uart/i2c/spi's
own to_code() only ever deal in real pin numbers, not devicetree overlay
text. Per-family signal decoding and macro building lives in variants/*_family.py;
this module is the generic dispatcher and DTS-merge machinery shared by all
of them."""

from collections.abc import Callable
import logging

import esphome.config_validation as cv
from esphome.core import CORE, EsphomeError

from .const import ZEPHYR_VARIANT_ESP32, ZEPHYR_VARIANT_NATIVE_SIM
from .variants import VARIANTS

_LOGGER = logging.getLogger(__name__)


def _resolve_i2c_pinctrl_states(
    board: str,
    bus_label: str,
    fallback_group: str,
    fallback_state_suffixes: tuple[str, ...] = ("default",),
) -> list[tuple[str, str]]:
    """Return [(pinctrl_label, group_name), ...] for every pinctrl state (e.g.
    "default" and "sleep") to merge SDA+SCL into. Falls back to guessing
    `{bus_label}_{suffix}`/`fallback_group` per `fallback_state_suffixes` (with
    a warning) when DTS resolves nothing, and to `fallback_group` for any
    resolved state with more than one group (the shape UART's TX/RX split
    hit)."""
    from .dts_lookup import get_pinctrl_states

    states = get_pinctrl_states(board, bus_label)
    if states is None:
        _LOGGER.warning(
            "Could not resolve board '%s''s real pinctrl node for '%s' from "
            "devicetree -- assuming '%s_%s'/'%s' for each state. If that's "
            "wrong, this will fail at devicetree-compile time.",
            board,
            bus_label,
            bus_label,
            "/".join(fallback_state_suffixes),
            fallback_group,
        )
        return [
            (f"{bus_label}_{suffix}", fallback_group)
            for suffix in fallback_state_suffixes
        ]

    resolved: list[tuple[str, str]] = []
    for label, groups in states:
        if len(groups) == 1:
            resolved.append((label, groups[0]))
            continue
        _LOGGER.warning(
            "Board '%s''s pinctrl node '%s' has %d groups (%s), not the single "
            "shared group SDA/SCL expect -- assuming '%s'. If that's wrong, "
            "this will fail at devicetree-compile time.",
            board,
            label,
            len(groups),
            ", ".join(groups),
            fallback_group,
        )
        resolved.append((label, fallback_group))
    return resolved


def _build_i2c_pinctrl_states_overlay(
    board: str,
    states: list[tuple[str, str]],
    property_name: str,
    role_values: dict[str, str],
    value_role_decoder: Callable[[int], str | None] | None,
    pinctrl_label: str = "pinctrl",
) -> str:
    """Build a `&<pinctrl_label> { <label> { <group> { <property_name> =
    <merged>; }; }; ... };` overlay for every (label, group) in `states`.
    pinctrl_label is the variant's own pinctrl controller node label (see
    ZephyrVariant.pinctrl_node_label). `role_values` only has
    entries for the signal(s) actually being remapped -- when only one of
    sda/scl is given, `value_role_decoder` reads the group's real existing
    values and restates whichever one isn't touched (as its raw integer), since
    an overlay setting a property again replaces it wholesale rather than
    merging. None decoder = no way to decode, falls back to just the given
    values."""
    from .dts_lookup import get_pinctrl_group_property

    def _merged_value(label: str, group: str) -> str:
        if value_role_decoder is None:
            return ", ".join(role_values.values())
        existing = get_pinctrl_group_property(board, label, group, property_name) or []
        parts = []
        seen_roles: set[str] = set()
        for raw in existing:
            role = value_role_decoder(raw)
            if role in role_values:
                parts.append(role_values[role])
                seen_roles.add(role)
            else:
                parts.append(f"<{raw}>")
        for role, value in role_values.items():
            if role not in seen_roles:
                parts.append(value)
        return ", ".join(parts)

    blocks = "\n".join(
        f"""
                {label} {{
                    {group} {{
                        {property_name} = {_merged_value(label, group)};
                    }};
                }};
        """
        for label, group in states
    )
    return f"""
        &{pinctrl_label} {{
            {blocks}
        }};
    """


def _positional_uart_group_roles(
    board: str, label: str, groups: list[str]
) -> dict[str, str] | None:
    """Default group-role guess: group[0]=TX, group[1]=RX by position. Not
    universally safe -- nrf54lm20dk's uart20 swaps group order between its
    "default" and "sleep" states -- used only where no better signal exists."""
    if len(groups) != 2:
        return None
    return {"tx": groups[0], "rx": groups[1]}


def _resolve_uart_pinctrl_states(
    board: str,
    port_label: str,
    tx_value: str | None,
    rx_value: str | None,
    group_role_resolver=None,
    value_role_decoder: Callable[[int], str | None] | None = None,
    property_name: str = "pinmux",
) -> list[tuple[str, list[tuple[str, str]]]]:
    """Return [(pinctrl_label, [(group_name, value), ...]), ...] for every
    pinctrl state on port_label, deciding per state whether TX/RX share one
    group or need more (some boards split TX/RX/RTS/CTS into 4 separate
    groups, e.g. dptechnics/walter; nRF52 combines TX/RX into one group for
    "sleep" but splits them for "default").

    `group_role_resolver(board, label, groups)` returns {"tx": group, "rx":
    group, ...} for a state's groups (any count > 1) -- None (the default)
    only ever succeeds for exactly 2 groups (a position-based guess), but a
    family with a real way to tell (e.g. esp32's pinmux encoding) can pass a
    resolver that reads the actual group content instead, and isn't limited
    to 2 groups. Falls back to the positional guess (with a warning) when DTS
    resolves nothing, or a state's "tx"/"rx" roles can't both be determined.

    `value_role_decoder(value)` decodes a single real property value (e.g. one
    entry of a group's `pinmux` array) back to a role name, or None if the
    family has no way to (positional-only families). Needed because a
    devicetree overlay setting a property again *replaces* it wholesale --
    when TX and RX (or TX and RTS, etc.) share one real group and only one of
    them is being remapped, every other value already in that group must be
    restated (as its raw resolved integer, since the friendly macro name isn't
    recoverable from a bare int) or it's silently dropped from the overlay.
    """
    from .dts_lookup import get_pinctrl_group_property, get_pinctrl_states

    group_role_resolver = group_role_resolver or _positional_uart_group_roles

    new_values: dict[str, str] = {}
    if tx_value is not None:
        new_values["tx"] = tx_value
    if rx_value is not None:
        new_values["rx"] = rx_value

    def _merge_group(label: str, group: str, role_values: dict[str, str]) -> str:
        if value_role_decoder is None:
            return ", ".join(role_values.values())
        existing = get_pinctrl_group_property(board, label, group, property_name) or []
        parts = []
        seen_roles: set[str] = set()
        for raw in existing:
            role = value_role_decoder(raw)
            if role in role_values:
                parts.append(role_values[role])
                seen_roles.add(role)
            else:
                parts.append(f"<{raw}>")
        for role, value in role_values.items():
            if role not in seen_roles:
                parts.append(value)
        return ", ".join(parts)

    def _values_for_role_groups(
        label: str, role_to_group: dict[str, str]
    ) -> list[tuple[str, str]]:
        by_group: dict[str, dict[str, str]] = {}
        for role, value in new_values.items():
            group = role_to_group.get(role)
            if group is not None:
                by_group.setdefault(group, {})[role] = value
        return [
            (group, _merge_group(label, group, role_values))
            for group, role_values in by_group.items()
        ]

    states = get_pinctrl_states(board, port_label)
    if states is None:
        label = f"{port_label}_default"
        _LOGGER.warning(
            "Could not resolve board '%s''s real pinctrl node for '%s' from "
            "devicetree -- assuming '%s' with a TX/RX group1/group2 split. If "
            "that's wrong, this will fail at devicetree-compile time.",
            board,
            port_label,
            label,
        )
        return [
            (label, _values_for_role_groups(label, {"tx": "group1", "rx": "group2"}))
        ]

    resolved: list[tuple[str, list[tuple[str, str]]]] = []
    for label, groups in states:
        if len(groups) == 1:
            resolved.append(
                (
                    label,
                    _values_for_role_groups(
                        label, dict.fromkeys(new_values, groups[0])
                    ),
                )
            )
            continue
        roles = group_role_resolver(board, label, groups)
        if roles is not None:
            resolved.append((label, _values_for_role_groups(label, roles)))
            continue
        _LOGGER.warning(
            "Could not determine TX/RX group roles for board '%s''s pinctrl "
            "node '%s' (groups: %s) -- assuming a group1/group2 split. If "
            "that's wrong, this will fail at devicetree-compile time.",
            board,
            label,
            ", ".join(groups),
        )
        resolved.append(
            (label, _values_for_role_groups(label, {"tx": "group1", "rx": "group2"}))
        )
    return resolved


def _build_uart_pinctrl_states_overlay(
    states: list[tuple[str, list[tuple[str, str]]]],
    property_name: str,
    pinctrl_label: str = "pinctrl",
) -> str:
    """Build the `&<pinctrl_label> { ... }` overlay from
    _resolve_uart_pinctrl_states()'s output. The `<label>:` prefix
    (re-)establishes the phandle even for a state the board never pinctrl'd
    itself -- without it, `&<label>` in the bus-enable overlay resolves to
    nothing and fails at DTS-compile time. pinctrl_label is the variant's own
    pinctrl controller node label (see ZephyrVariant.pinctrl_node_label)."""
    state_blocks = []
    for label, group_values in states:
        if not group_values:
            continue
        group_blocks = "\n".join(
            f"""
                    {group} {{
                        {property_name} = {value};
                    }};
            """
            for group, value in group_values
        )
        state_blocks.append(
            f"""
                {label}: {label} {{
                    {group_blocks}
                }};
            """
        )
    return f"""
        &{pinctrl_label} {{
            {"".join(state_blocks)}
        }};
    """


def zephyr_setup_uart_pinctrl(
    board: str,
    port_label: str,
    tx_pin: int | None,
    rx_pin: int | None,
    baud_rate: int,
    node_known: bool = True,
) -> None:
    """Add the TX/RX pinctrl overlay (if either pin is given) and the bus-enable
    overlay for `port_label`, mirroring zephyr_setup_i2c_pinctrl()/
    zephyr_setup_spi_pinctrl()'s shape. node_known=False skips the bus-enable
    override -- it'd forward-reference an undeclared label."""
    from . import zephyr_add_overlay, zephyr_variant, zephyr_variant_family  # noqa: PLC0415 -- avoids circular import at module load

    if tx_pin is None and rx_pin is None:
        if not node_known:
            return
        from .dts_lookup import has_pinctrl_configured

        if not has_pinctrl_configured(board, port_label):
            _LOGGER.warning(
                "Board '%s' has no pinctrl configured for port '%s' -- "
                "assuming you've configured it yourself via `zephyr: "
                "overlays:`. If not, this will fail at devicetree-compile "
                "time.",
                board,
                port_label,
            )
        zephyr_add_overlay(f'&{port_label} {{ status = "okay"; }};')
        return

    if not node_known:
        _LOGGER.warning(
            "'%s' is not a devicetree node ESPHome found on board '%s' -- "
            "tx_pin:/rx_pin: can't be applied to it this way. Configure pins "
            "directly in your own `zephyr: overlays:` instead.",
            port_label,
            board,
        )
        return

    prefix = port_label.upper()
    family = zephyr_variant_family()
    if family == "nordic":
        from .variants import nordic_family as family_module

        prefix = "UART"
    elif family == "silabs":
        from .variants import silabs_family as family_module
    elif family == "esp32":
        from .variants import esp32_family as family_module
    elif family == "rpi_pico":
        from .variants import rpi_pico_family as family_module
    else:
        # No overlay-generation branch for this family -- don't silently ignore the
        # pins. Every family with uart_valid_pins/uart_valid_pins_by_instance set
        # (the only way validate_zephyr_config lets tx_pin/rx_pin through) is
        # already handled above, so this should be unreachable in practice.
        raise EsphomeError(
            f"tx_pin:/rx_pin: are not supported for UART on this Zephyr variant "
            f"('{zephyr_variant()}') -- remove them to use the board's default UART pins."
        )

    tx_value = family_module.pin_macro(prefix, "TX", tx_pin, zephyr_variant())
    rx_value = family_module.pin_macro(prefix, "RX", rx_pin, zephyr_variant())
    resolvers = family_module.uart_pinctrl(board, port_label)
    property_name = family_module.PROPERTY_NAME
    group_role_resolver, value_role_decoder = (
        resolvers if resolvers is not None else (None, None)
    )
    states = _resolve_uart_pinctrl_states(
        board,
        port_label,
        tx_value,
        rx_value,
        group_role_resolver=group_role_resolver,
        value_role_decoder=value_role_decoder,
        property_name=property_name,
    )
    zephyr_add_overlay(
        _build_uart_pinctrl_states_overlay(
            states, property_name, VARIANTS[zephyr_variant()].pinctrl_node_label
        )
    )
    # A board whose stock node already declares >1 pinctrl state (e.g. "sleep" for
    # PM) needs every pinctrl-<N> restated to match, or the now-uncovered old state
    # is a pinctrl-names count mismatch at DTS-compile time.
    from .dts_lookup import get_pinctrl_state_names

    state_names = get_pinctrl_state_names(board, port_label)
    if state_names is None or len(state_names) != len(states):
        state_names = ["default"]
        states = states[:1]
    pinctrl_props = " ".join(
        f"pinctrl-{i} = <&{label}>;" for i, (label, _) in enumerate(states)
    )
    names_prop = ", ".join(f'"{name}"' for name in state_names)
    # current-speed must exist in DT for the driver's init macro regardless of
    # value; the real baud rate is set at runtime by uart_configure().
    zephyr_add_overlay(
        f'&{port_label} {{ status = "okay"; '
        f"current-speed = <{baud_rate}>; "
        f"{pinctrl_props} pinctrl-names = {names_prop}; }};"
    )


def zephyr_setup_i2c_pinctrl(
    board: str, bus_label: str, sda: int | None, scl: int | None
) -> tuple[str, str]:
    """Resolve I2C pin assignments and add the variant-specific pinctrl overlay,
    returning (sda, scl) as dump_config display strings -- "GPIO{n}" for
    whichever pin was actually given, "board default" for whichever wasn't
    (independently, not all-or-nothing: giving only one remaps just that
    signal, leaving the other at the board's own existing wiring)."""
    from . import zephyr_add_overlay, zephyr_data, zephyr_variant, zephyr_variant_family  # noqa: PLC0415 -- avoids circular import at module load

    variant_name = zephyr_data().get("variant") or ""
    sda_display = f"GPIO{sda}" if sda is not None else "board default"
    scl_display = f"GPIO{scl}" if scl is not None else "board default"

    if sda is None and scl is None:
        # Bus is already enabled unconditionally by the caller -- board's own
        # pre-wired pinctrl default (if any) is left untouched.
        return sda_display, scl_display

    if variant_name == ZEPHYR_VARIANT_NATIVE_SIM:
        # No pinctrl node -- the emulated controller has no physical pins.
        zephyr_add_overlay(f'&{bus_label} {{ status = "okay"; }};')
        return sda_display, scl_display

    if CORE.is_nrf52:
        # This platform never has DTS access to merge with, so both pins are needed
        # together -- there's no "board default" to leave the other one at.
        if sda is None or scl is None:
            raise EsphomeError(
                "sda: and scl: must be given together for I2C on platform: nrf52 -- "
                "there's no board-default pinctrl to fall back to for the one you omitted."
            )
        zephyr_add_overlay(
            f"""
                &pinctrl {{
                    {bus_label}_default {{
                        group1 {{
                            psels = <NRF_PSEL(TWIM_SDA, {sda // 32}, {sda % 32})>,
                                <NRF_PSEL(TWIM_SCL, {scl // 32}, {scl % 32})>;
                        }};
                    }};
                    {bus_label}_sleep {{
                        group1 {{
                            psels = <NRF_PSEL(TWIM_SDA, {sda // 32}, {sda % 32})>,
                                <NRF_PSEL(TWIM_SCL, {scl // 32}, {scl % 32})>;
                        }};
                    }};
                }};
            """
        )
        return sda_display, scl_display

    family = zephyr_variant_family()
    role_values: dict[str, str] = {}
    prefix = bus_label.upper()
    if family == "nordic":
        from .variants import nordic_family as family_module

        # Same peripheral as platform: nrf52 above, but resolved via DTS first
        # like esp32/silabs, in case a board ever does pre-wire it.
        prefix = "TWIM"
    elif family == "esp32":
        from .variants import esp32_family as family_module

        if zephyr_variant() == ZEPHYR_VARIANT_ESP32 and (sda is None) != (scl is None):
            # Original ESP32's software bus-clear needs a real sda-gpios/scl-gpios
            # pair below -- it can't express "leave it as-is" as a GPIO phandle.
            raise EsphomeError(
                "sda: and scl: must be given together for I2C on original ESP32 -- "
                "its software bus-clear workaround needs both real pin numbers."
            )
    elif family == "silabs":
        # Silabs macros are lettered-port form ({BUS}_{SIGNAL}_P{port}{n}, e.g.
        # I2C0_SDA_PC5), not ESP32's flat GPIO{n}.
        from .variants import silabs_family as family_module
    elif family == "rpi_pico":
        from .variants import rpi_pico_family as family_module

        # Each pin is tied to one fixed I2C instance+role -- both are needed
        # together to even know which instance is being targeted.
        if sda is None or scl is None:
            raise EsphomeError(
                f"sda: and scl: must be given together for I2C on '{bus_label}' "
                f"({variant_name}) -- its GPIO mux ties each pin to a fixed "
                "instance+role pair."
            )
        instance_pins = VARIANTS[variant_name].i2c_valid_pins_by_instance.get(
            bus_label.upper()
        )
        if (
            instance_pins is None
            or sda not in instance_pins.get("sda", frozenset())
            or scl not in instance_pins.get("scl", frozenset())
        ):
            raise EsphomeError(
                f"GPIO{sda}/GPIO{scl} are not a valid sda:/scl: pair for '{bus_label}' "
                f"on {variant_name}."
            )
    else:
        # No overlay-generation branch for this family -- don't silently ignore the pins.
        raise EsphomeError(
            f"sda:/scl: are not supported for I2C on this Zephyr variant "
            f"('{variant_name}') -- remove them to use the board's default I2C pins."
        )

    if sda is not None:
        role_values["sda"] = family_module.pin_macro(prefix, "SDA", sda, variant_name)
    if scl is not None:
        role_values["scl"] = family_module.pin_macro(prefix, "SCL", scl, variant_name)
    value_role_decoder = family_module.i2c_value_role_decoder(bus_label)
    property_name = family_module.PROPERTY_NAME
    fallback_group = family_module.I2C_FALLBACK_GROUP
    fallback_state_suffixes = family_module.I2C_FALLBACK_STATE_SUFFIXES

    states = _resolve_i2c_pinctrl_states(
        board,
        bus_label,
        fallback_group,
        fallback_state_suffixes=fallback_state_suffixes,
    )
    pinctrl_overlay = _build_i2c_pinctrl_states_overlay(
        board,
        states,
        property_name,
        role_values,
        value_role_decoder,
        VARIANTS[variant_name].pinctrl_node_label,
    )
    if family == "esp32" and zephyr_variant() == ZEPHYR_VARIANT_ESP32:
        # sda/scl are both real ints here (raised above otherwise).
        sda_ctlr, sda_pin = ("gpio1", sda - 32) if sda >= 32 else ("gpio0", sda)
        scl_ctlr, scl_pin = ("gpio1", scl - 32) if scl >= 32 else ("gpio0", scl)
        pinctrl_overlay += f"""
            &{bus_label} {{
                sda-gpios = <&{sda_ctlr} {sda_pin} GPIO_OPEN_DRAIN>;
                scl-gpios = <&{scl_ctlr} {scl_pin} GPIO_OPEN_DRAIN>;
            }};
        """
    zephyr_add_overlay(pinctrl_overlay)

    return sda_display, scl_display


def _resolve_spi_pinctrl_states(
    board: str,
    bus_label: str,
    values: dict[str, str],
    group_role_resolver: Callable[[str, str, list[str]], dict[str, str] | None] | None,
    value_role_decoder: Callable[[int], str | None] | None = None,
    property_name: str = "pinmux",
) -> list[tuple[str, list[tuple[str, str]]]]:
    """Return [(pinctrl_label, [(group, combined_value), ...]), ...], merging
    `values` (signal -> pinmux macro text) into whichever real group each signal
    already belongs to per group_role_resolver's content decode -- kept separate
    from _resolve_uart_pinctrl_states() since SPI has no meaningful 2-group
    positional fallback the way UART's tx/rx split does. Falls back to a single
    shared 'group1' (with a warning) when DTS resolves nothing or a signal can't
    be placed.

    `value_role_decoder` reads a group's real existing values and restates
    whatever signal isn't in `values` (as its raw resolved integer) -- a group
    can hold more signals than the ones being remapped (e.g. clk+mosi+miso all
    sharing one group while only clk/mosi are given), and an overlay setting a
    property again replaces it wholesale rather than merging."""
    from .dts_lookup import get_pinctrl_group_property, get_pinctrl_states

    def _merge_group(label: str, group: str, group_values: dict[str, str]) -> str:
        if value_role_decoder is None:
            return ", ".join(group_values.values())
        existing = get_pinctrl_group_property(board, label, group, property_name) or []
        parts = []
        seen_roles: set[str] = set()
        for raw in existing:
            role = value_role_decoder(raw)
            if role in group_values:
                parts.append(group_values[role])
                seen_roles.add(role)
            else:
                parts.append(f"<{raw}>")
        for role, value in group_values.items():
            if role not in seen_roles:
                parts.append(value)
        return ", ".join(parts)

    def _single_group(label: str, group: str) -> list[tuple[str, str]]:
        return [(group, _merge_group(label, group, values))]

    states = get_pinctrl_states(board, bus_label)
    if states is None:
        label = f"{bus_label}_default"
        _LOGGER.warning(
            "Could not resolve board '%s''s real pinctrl node for '%s' from "
            "devicetree -- assuming '%s' with every signal in one shared group. "
            "If that's wrong, this will fail at devicetree-compile time.",
            board,
            bus_label,
            label,
        )
        return [(label, _single_group(label, "group1"))]

    resolved: list[tuple[str, list[tuple[str, str]]]] = []
    for label, groups in states:
        if len(groups) == 1:
            resolved.append((label, _single_group(label, groups[0])))
            continue
        roles = (
            group_role_resolver(board, label, groups)
            if group_role_resolver is not None
            else None
        )
        if roles is not None and all(signal in roles for signal in values):
            by_group: dict[str, dict[str, str]] = {}
            for signal, value in values.items():
                by_group.setdefault(roles[signal], {})[signal] = value
            resolved.append(
                (
                    label,
                    [
                        (group, _merge_group(label, group, group_values))
                        for group, group_values in by_group.items()
                    ],
                )
            )
            continue
        _LOGGER.warning(
            "Could not determine SPI signal group roles for board '%s''s pinctrl "
            "node '%s' (groups: %s) -- assuming every signal shares 'group1'. If "
            "that's wrong, this will fail at devicetree-compile time.",
            board,
            label,
            ", ".join(groups),
        )
        resolved.append((label, _single_group(label, "group1")))
    return resolved


def _build_spi_pinctrl_states_overlay(
    states: list[tuple[str, list[tuple[str, str]]]],
    property_name: str,
    pinctrl_label: str = "pinctrl",
) -> str:
    """Build the `&<pinctrl_label> { ... }` overlay from
    _resolve_spi_pinctrl_states()'s output. The `<label>:` prefix
    (re-)establishes the phandle even for a state the board never pinctrl'd
    itself -- without it, `&<label>` in the bus-enable overlay resolves to
    nothing at DTS-compile time. pinctrl_label is the variant's own pinctrl
    controller node label (see ZephyrVariant.pinctrl_node_label) -- almost
    always "pinctrl", but not universal (e.g. SiWx91x's "pinctrl0")."""
    state_blocks = []
    for label, group_values in states:
        if not group_values:
            continue
        group_blocks = "\n".join(
            f"""
                    {group} {{
                        {property_name} = {value};
                    }};
            """
            for group, value in group_values
        )
        state_blocks.append(
            f"""
                {label}: {label} {{
                    {group_blocks}
                }};
            """
        )
    return f"""
        &{pinctrl_label} {{
            {"".join(state_blocks)}
        }};
    """


def zephyr_setup_spi_pinctrl(
    board: str,
    bus_label: str,
    clk: int | None = None,
    miso: int | None = None,
    mosi: int | None = None,
    data_pins: list[int] | None = None,
) -> None:
    """Enable the hardware SPI bus node for `bus_label`. esp32/nordic/silabs are
    free-mux, so clk/miso/mosi/data_pins generate a pinctrl overlay merged into
    the board's real pinctrl group(s) resolved from DTS (mirrors
    zephyr_setup_uart_pinctrl()). Every other family (stm32, renesas, rp2040, ...)
    has no generated overlay -- the board's own pinctrl (if any) is trusted as-is,
    same fallback has_pinctrl_configured() warns about below when it's missing.
    A None pin leaves that signal at the board's default pinctrl entry instead
    of remapping it."""
    from . import (  # noqa: PLC0415 -- avoids circular import at module load
        zephyr_add_overlay,
        zephyr_add_prj_conf,
        zephyr_variant,
        zephyr_variant_family,
    )

    family = zephyr_variant_family()

    from .dts_lookup import has_pinctrl_configured

    if not has_pinctrl_configured(board, bus_label):
        # A board with zero pinctrl here has never chosen this bus for SPI at all --
        # e.g. silabs' spi-controller.yaml requires #address-cells/#size-cells plus
        # real cs-gpios wiring, which every board that actually supports SPI on a
        # USART/EUSART sets itself (verified against every board in-tree that does).
        # We don't fabricate that ourselves -- add it via `zephyr: overlays:`, or use
        # a board that already wires this bus for SPI.
        _LOGGER.warning(
            "Board '%s' has no pinctrl configured for SPI bus '%s' -- assuming "
            "you've configured it yourself via `zephyr: overlays:`. If not, this "
            "will fail at devicetree-compile time.",
            board,
            bus_label,
        )

    if family not in ("esp32", "nordic", "silabs", "silabs_siwx91x"):
        # stm32, renesas, rp2040, and any other family: no generated pinctrl overlay
        # -- clk/miso/mosi are irrelevant here, whatever the board (or the user's own
        # zephyr: overlays:) already wires is trusted as-is.
        zephyr_add_overlay(f'&{bus_label} {{ status = "okay"; }};')
        return

    if family == "nordic":
        from .variants import nordic_family as family_module

        prefix = "SPIM"
        clk_signal, miso_signal, mosi_signal = "SCK", "MISO", "MOSI"
    elif family == "silabs":
        from .variants import silabs_family as family_module

        prefix = bus_label.upper()
        # EUSART's clock signal macro is SCLK, not CLK like classic USART (verified
        # xg24-pinctrl.h) -- TX/RX names and en_bit values are otherwise identical.
        clk_signal = "SCLK" if bus_label.startswith("eusart") else "CLK"
        miso_signal, mosi_signal = "RX", "TX"
    elif family == "silabs_siwx91x":
        from .variants import silabs_siwx91x_family as family_module

        # No per-pin macro formula (unlike EFR32/Nordic/ESP32 above/below) --
        # family_module.pin_macro() looks up spi_pin_macros by these generic
        # signal keys directly, so prefix is unused here.
        prefix = None
        clk_signal, miso_signal, mosi_signal = "clk", "miso", "mosi"
    else:
        from .variants import esp32_family as family_module

        valid_instances = family_module.SPI_INSTANCE_SIGNAL_PREFIX.get(
            zephyr_variant(), {}
        )
        instance = (
            int(bus_label.removeprefix("spi")) if bus_label[3:].isdigit() else None
        )
        if instance not in valid_instances:
            listed = ", ".join(f"spi{n}" for n in sorted(valid_instances))
            raise cv.Invalid(
                f"'{bus_label}' is not a valid SPI bus for Zephyr variant "
                f"{zephyr_variant()!r} -- valid options: {listed}"
            )

        prefix = f"SPIM{instance}"
        if data_pins:
            # Quad mode routes mosi/miso through data_pins[0]/[1] (D0/D1) instead of
            # separate mosi_pin/miso_pin -- schema forbids the latter for quad.
            mosi, miso = data_pins[0], data_pins[1]
        clk_signal, miso_signal, mosi_signal = "SCLK", "MISO", "MOSI"

    values: dict[str, str] = {}
    if clk is not None:
        values["clk"] = family_module.pin_macro(
            prefix, clk_signal, clk, zephyr_variant()
        )
    if miso is not None:
        values["miso"] = family_module.pin_macro(
            prefix, miso_signal, miso, zephyr_variant()
        )
    if mosi is not None:
        values["mosi"] = family_module.pin_macro(
            prefix, mosi_signal, mosi, zephyr_variant()
        )
    if family == "esp32" and data_pins:
        # data_pins[2]/[3] are WP/HD, same order as ESP-IDF's
        # data0_io_num..data3_io_num (spi_esp_idf.cpp).
        instance_prefix = valid_instances[instance]
        wp, hd = data_pins[2], data_pins[3]
        values["wp"] = family_module.spi_quad_macro(wp, instance_prefix, "WP")
        values["hd"] = family_module.spi_quad_macro(hd, instance_prefix, "HD")
    resolvers = family_module.spi_pinctrl(board, bus_label)
    property_name = family_module.PROPERTY_NAME

    group_role_resolver, value_role_decoder = (
        resolvers if resolvers is not None else (None, None)
    )
    states = _resolve_spi_pinctrl_states(
        board,
        bus_label,
        values,
        group_role_resolver,
        value_role_decoder=value_role_decoder,
        property_name=property_name,
    )
    pinctrl_label = VARIANTS[zephyr_variant()].pinctrl_node_label
    zephyr_add_overlay(
        _build_spi_pinctrl_states_overlay(states, property_name, pinctrl_label)
    )

    # A board whose stock node already declares >1 pinctrl state (e.g. "sleep" for
    # PM) needs every pinctrl-<N> restated to match, or the now-uncovered old state
    # is a pinctrl-names count mismatch at DTS-compile time.
    from .dts_lookup import get_pinctrl_state_names

    state_names = get_pinctrl_state_names(board, bus_label)
    if state_names is None or len(state_names) != len(states):
        state_names = ["default"]
        states = states[:1]
    pinctrl_props = " ".join(
        f"pinctrl-{i} = <&{label}>;" for i, (label, _) in enumerate(states)
    )
    names_prop = ", ".join(f'"{name}"' for name in state_names)
    zephyr_add_overlay(
        f'&{bus_label} {{ status = "okay"; '
        f"{pinctrl_props} pinctrl-names = {names_prop}; }};"
    )
    if data_pins:
        zephyr_add_prj_conf("SPI_EXTENDED_MODES", True)
