"""Tests for the remote file prefetch validation step."""

from __future__ import annotations

from collections.abc import Iterable
from pathlib import Path
from types import SimpleNamespace
from typing import Any
from unittest.mock import MagicMock, patch

import pytest

from esphome import core
from esphome.config import Config, PrefetchRemoteFilesValidationStep
import esphome.config_validation as cv
from esphome.external_files import RemoteFile


def _component(prefetch: Any = None, is_platform: bool = False) -> SimpleNamespace:
    return SimpleNamespace(
        is_platform_component=is_platform,
        prefetch_files=prefetch,
    )


def _run_step(
    domains: dict[str, Any],
    components: dict[str, Any],
    platforms: dict[tuple[str, str], Any] | None = None,
    download_side_effect: Any = None,
) -> tuple[Config, MagicMock]:
    result = Config()
    for domain, conf in domains.items():
        result[domain] = conf
    with (
        patch("esphome.config.get_component", side_effect=components.get),
        patch(
            "esphome.config.get_platform",
            side_effect=lambda d, p: (platforms or {}).get((d, p)),
        ),
        patch(
            "esphome.external_files.download_content_many",
            side_effect=download_side_effect,
        ) as mock_download,
    ):
        PrefetchRemoteFilesValidationStep().run(result)
    return result, mock_download


def _downloaded(mock_download: MagicMock, call: int = 0) -> list[RemoteFile]:
    return list(mock_download.call_args_list[call][0][0])


def test_component_hook_receives_normalized_entries() -> None:
    """A bare dict conf is passed to the hook as a one-entry list."""
    seen: list[Any] = []

    def hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        seen.append(entries)
        yield [RemoteFile("https://example.com/a", Path("/cache/a"))]

    _, mock_download = _run_step(
        {"my_comp": {"key": "value"}},
        {"my_comp": _component(prefetch=hook)},
    )

    assert seen == [[{"key": "value"}]]
    mock_download.assert_called_once()
    assert _downloaded(mock_download) == [
        RemoteFile("https://example.com/a", Path("/cache/a"))
    ]


def test_platform_entries_are_grouped_per_platform() -> None:
    """Platform domains route entries to each platform module's hook."""
    seen_a: list[Any] = []
    seen_b: list[Any] = []

    def hook_a(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        seen_a.extend(entries)
        yield [RemoteFile("url-a", Path("/a"))]

    def hook_b(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        seen_b.extend(entries)
        yield [RemoteFile("url-b", Path("/b"))]

    entries = [
        {"platform": "a", "n": 1},
        {"platform": "b", "n": 2},
        {"platform": "a", "n": 3},
    ]
    _, mock_download = _run_step(
        {"image": entries},
        {"image": _component(is_platform=True)},
        platforms={
            ("image", "a"): _component(prefetch=hook_a),
            ("image", "b"): _component(prefetch=hook_b),
        },
    )

    assert seen_a == [entries[0], entries[2]]
    assert seen_b == [entries[1]]
    assert sorted(_downloaded(mock_download), key=lambda f: f.url) == [
        RemoteFile("url-a", Path("/a")),
        RemoteFile("url-b", Path("/b")),
    ]


def test_hook_failure_does_not_fail_validation(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A raising hook is logged and other hooks still prefetch."""

    def bad_hook(entries: list[dict]) -> list[RemoteFile]:
        raise RuntimeError("garbage config")

    def good_hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        yield [RemoteFile("url", Path("/g"))]

    _, mock_download = _run_step(
        {"bad": {"x": 1}, "good": {"y": 2}},
        {
            "bad": _component(prefetch=bad_hook),
            "good": _component(prefetch=good_hook),
        },
    )

    assert "Remote file prefetch for bad failed" in caplog.text
    assert _downloaded(mock_download) == [RemoteFile("url", Path("/g"))]


def test_stages_download_between_resumptions() -> None:
    """Each yielded stage is downloaded before the generator resumes."""
    order: list[str] = []

    def hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        order.append("stage1")
        yield [RemoteFile("css-url", Path("/css"))]
        order.append("stage2")
        yield [RemoteFile("ttf-url", Path("/ttf"))]

    def record_download(items: Any, description: str) -> None:
        order.append(f"download:{[file.url for file in items]}")

    _, mock_download = _run_step(
        {"font": {"f": 1}},
        {"font": _component(prefetch=hook)},
        download_side_effect=record_download,
    )

    assert order == [
        "stage1",
        "download:['css-url']",
        "stage2",
        "download:['ttf-url']",
    ]
    assert mock_download.call_count == 2


def test_runaway_generator_is_capped(caplog: pytest.LogCaptureFixture) -> None:
    """An endless generator stops after the stage backstop."""

    def hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        n = 0
        while True:
            yield [RemoteFile(f"url-{n}", Path(f"/f{n}"))]
            n += 1

    _, mock_download = _run_step(
        {"my_comp": {"x": 1}},
        {"my_comp": _component(prefetch=hook)},
    )

    assert mock_download.call_count == 10
    assert "stopped after" in caplog.text


def test_mid_stage_failure_stops_only_that_hook(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A generator raising on a later stage does not affect other hooks."""

    def flaky_hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        yield [RemoteFile("first", Path("/first"))]
        raise RuntimeError("stage two exploded")

    def steady_hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        yield [RemoteFile("one", Path("/one"))]
        yield [RemoteFile("two", Path("/two"))]

    _, mock_download = _run_step(
        {"flaky": {"x": 1}, "steady": {"y": 2}},
        {
            "flaky": _component(prefetch=flaky_hook),
            "steady": _component(prefetch=steady_hook),
        },
    )

    assert "Remote file prefetch for flaky failed" in caplog.text
    assert mock_download.call_count == 2
    assert _downloaded(mock_download, 1) == [RemoteFile("two", Path("/two"))]


def test_download_failure_is_swallowed() -> None:
    """cv.Invalid from the batch download never escapes the step."""

    def hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        yield [RemoteFile("url", Path("/p"))]

    result, mock_download = _run_step(
        {"my_comp": {"x": 1}},
        {"my_comp": _component(prefetch=hook)},
        download_side_effect=cv.Invalid("download failed"),
    )

    mock_download.assert_called_once()
    assert not result.errors


def test_domains_without_hooks_do_not_download() -> None:
    """Components without PREFETCH_FILES cause no download call."""
    _, mock_download = _run_step(
        {"plain": {"x": 1}, ".ignored": {"y": 2}, "unknown": {"z": 3}},
        {"plain": _component()},
    )
    mock_download.assert_not_called()


def test_none_and_autoload_confs_are_skipped() -> None:
    """None and AutoLoad confs never reach a hook."""
    hook = MagicMock()
    _, mock_download = _run_step(
        {"a": None, "b": core.AutoLoad()},
        {"a": _component(prefetch=hook), "b": _component(prefetch=hook)},
    )
    hook.assert_not_called()
    mock_download.assert_not_called()


def test_non_dict_entries_are_ignored() -> None:
    """Garbage entries never reach a component hook."""
    hook = MagicMock()
    _, mock_download = _run_step(
        {"my_comp": ["just-a-string", 42]},
        {"my_comp": _component(prefetch=hook)},
    )
    hook.assert_not_called()
    mock_download.assert_not_called()


def test_platform_entries_without_platform_key_are_ignored() -> None:
    """Entries with a missing or unknown platform never reach a hook."""
    _, mock_download = _run_step(
        {"image": [{"n": 1}, "garbage", {"platform": "unknown"}]},
        {"image": _component(is_platform=True)},
    )
    mock_download.assert_not_called()


def test_generator_still_alive_at_the_cap_is_warned_and_closed(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A generator with a stage left at the cap is warned about and closed."""
    closed: list[bool] = []

    def hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        try:
            for n in range(10):
                yield [RemoteFile(f"url-{n}", Path(f"/f{n}"))]
        finally:
            closed.append(True)

    _, mock_download = _run_step(
        {"my_comp": {"x": 1}},
        {"my_comp": _component(prefetch=hook)},
    )

    assert mock_download.call_count == 10
    assert "stopped after" in caplog.text
    assert closed == [True]


def test_plain_iterable_hook_survives_the_cap(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A hook returning a plain list of batches cannot crash the backstop."""

    def hook(entries: list[dict]) -> list[list[RemoteFile]]:
        return [[RemoteFile(f"url-{n}", Path(f"/f{n}"))] for n in range(12)]

    _, mock_download = _run_step(
        {"my_comp": {"x": 1}},
        {"my_comp": _component(prefetch=hook)},
    )

    assert mock_download.call_count == 10
    assert "stopped after" in caplog.text


def test_domain_level_hook_on_platform_component() -> None:
    """A hook on the platform component's domain module sees all entries."""
    seen: list[Any] = []

    def domain_hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        seen.append(entries)
        yield [RemoteFile("domain-url", Path("/domain"))]

    entries = [{"platform": "a", "n": 1}, {"platform": "b", "n": 2}]
    _, mock_download = _run_step(
        {"image": entries},
        {"image": _component(prefetch=domain_hook, is_platform=True)},
    )

    assert seen == [entries]
    assert _downloaded(mock_download) == [RemoteFile("domain-url", Path("/domain"))]


def test_generator_raising_on_close_is_contained(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A generator whose close() raises at the cap is logged, not crashed on."""

    def hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        try:
            for n in range(10):
                yield [RemoteFile(f"url-{n}", Path(f"/f{n}"))]
        except GeneratorExit:
            raise RuntimeError("close exploded") from None

    _, mock_download = _run_step(
        {"my_comp": {"x": 1}},
        {"my_comp": _component(prefetch=hook)},
    )

    assert mock_download.call_count == 10
    assert "stopped after" in caplog.text


def test_unexpected_download_error_is_logged_visibly(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A broken batch downloader warns instead of silently disabling prefetch."""

    def hook(entries: list[dict]) -> Iterable[list[RemoteFile]]:
        yield [RemoteFile("url", Path("/p"))]

    result, mock_download = _run_step(
        {"my_comp": {"x": 1}},
        {"my_comp": _component(prefetch=hook)},
        download_side_effect=TypeError("not a RemoteFile"),
    )

    mock_download.assert_called_once()
    assert not result.errors
    assert "Remote file prefetch failed" in caplog.text
