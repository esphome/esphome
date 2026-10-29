"""Tests for the pcm5122 power switches."""

from collections.abc import Callable


def test_power_mode_picks_the_setter(generate_main: Callable[[str], str]) -> None:
    """Each power_mode calls its own setter and publishes only when the DAC accepts it."""
    main_cpp = generate_main("tests/component_tests/pcm5122/test_pcm5122_switch.yaml")

    for switch_id, setter in (
        ("sw_standby", "set_standby"),
        ("sw_powerdown", "set_powerdown"),
    ):
        body = main_cpp.split(
            f"esphome__{switch_id}__fn(switch_::Switch * entity, bool state) {{", 1
        )[1]
        body = body.split("\n}", 1)[0]
        assert (
            f"if (::pcm5122_dac->{setter}(state)) {{ entity->publish_state(state); }}"
            in body
        )
