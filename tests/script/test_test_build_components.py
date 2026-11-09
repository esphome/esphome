"""Unit tests for script/test_build_components.py logging helpers."""

from collections.abc import Callable
from pathlib import Path
import subprocess
import sys

import pytest

# Add the script directory to the path so we can import the module under test.
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "script"))

import test_build_components as tbc  # noqa: E402


class _FakeCompleted:
    """Minimal stand-in for subprocess.CompletedProcess."""

    def __init__(self, returncode: int) -> None:
        self.returncode = returncode


@pytest.fixture
def _no_ci(monkeypatch: pytest.MonkeyPatch) -> None:
    """Ensure GITHUB_ACTIONS is unset so group markers are suppressed."""
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)


@pytest.fixture
def _ci(monkeypatch: pytest.MonkeyPatch) -> None:
    """Pretend we are running inside GitHub Actions."""
    monkeypatch.setenv("GITHUB_ACTIONS", "true")


def test_start_log_group_outside_ci_is_silent(
    _no_ci: None, capsys: pytest.CaptureFixture[str]
) -> None:
    tbc.start_log_group("hello")
    assert capsys.readouterr().out == ""


def test_end_log_group_outside_ci_is_silent(
    _no_ci: None, capsys: pytest.CaptureFixture[str]
) -> None:
    tbc.end_log_group()
    assert capsys.readouterr().out == ""


def test_start_log_group_in_ci_emits_marker(
    _ci: None, capsys: pytest.CaptureFixture[str]
) -> None:
    tbc.start_log_group("hello")
    assert capsys.readouterr().out == "::group::hello\n"


def test_end_log_group_in_ci_emits_marker(
    _ci: None, capsys: pytest.CaptureFixture[str]
) -> None:
    tbc.end_log_group()
    assert capsys.readouterr().out == "::endgroup::\n"


def _make_base_file(tmp_path: Path) -> Path:
    base_file = tmp_path / "base.yaml"
    base_file.write_text("esphome:\n  name: $component_test_file\n")
    return base_file


def test_run_esphome_test_wraps_output_in_group(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A passing single-component test is bracketed by group markers."""
    monkeypatch.setattr(tbc.subprocess, "run", lambda *a, **k: _FakeCompleted(0))
    repo_root = Path(tbc.__file__).parent.parent
    test_file = repo_root / "tests" / "components" / "foo" / "test.esp32-idf.yaml"

    result = tbc.run_esphome_test(
        component="foo",
        test_file=test_file,
        platform="esp32-idf",
        platform_with_version="esp32-idf",
        base_file=_make_base_file(tmp_path),
        build_dir=tmp_path,
        esphome_command="config",
        continue_on_fail=True,
    )

    out = capsys.readouterr().out
    assert result.success is True
    assert "::group::[foo] [test] [esp32-idf]" in out
    assert "::endgroup::" in out
    # The header line is printed inside the group.
    assert out.index("::group::") < out.index("> [foo]") < out.index("::endgroup::")


@pytest.mark.parametrize(
    ("platform", "command", "skip", "expects_flag"),
    [
        ("esp32-idf", "compile", True, True),
        ("esp32-s3-idf", "compile", True, True),
        ("esp32-ard", "compile", True, True),
        ("esp32-idf", "compile", False, False),
        ("esp32-idf", "config", True, False),
        ("esp8266-ard", "compile", True, False),
    ],
    ids=[
        "idf-skip",
        "idf-variant",
        "arduino-as-idf-component",
        "idf-off",
        "config",
        "esp8266",
    ],
)
def test_run_esphome_test_skip_bootloader_argv(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    platform: str,
    command: str,
    skip: bool,
    expects_flag: bool,
) -> None:
    """The flag lands on esp32 family compiles only."""
    seen: list[list[str]] = []

    def fake_run(cmd: list[str], **kwargs: object) -> _FakeCompleted:
        seen.append(cmd)
        return _FakeCompleted(0)

    monkeypatch.setattr(tbc.subprocess, "run", fake_run)
    repo_root = Path(tbc.__file__).parent.parent
    test_file = repo_root / "tests" / "components" / "foo" / f"test.{platform}.yaml"
    tbc.run_esphome_test(
        component="foo",
        test_file=test_file,
        platform=platform,
        platform_with_version=platform,
        base_file=_make_base_file(tmp_path),
        build_dir=tmp_path,
        esphome_command=command,
        continue_on_fail=True,
        skip_bootloader=skip,
    )
    assert ("--skip-bootloader" in seen[0]) is expects_flag


def test_run_grouped_test_skip_bootloader_argv(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    """The grouped command site adds the flag the same way."""
    seen: list[list[str]] = []

    def fake_run(cmd: list[str], **kwargs: object) -> _FakeCompleted:
        seen.append(cmd)
        return _FakeCompleted(0)

    monkeypatch.setattr(tbc.subprocess, "run", fake_run)
    repo_root = Path(tbc.__file__).parent.parent
    tests_dir = repo_root / "tests" / "components"
    tbc.run_grouped_test(
        components=["gpio"],
        platform="esp32-idf",
        platform_with_version="esp32-idf",
        base_file=_make_base_file(tmp_path),
        build_dir=tmp_path,
        tests_dir=tests_dir,
        esphome_command="compile",
        continue_on_fail=True,
        skip_bootloader=True,
    )
    assert "--skip-bootloader" in seen[0]


def test_run_esphome_test_closes_group_before_failure_report(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """On a fail-fast failure the group closes before the reproduce report."""
    monkeypatch.setattr(tbc.subprocess, "run", lambda *a, **k: _FakeCompleted(1))
    repo_root = Path(tbc.__file__).parent.parent
    test_file = repo_root / "tests" / "components" / "foo" / "test.esp32-idf.yaml"

    # continue_on_fail=False makes the failure raise after printing the
    # reproduce block, which is the path that must stay outside the group.
    with pytest.raises(tbc.subprocess.CalledProcessError):
        tbc.run_esphome_test(
            component="foo",
            test_file=test_file,
            platform="esp32-idf",
            platform_with_version="esp32-idf",
            base_file=_make_base_file(tmp_path),
            build_dir=tmp_path,
            esphome_command="config",
            continue_on_fail=False,
        )

    out = capsys.readouterr().out
    assert "::endgroup::" in out
    assert "FAILED - Command to reproduce:" in out
    # The group must be closed before the failure report is printed.
    assert out.index("::endgroup::") < out.index("FAILED - Command to reproduce:")


def test_run_esphome_test_closes_group_when_subprocess_raises(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """If the subprocess raises, the group is still closed (via finally)."""

    def _boom(*a: object, **k: object) -> None:
        raise OSError("boom")

    monkeypatch.setattr(tbc.subprocess, "run", _boom)
    repo_root = Path(tbc.__file__).parent.parent
    test_file = repo_root / "tests" / "components" / "foo" / "test.esp32-idf.yaml"

    with pytest.raises(OSError, match="boom"):
        tbc.run_esphome_test(
            component="foo",
            test_file=test_file,
            platform="esp32-idf",
            platform_with_version="esp32-idf",
            base_file=_make_base_file(tmp_path),
            build_dir=tmp_path,
            esphome_command="config",
            continue_on_fail=True,
        )

    assert "::endgroup::" in capsys.readouterr().out


def test_run_grouped_test_wraps_output_in_group(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A grouped test is bracketed by group markers listing its components."""
    monkeypatch.setattr(tbc.subprocess, "run", lambda *a, **k: _FakeCompleted(0))
    monkeypatch.setattr(tbc, "merge_component_configs", lambda **k: None)

    result = tbc.run_grouped_test(
        components=["foo", "bar"],
        platform="esp32-idf",
        platform_with_version="esp32-idf",
        base_file=_make_base_file(tmp_path),
        build_dir=tmp_path,
        tests_dir=tmp_path,
        esphome_command="config",
        continue_on_fail=True,
    )

    out = capsys.readouterr().out
    assert result.success is True
    assert "::group::[GROUPED: foo, bar] [esp32-idf]" in out
    assert out.index("::group::") < out.index("> [GROUPED") < out.index("::endgroup::")


def test_run_grouped_test_closes_group_before_failure_report(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A fail-fast grouped failure closes the group before the report."""
    monkeypatch.setattr(tbc.subprocess, "run", lambda *a, **k: _FakeCompleted(1))
    monkeypatch.setattr(tbc, "merge_component_configs", lambda **k: None)

    with pytest.raises(tbc.subprocess.CalledProcessError):
        tbc.run_grouped_test(
            components=["foo", "bar"],
            platform="esp32-idf",
            platform_with_version="esp32-idf",
            base_file=_make_base_file(tmp_path),
            build_dir=tmp_path,
            tests_dir=tmp_path,
            esphome_command="config",
            continue_on_fail=False,
        )

    out = capsys.readouterr().out
    assert out.index("::endgroup::") < out.index("FAILED - Command to reproduce:")


def test_run_grouped_test_closes_group_when_subprocess_raises(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """If the grouped subprocess raises, the group is still closed (finally)."""

    def _boom(*a: object, **k: object) -> None:
        raise OSError("boom")

    monkeypatch.setattr(tbc.subprocess, "run", _boom)
    monkeypatch.setattr(tbc, "merge_component_configs", lambda **k: None)

    with pytest.raises(OSError, match="boom"):
        tbc.run_grouped_test(
            components=["foo", "bar"],
            platform="esp32-idf",
            platform_with_version="esp32-idf",
            base_file=_make_base_file(tmp_path),
            build_dir=tmp_path,
            tests_dir=tmp_path,
            esphome_command="config",
            continue_on_fail=True,
        )

    assert "::endgroup::" in capsys.readouterr().out


def test_components_empty_match_fails_with_flag(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Under --fail-on-no-tests, a real component filtered to a platform
    with no matching test file must not pass CI as a green zero-component
    compile."""
    rc = tbc.test_components(
        ["logger"],
        "zz-none",
        "compile",
        False,
        enable_grouping=False,
        fail_on_no_tests=True,
    )
    assert rc == 1
    assert "No tests ran for requested pattern(s): logger" in (capsys.readouterr().out)


def test_components_component_with_no_base_file_fails_with_flag(
    capsys: pytest.CaptureFixture[str],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A component whose fixture matches the platform but whose platform has
    no base file builds nothing; under the flag that silent zero fails by
    component name instead of hiding behind other components."""
    monkeypatch.setattr(tbc, "get_platform_base_files", lambda base_dir: {})
    rc = tbc.test_components(
        ["logger"],
        "esp8266-ard",
        "compile",
        False,
        enable_grouping=False,
        fail_on_no_tests=True,
    )
    assert rc == 1
    assert "No tests ran for requested pattern(s): logger" in (capsys.readouterr().out)


def test_components_blank_list_fails_with_flag(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A fully blank component list must not slide into the baseline
    fallback and exit green under the flag."""
    rc = tbc.test_components(
        [""], "esp8266-ard", "compile", False, fail_on_no_tests=True
    )
    assert rc == 1
    assert "blank component list" in capsys.readouterr().out


def test_components_wildcard_no_match_fails_with_flag(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A wildcard matching nothing must not degrade to the synthetic
    baseline build and exit green under the flag."""
    rc = tbc.test_components(
        ["zz_no_such*"],
        "esp8266-ard",
        "compile",
        False,
        enable_grouping=False,
        fail_on_no_tests=True,
    )
    assert rc == 1
    assert "No components found matching" in capsys.readouterr().out


def test_components_empty_match_tolerated_without_flag() -> None:
    """The esp32-ard smoke leg deliberately builds only the subset with a
    matching fixture; without the flag an empty match stays green."""
    assert (
        tbc.test_components(
            ["logger"], "zz-none", "compile", False, enable_grouping=False
        )
        == 0
    )


def test_components_unknown_component_fails_with_flag(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A renamed smoke-test component must shrink coverage loudly, not fall
    into the reference-baseline build."""
    rc = tbc.test_components(
        ["no_such_component_xyz"],
        "esp8266-ard",
        "compile",
        False,
        enable_grouping=False,
        fail_on_no_tests=True,
    )
    assert rc == 1
    assert "No components found matching" in capsys.readouterr().out


_PARTNER_BUSES: dict[str, dict[str, list[str]]] = {
    "tca9548a": {
        "esp32-idf": ["i2c"],
        "rp2040-ard": ["i2c"],
        "nrf52-adafruit": ["i2c"],
    },
    "bmi270": {"esp32-idf": ["i2c"]},
    "icm20689": {"esp32-idf": ["spi"], "rp2040-ard": ["spi"]},
    "ds2484": {"esp32-idf": ["i2c"], "rp2040-ard": ["i2c"]},
    "ds248x": {"esp32-idf": ["i2c"], "rp2040-ard": ["i2c"]},
}
_GY39_ESP32 = {"esp32-idf": ["i2c"]}


@pytest.fixture
def _no_conflicts(monkeypatch: pytest.MonkeyPatch) -> None:
    """Treat every pair of components as compatible."""
    monkeypatch.setattr(tbc, "components_conflict", lambda a, b, platform: False)


def _buses(**extra: dict[str, list[str]]) -> dict[str, dict[str, list[str]]]:
    return {**_PARTNER_BUSES, **extra}


@pytest.mark.parametrize(
    ("component", "buses", "platform", "expected", "non_groupable"),
    [
        # One partner per type, preferred candidates first
        ("gy39", _GY39_ESP32, "esp32-idf", ["tca9548a", "bmi270", "ds2484"], set()),
        # No bmi270 test on rp2040: motion falls back to icm20689
        (
            "gy39",
            {"rp2040-ard": ["i2c"]},
            "rp2040-ard",
            ["tca9548a", "icm20689", "ds2484"],
            set(),
        ),
        # Only tca9548a has an nrf52 test
        ("gy39", {"nrf52-adafruit": ["i2c"]}, "nrf52-adafruit", ["tca9548a"], set()),
        # A component without buses can be grouped with any partner
        (
            "template",
            {"esp32-idf": []},
            "esp32-idf",
            ["tca9548a", "bmi270", "ds2484"],
            set(),
        ),
        # A different i2c package config never shares a build with the partners
        (
            "bh1900nux",
            {"esp32-idf": ["i2c_low_freq"]},
            "esp32-idf",
            ["icm20689"],
            set(),
        ),
        # Never its own partner: next candidate, or none if there is no other
        ("tca9548a", None, "esp32-idf", ["bmi270", "ds2484"], set()),
        ("bmi270", None, "esp32-idf", ["tca9548a", "icm20689", "ds2484"], set()),
        ("ds2484", None, "esp32-idf", ["tca9548a", "bmi270", "ds248x"], set()),
        # Platform the component has no test for
        ("gy39", _GY39_ESP32, "esp8266-ard", [], set()),
        # Non-groupable (isolated, base bus, direct bus) never meets a partner in CI
        ("gy39", _GY39_ESP32, "esp32-idf", [], {"gy39"}),
        # A non-groupable partner falls through to the next candidate
        (
            "gy39",
            _GY39_ESP32,
            "esp32-idf",
            ["tca9548a", "icm20689", "ds2484"],
            {"bmi270"},
        ),
    ],
)
def test_select_shared_id_partners(
    _no_conflicts: None,
    component: str,
    buses: dict[str, list[str]] | None,
    platform: str,
    expected: list[str],
    non_groupable: set[str],
) -> None:
    component_buses = _buses() if buses is None else _buses(**{component: buses})
    assert (
        tbc.select_shared_id_partners(
            component, platform, component_buses, non_groupable
        )
        == expected
    )


def test_select_shared_id_partners_skips_conflicting_partner(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A partner that conflicts with the component or an earlier partner is skipped."""
    monkeypatch.setattr(
        tbc,
        "components_conflict",
        lambda a, b, platform: {a, b} in ({"gy39", "ds2484"}, {"tca9548a", "bmi270"}),
    )
    component_buses = _buses(gy39=_GY39_ESP32)
    assert tbc.select_shared_id_partners(
        "gy39", "esp32-idf", component_buses, set()
    ) == [
        "tca9548a",
        "icm20689",
        "ds248x",
    ]


def _result(
    components: list[str],
    success: bool,
    output: str = "",
    platform: str = "esp32-idf",
    shared_id_check: bool = False,
) -> tbc.TestResult:
    return tbc.TestResult(
        test_id="GROUPED",
        components=components,
        platform=platform,
        success=success,
        duration=0.0,
        test_type="config",
        output=output,
        shared_id_check=shared_id_check,
    )


_GY39_FAILURE = (
    "INFO noise\nFailed config\n\nsensor.gy39:\n"
    "  Too many candidates found for 'i2c_id'"
)


@pytest.fixture
def fake_grouped(
    _no_conflicts: None, monkeypatch: pytest.MonkeyPatch
) -> Callable[..., list[dict[str, object]]]:
    """Patch run_grouped_test to record its calls and return a fixed outcome."""

    def setup(success: bool = True, output: str = "") -> list[dict[str, object]]:
        calls: list[dict[str, object]] = []

        def fake(**kwargs: object) -> tbc.TestResult:
            calls.append(kwargs)
            return _result(
                kwargs["components"], success, output, kwargs["platform_with_version"]
            )

        monkeypatch.setattr(tbc, "run_grouped_test", fake)
        return calls

    return setup


def _run_partners(
    tmp_path: Path,
    components: set[str],
    component_buses: dict[str, dict[str, list[str]]],
    platform_filter: str | None = None,
    prior_results: list[tbc.TestResult] | None = None,
    continue_on_fail: bool = True,
) -> list[tbc.TestResult]:
    ctx = tbc.SharedIdContext(
        platform_bases={
            p: [tmp_path / f"build_components_base.{p}.yaml"]
            for p in ("esp32-idf", "rp2040-ard")
        },
        platform_filter=platform_filter,
        tests_dir=tmp_path,
        build_dir=tmp_path,
        component_buses=component_buses,
        non_groupable={"animation"},
        continue_on_fail=continue_on_fail,
    )
    return tbc.run_shared_id_tests(ctx, components, prior_results or [])


def test_run_shared_id_tests_reports_failure(
    fake_grouped: Callable[..., list[dict[str, object]]],
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Only changed, groupable components run, config-only, and a failure names everything."""
    calls = fake_grouped(False, _GY39_FAILURE)
    results = _run_partners(
        tmp_path,
        {"gy39", "animation"},
        _buses(gy39=_GY39_ESP32, animation=_GY39_ESP32),
    )
    assert [c["components"] for c in calls] == [
        ["gy39", "tca9548a", "bmi270", "ds2484"]
    ]
    assert calls[0]["esphome_command"] == "config"
    assert calls[0]["capture_output"] is True
    assert [r.test_id for r in results] == [
        "SHARED_ID[gy39+tca9548a,bmi270,ds2484].esp32-idf"
    ]
    assert all(r.shared_id_check for r in results)
    out = capsys.readouterr().out
    assert "gy39 fails config validation on esp32-idf" in out
    assert "tca9548a,bmi270,ds2484" in out
    assert "Too many candidates found for 'i2c_id'" in out
    assert "INFO noise" not in out


def test_run_shared_id_tests_respects_platform_filter(
    fake_grouped: Callable[..., list[dict[str, object]]],
    tmp_path: Path,
) -> None:
    calls = fake_grouped()
    _run_partners(
        tmp_path,
        {"gy39"},
        _buses(gy39={"esp32-idf": ["i2c"], "rp2040-ard": ["i2c"]}),
        platform_filter="rp2040-ard",
    )
    assert [c["platform"] for c in calls] == ["rp2040-ard"]


def test_run_shared_id_tests_skips_component_that_fails_alone(
    fake_grouped: Callable[..., list[dict[str, object]]],
    tmp_path: Path,
) -> None:
    """A test that already fails on its own gets no shared-id run blaming grouping."""
    calls = fake_grouped()
    results = _run_partners(
        tmp_path,
        {"gy39"},
        _buses(gy39=_GY39_ESP32),
        prior_results=[_result(["gy39"], False)],
    )
    assert calls == []
    assert results == []


def test_run_shared_id_tests_skips_group_with_failing_partner(
    fake_grouped: Callable[..., list[dict[str, object]]],
    tmp_path: Path,
) -> None:
    """A partner that fails alone would make the group fail and blame the component."""
    calls = fake_grouped()
    results = _run_partners(
        tmp_path,
        {"gy39"},
        _buses(gy39=_GY39_ESP32),
        prior_results=[_result(["tca9548a"], False)],
    )
    assert calls == []
    assert results == []


def test_run_shared_id_tests_fails_fast_without_continue_on_fail(
    fake_grouped: Callable[..., list[dict[str, object]]],
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Without -f the first shared-id failure stops the run after its diagnostic."""
    calls = fake_grouped(False, _GY39_FAILURE)
    with pytest.raises(subprocess.CalledProcessError):
        _run_partners(
            tmp_path,
            {"gy39", "lm75b"},
            _buses(gy39=_GY39_ESP32, lm75b=_GY39_ESP32),
            continue_on_fail=False,
        )
    assert len(calls) == 1
    assert "gy39 fails config validation" in capsys.readouterr().out


def test_reproduce_commands_keep_shared_id_groups_apart() -> None:
    """A shared-id failure gets its own command, never folded into the per-platform list."""
    partner = _result(
        ["gy39", "tca9548a", "bmi270", "ds2484"], False, shared_id_check=True
    )
    plain = _result(["foo"], False, "")
    assert tbc.reproduce_commands([plain, partner], "gcc") == [
        "script/test_build_components.py -c foo -t esp32-idf -e config --toolchain gcc",
        (
            "script/test_build_components.py -c gy39,tca9548a,bmi270,ds2484 "
            "-t esp32-idf -e config --toolchain gcc"
        ),
    ]


def test_github_summary_uses_shared_id_reproduce_command() -> None:
    partner = _result(["gy39", "tca9548a"], False, shared_id_check=True)
    summary = tbc.format_github_summary([partner])
    assert "-c gy39,tca9548a -t esp32-idf -e config" in summary


def _patch_component_runs(
    monkeypatch: pytest.MonkeyPatch,
    grouped: list[tbc.TestResult],
    partners: list[tbc.TestResult],
) -> list[set[str]]:
    seen: list[set[str]] = []
    monkeypatch.setattr(
        tbc, "analyze_all_components", lambda tests_dir: ({}, set(), set())
    )
    monkeypatch.setattr(
        tbc, "run_grouped_component_tests", lambda **kwargs: (set(), grouped)
    )
    monkeypatch.setattr(tbc, "run_individual_component_test", lambda **kwargs: None)

    def fake_partners(
        ctx: tbc.SharedIdContext, components: set[str], prior_results: object
    ) -> list[tbc.TestResult]:
        seen.append(components)
        return partners

    monkeypatch.setattr(tbc, "run_shared_id_tests", fake_partners)
    return seen


@pytest.mark.parametrize(
    ("command", "isolate", "expected"),
    [("config", {"gy39"}, 1), ("compile", {"gy39"}, 0), ("config", None, 0)],
)
def test_components_runs_partners_only_for_isolated_config(
    monkeypatch: pytest.MonkeyPatch,
    command: str,
    isolate: set[str] | None,
    expected: int,
) -> None:
    """The --isolate config run is the hook; compile runs never pay for it."""
    seen = _patch_component_runs(monkeypatch, [], [])
    tbc.test_components(
        ["gy39"], "esp32-idf", command, True, isolated_components=isolate
    )
    assert len(seen) == expected


def test_components_shared_id_run_does_not_count_as_build(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """--fail-on-no-tests ignores shared-id runs that mention a requested component."""
    partner = _result(["gy39", "tca9548a"], True, shared_id_check=True)
    _patch_component_runs(monkeypatch, [], [partner])
    rc = tbc.test_components(
        ["gy39", "tca9548a"],
        "esp32-idf",
        "config",
        True,
        isolated_components={"gy39"},
        fail_on_no_tests=True,
    )
    assert rc == 1
    assert "No tests ran for requested pattern(s): gy39, tca9548a" in (
        capsys.readouterr().out
    )


def test_run_grouped_test_capture_output(
    _no_ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Captured output is printed, and kept on the result only when the run fails."""
    returncodes = iter([1, 0])

    def fake_run(cmd: list[str], **kwargs: object) -> _FakeCompleted:
        assert kwargs["stdout"] is tbc.subprocess.PIPE
        result = _FakeCompleted(next(returncodes))
        result.stdout = "Failed config\nboom\n"
        return result

    monkeypatch.setattr(tbc.subprocess, "run", fake_run)
    monkeypatch.setattr(tbc, "merge_component_configs", lambda **k: None)

    def run() -> tbc.TestResult:
        return tbc.run_grouped_test(
            components=["foo", "bar"],
            platform="esp32-idf",
            platform_with_version="esp32-idf",
            base_file=_make_base_file(tmp_path),
            build_dir=tmp_path,
            tests_dir=tmp_path,
            esphome_command="config",
            continue_on_fail=True,
            capture_output=True,
        )

    failed = run()
    assert not failed.success
    assert failed.output == "Failed config\nboom\n"
    assert "boom" in capsys.readouterr().out
    passed = run()
    assert passed.success
    assert passed.output == ""
