"""Code generation of scripts.

Objects are found from the script ids and the links between them, not from auto-numbered
ids, so adding or changing one script does not break the others.
"""

import re

import pytest

from tests.component_tests.helpers import constructed, function_body

CONFIG = "tests/component_tests/script/test_script_codegen.yaml"


def _automation(main_cpp: str, trigger: str) -> str:
    match = re.search(rf"\bnew\((\w+)\) Automation<.*?>\({trigger}\);", main_cpp)
    assert match is not None, trigger
    return match.group(1)


def _actions_of(main_cpp: str, automation: str) -> list[str]:
    return re.findall(rf"\b{automation}->add_action\((\w+)\);", main_cpp)


def _template_tail(types: tuple[str, ...]) -> str:
    return "".join(f", {type_}" for type_ in types)


def _mode_class(mode: str, *types: str) -> str:
    """The mode class shared by scripts that run a function, with the same parameters."""
    return f"script::{mode}<script::FunctionBody<{', '.join(types)}>{_template_tail(types)}>"


def _function_script(mode: str, script: str, *types: str) -> str:
    """The class of a script whose actions run as one function."""
    return (
        f"script::FunctionScript<{_mode_class(mode, *types)}, "
        f"esphome__{script}__fn{_template_tail(types)}>"
    )


def _assert_function_script(main_cpp: str, mode: str, script: str, *types: str) -> None:
    assert (
        constructed(main_cpp, script) == f"{_function_script(mode, script, *types)}()"
    )


def _script_body(main_cpp: str, script: str) -> str:
    """The function a script runs; it has no Automation."""
    assert not re.search(rf"\bAutomation<.*?>\({script}\);", main_cpp)
    return function_body(main_cpp, f"esphome__{script}__fn")


def test_inline_actions_run_as_one_function(generate_main) -> None:
    """A script of inline actions calls one function, declared before the script."""
    main_cpp = generate_main(CONFIG)
    class_ = _function_script("SingleMode", "inline_single")
    assert constructed(main_cpp, "inline_single") == f"{class_}()"
    declaration = main_cpp.index(
        "static void esphome__inline_single__fn(const int & esphome__running);"
    )
    assert declaration < main_cpp.index(f"alignas({class_})")
    assert f"static {class_} *const inline_single" in main_cpp
    body = _script_body(main_cpp, "inline_single")
    assert '"INLINE first"' in body
    assert "counter->value() += 1;" in body
    # A script can be stopped from inside, so the rest of the run checks for it
    assert "if (!esphome__running)\n    return;" in body


def test_one_lambda_action(generate_main) -> None:
    """A single inline action is the whole function."""
    main_cpp = generate_main(CONFIG)
    body = _script_body(main_cpp, "one_action")
    assert '"ONE action"' in body
    assert "esphome__running" not in body


def test_modes(generate_main) -> None:
    """Each mode has its own template; max_runs is set on queued and parallel scripts."""
    main_cpp = generate_main(CONFIG)
    for mode, script in (
        ("RestartMode", "inline_restart"),
        ("ParallelMode", "inline_parallel"),
    ):
        _assert_function_script(main_cpp, mode, script)
    # One class for all queued scripts, given the function
    assert constructed(main_cpp, "inline_queued") == (
        f"script::FunctionPointerScript<{_mode_class('QueueingMode')}>"
        "(esphome__inline_queued__fn)"
    )
    assert "inline_queued->set_max_runs(3);" in main_cpp
    assert "inline_parallel->set_max_runs(2);" in main_cpp
    assert re.search(r"App\.register_component_\(inline_queued, \d+\);", main_cpp)
    assert '"QUEUED run"' in _script_body(main_cpp, "inline_queued")
    assert '"PARALLEL run"' in _script_body(main_cpp, "inline_parallel")


def test_restart_executes_itself(generate_main) -> None:
    """A script can execute itself; on_stop keeps its own Automation."""
    main_cpp = generate_main(CONFIG)
    body = _script_body(main_cpp, "inline_restart")
    assert "::inline_restart->execute();" in body
    stop = _automation(main_cpp, r"inline_restart->get_stop_trigger\(\)")
    (action,) = _actions_of(main_cpp, stop)
    assert '"RESTART stopped"' in constructed(main_cpp, action)


def test_parameters_by_value(generate_main) -> None:
    """Lambdas take the parameters by value, so ``x *= 2`` compiles in every form."""
    main_cpp = generate_main(CONFIG)
    _assert_function_script(main_cpp, "SingleMode", "with_params", "int", "std::string")
    assert re.search(
        r"static void esphome__with_params__fn\(const int & esphome__running, "
        r"const std::remove_cvref_t<int> & x, "
        r"const std::remove_cvref_t<std::string> & s\)",
        main_cpp,
    )
    body = _script_body(main_cpp, "with_params")
    assert body.count("[](int x, std::string s) -> void {") == 2
    # A lambda passed to another script copies only the parameters it names
    assert (
        body.count("[](int x, const std::remove_cvref_t<std::string> & s) -> int {")
        == 2
    )
    assert "x *= 2;\n    ESP_LOGI(" in body
    assert '"BY_VALUE again %d", x *= 2' in body
    assert "return x *= 2;" in body
    assert "x *= 2;\n    return x;" in body
    assert body.count("::sink->execute(std::move(esphome__arg_0));") == 2


def test_script_stop_inside(generate_main) -> None:
    """``script.stop`` without an id stops the script it is in; the next action checks."""
    main_cpp = generate_main(CONFIG)
    body = _script_body(main_cpp, "stops_itself")
    stop, rest = body.split("::stops_itself->stop();")
    assert '"STOPS start"' in stop
    assert rest.lstrip("\n }").startswith("if (!esphome__running)\n    return;")
    assert '"STOPS not reached"' in rest


def test_calls_itself_with_parameter(generate_main) -> None:
    """A parallel script with a parameter can execute itself from an ``if``."""
    main_cpp = generate_main(CONFIG)
    _assert_function_script(main_cpp, "ParallelMode", "calls_itself", "int")
    body = _script_body(main_cpp, "calls_itself")
    assert "return n > 0;" in body
    assert "::calls_itself->execute(std::move(esphome__arg_0));" in body


def test_delay_keeps_separate_actions(generate_main) -> None:
    """A delay keeps today's class, Automation and separate actions."""
    main_cpp = generate_main(CONFIG)
    assert constructed(main_cpp, "with_delay") == "script::SingleScript<bool>()"
    assert "static script::SingleScript<bool> *const with_delay" in main_cpp
    before, delay, after = _actions_of(main_cpp, _automation(main_cpp, "with_delay"))
    assert constructed(main_cpp, delay) == "DelayAction<bool>()"
    assert '"DELAY before"' in constructed(main_cpp, before)
    assert '"DELAY after"' in constructed(main_cpp, after)
    assert "esphome__with_delay__fn" not in main_cpp


def test_manual_automation_id(generate_main) -> None:
    """An automation_id set by hand names the Automation, so a lambda can reach it."""
    main_cpp = generate_main(CONFIG)
    assert constructed(main_cpp, "manual_automation") == "script::SingleScript<bool>()"
    assert _automation(main_cpp, "manual_automation") == "manual_automation_id"
    (action,) = _actions_of(main_cpp, "manual_automation_id")
    assert '"MANUAL"' in constructed(main_cpp, action)


@pytest.mark.parametrize(
    ("waiting", "index", "expected"),
    [
        # Runs a function
        ("waits", 0, "script::ScriptWaitAction<script::Script<>, bool>(inline_single)"),
        # Built before the script it waits for
        (
            "waits_early",
            0,
            "script::ScriptWaitAction<script::Script<>, bool>(late_target)",
        ),
        # Has an Automation
        (
            "waits_early",
            1,
            "script::ScriptWaitAction<script::Script<bool>, bool>(with_delay)",
        ),
        # The waiting script's own parameter types are kept
        (
            "waits_with_param",
            0,
            "script::ScriptWaitAction<script::Script<>, script::SingleScript<>*>(late_target)",
        ),
        (
            "waits_mixed",
            0,
            "script::ScriptWaitAction<script::Script<int>, bool>(mixed_inline)",
        ),
    ],
)
def test_script_wait(generate_main, waiting: str, index: int, expected: str) -> None:
    """``script.wait`` is templated on the Script base, whatever class the script gets."""
    main_cpp = generate_main(CONFIG)
    action = _actions_of(main_cpp, _automation(main_cpp, waiting))[index]
    assert constructed(main_cpp, action) == expected


def test_script_needing_automation_keeps_its_group_on_it(generate_main) -> None:
    """An inline script with the mode and parameters of one that needs an Automation uses
    the Automation too, so the queue code is compiled once."""
    main_cpp = generate_main(CONFIG)
    for script in ("mixed_inline", "mixed_delay"):
        assert constructed(main_cpp, script) == "script::QueueingScript<int>()"
        assert f"esphome__{script}__fn" not in main_cpp
    (log,) = _actions_of(main_cpp, _automation(main_cpp, "mixed_inline"))
    assert '"MIXED inline %d"' in constructed(main_cpp, log)
    assert "QueueingMode<script::FunctionBody<int>" not in main_cpp


def test_later_script_reaches_hand_named_automations(generate_main) -> None:
    """An Automation named by hand, including an on_stop one, is created before later scripts
    are built, so a lambda in a later script can use it."""
    main_cpp = generate_main(CONFIG)
    body = _script_body(main_cpp, "reaches_named")
    assert "manual_automation_id->stop();" in body
    assert "restart_stop_auto->stop();" in body
    assert _automation(main_cpp, r"inline_restart->get_stop_trigger\(\)") == (
        "restart_stop_auto"
    )
