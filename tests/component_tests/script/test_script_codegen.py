"""Code generation of scripts.

Objects are found from the script ids and the links between them, not from auto-numbered
ids, so adding or changing one script does not break the others.
"""

import re

CONFIG = "tests/component_tests/script/test_script_codegen.yaml"


def _constructed(main_cpp: str, id_: str) -> str:
    match = re.search(rf"\bnew\({id_}\) (.*?);\n", main_cpp, re.DOTALL)
    assert match is not None, id_
    return match.group(1)


def _function_body(main_cpp: str, name: str) -> str:
    match = re.search(
        rf"static void {name}\([^;{{]*\) \{{(.*?)\n\}}\n", main_cpp, re.DOTALL
    )
    assert match is not None, name
    return match.group(1)


def _automation(main_cpp: str, trigger: str) -> str:
    match = re.search(rf"\bnew\((\w+)\) Automation<.*?>\({trigger}\);", main_cpp)
    assert match is not None, trigger
    return match.group(1)


def _actions_of(main_cpp: str, automation: str) -> list[str]:
    return re.findall(rf"\b{automation}->add_action\((\w+)\);", main_cpp)


def _single_action(main_cpp: str, script: str) -> str:
    (action,) = _actions_of(main_cpp, _automation(main_cpp, script))
    return action


def _run_function(main_cpp: str, script: str) -> str:
    """Return the function of the one merged action the script's Automation runs."""
    constructed = _constructed(main_cpp, _single_action(main_cpp, script))
    match = re.match(r"ApplyRunAction<(\w+)[,>]", constructed)
    assert match is not None, constructed
    return match.group(1)


def test_inline_actions_merge_into_one_action(generate_main) -> None:
    """A script of inline actions runs them as one merged action of an Automation."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "inline_single") == "script::SingleScript<>()"
    body = _function_body(main_cpp, _run_function(main_cpp, "inline_single"))
    assert '"INLINE first"' in body
    assert "counter->value() += 1;" in body
    # A script can be stopped from inside, so the rest of the run checks for it
    assert "if (!esphome__running)\n    return;" in body


def test_one_lambda_action(generate_main) -> None:
    """A single inline action keeps its own action object."""
    main_cpp = generate_main(CONFIG)
    constructed = _constructed(main_cpp, _single_action(main_cpp, "one_action"))
    assert constructed.startswith("StatelessLambdaAction<>(")
    assert '"ONE action"' in constructed


def test_modes(generate_main) -> None:
    """Each mode has its own class; max_runs is set on queued and parallel scripts."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "inline_restart") == "script::RestartScript<>()"
    assert _constructed(main_cpp, "inline_queued") == "script::QueueingScript<>()"
    assert _constructed(main_cpp, "inline_parallel") == "script::ParallelScript<>()"
    assert "inline_queued->set_max_runs(3);" in main_cpp
    assert "inline_parallel->set_max_runs(2);" in main_cpp
    assert re.search(r"App\.register_component_\(inline_queued, \d+\);", main_cpp)
    for script in ("inline_queued", "inline_parallel"):
        constructed = _constructed(main_cpp, _single_action(main_cpp, script))
        assert constructed.startswith("StatelessLambdaAction<>(")


def test_restart_executes_itself(generate_main) -> None:
    """A script can execute itself; on_stop has its own Automation."""
    main_cpp = generate_main(CONFIG)
    body = _function_body(main_cpp, _run_function(main_cpp, "inline_restart"))
    assert "::inline_restart->execute();" in body
    stop = _automation(main_cpp, r"inline_restart->get_stop_trigger\(\)")
    (action,) = _actions_of(main_cpp, stop)
    assert '"RESTART stopped"' in _constructed(main_cpp, action)


def test_parameters_by_value(generate_main) -> None:
    """Lambdas take the parameters by value, so ``x *= 2`` compiles in every form."""
    main_cpp = generate_main(CONFIG)
    assert (
        _constructed(main_cpp, "with_params")
        == "script::SingleScript<int, std::string>()"
    )
    name = _run_function(main_cpp, "with_params")
    assert re.search(
        rf"static void {name}\(const int & esphome__running, "
        r"const std::remove_cvref_t<int> & x, "
        r"const std::remove_cvref_t<std::string> & s\)",
        main_cpp,
    )
    body = _function_body(main_cpp, name)
    assert body.count("[](int x, std::string s) -> void {") == 2
    assert body.count("[](int x, std::string s) -> int {") == 2
    assert "x *= 2;\n    ESP_LOGI(" in body
    assert '"BY_VALUE again %d", x *= 2' in body
    assert "return x *= 2;" in body
    assert "x *= 2;\n    return x;" in body
    assert body.count("::sink->execute(std::move(esphome__arg_0));") == 2


def test_script_stop_inside(generate_main) -> None:
    """``script.stop`` without an id stops the script it is in; the next action checks."""
    main_cpp = generate_main(CONFIG)
    body = _function_body(main_cpp, _run_function(main_cpp, "stops_itself"))
    stop, rest = body.split("::stops_itself->stop();")
    assert '"STOPS start"' in stop
    assert rest.lstrip("\n }").startswith("if (!esphome__running)\n    return;")
    assert '"STOPS not reached"' in rest


def test_calls_itself_with_parameter(generate_main) -> None:
    """A parallel script with a parameter can execute itself from an ``if``."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "calls_itself") == "script::ParallelScript<int>()"
    body = _function_body(main_cpp, _run_function(main_cpp, "calls_itself"))
    assert "return n > 0;" in body
    assert "::calls_itself->execute(std::move(esphome__arg_0));" in body


def test_delay_keeps_separate_actions(generate_main) -> None:
    """A delay keeps the Automation and its separate actions."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "with_delay") == "script::SingleScript<>()"
    before, delay, after = _actions_of(main_cpp, _automation(main_cpp, "with_delay"))
    assert _constructed(main_cpp, delay) == "DelayAction<>()"
    assert '"DELAY before"' in _constructed(main_cpp, before)
    assert '"DELAY after"' in _constructed(main_cpp, after)


def test_manual_automation_id(generate_main) -> None:
    """An automation_id set by hand names the Automation, so a lambda can reach it."""
    main_cpp = generate_main(CONFIG)
    assert _automation(main_cpp, "manual_automation") == "manual_automation_id"
    (action,) = _actions_of(main_cpp, "manual_automation_id")
    assert '"MANUAL"' in _constructed(main_cpp, action)


def test_script_wait(generate_main) -> None:
    """``script.wait`` is a component templated on the class of the script it waits for."""
    main_cpp = generate_main(CONFIG)
    (action,) = _actions_of(main_cpp, _automation(main_cpp, "waits"))
    assert (
        _constructed(main_cpp, action)
        == "script::ScriptWaitAction<script::SingleScript<>>(inline_single)"
    )
