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
        _constructed(main_cpp, script) == f"{_function_script(mode, script, *types)}()"
    )


def _wait_class(script_class: str) -> str:
    return f"script::ScriptWaitAction<{script_class}>"


def _script_body(main_cpp: str, script: str) -> str:
    """The function a script runs; it has no Automation."""
    assert not re.search(rf"\bAutomation<.*?>\({script}\);", main_cpp)
    return _function_body(main_cpp, f"esphome__{script}__fn")


def test_inline_actions_run_as_one_function(generate_main) -> None:
    """A script of inline actions calls one function, declared before the script."""
    main_cpp = generate_main(CONFIG)
    _assert_function_script(main_cpp, "SingleMode", "inline_single")
    class_ = _function_script("SingleMode", "inline_single")
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
    assert _constructed(main_cpp, "inline_queued") == (
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
    assert '"RESTART stopped"' in _constructed(main_cpp, action)


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
    assert body.count("[](int x, std::string s) -> int {") == 2
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
    assert _constructed(main_cpp, "with_delay") == "script::SingleScript<>()"
    assert "static script::SingleScript<> *const with_delay" in main_cpp
    before, delay, after = _actions_of(main_cpp, _automation(main_cpp, "with_delay"))
    assert _constructed(main_cpp, delay) == "DelayAction<>()"
    assert '"DELAY before"' in _constructed(main_cpp, before)
    assert '"DELAY after"' in _constructed(main_cpp, after)
    assert "esphome__with_delay__fn" not in main_cpp


def test_manual_automation_id(generate_main) -> None:
    """An automation_id set by hand names the Automation, so a lambda can reach it."""
    main_cpp = generate_main(CONFIG)
    assert _constructed(main_cpp, "manual_automation") == "script::SingleScript<>()"
    assert _automation(main_cpp, "manual_automation") == "manual_automation_id"
    (action,) = _actions_of(main_cpp, "manual_automation_id")
    assert '"MANUAL"' in _constructed(main_cpp, action)


def test_script_wait(generate_main) -> None:
    """``script.wait`` on a script that runs a function uses the class shared by its mode."""
    main_cpp = generate_main(CONFIG)
    (action,) = _actions_of(main_cpp, _automation(main_cpp, "waits"))
    class_ = _wait_class(_mode_class("SingleMode"))
    assert _constructed(main_cpp, action) == f"{class_}(inline_single)"


def test_script_wait_built_first(generate_main) -> None:
    """A ``script.wait`` built before the script it waits for gets the shared class too."""
    main_cpp = generate_main(CONFIG)
    late, delay = _actions_of(main_cpp, _automation(main_cpp, "waits_early"))
    class_ = _wait_class(_mode_class("SingleMode"))
    assert _constructed(main_cpp, late) == f"{class_}(late_target)"
    assert f"static {class_} *const {late}" in main_cpp
    assert _constructed(main_cpp, delay) == (
        f"{_wait_class('script::SingleScript<>')}(with_delay)"
    )


def test_script_wait_keeps_waiting_script_parameters(generate_main) -> None:
    """Only the waited-for script's class changes, not a parameter with the same name."""
    main_cpp = generate_main(CONFIG)
    automation = _automation(main_cpp, "waits_with_param")
    wait, _ = _actions_of(main_cpp, automation)
    assert _constructed(main_cpp, wait) == (
        f"script::ScriptWaitAction<{_mode_class('SingleMode')}, "
        "script::SingleScript<>*>(late_target)"
    )
