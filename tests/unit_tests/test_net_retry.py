"""Tests for esphome.net_retry."""

import socket
from unittest.mock import MagicMock, call, patch

import pytest
import requests as req

from esphome.core import EsphomeError
from esphome.net_retry import (
    fetch_with_retry,
    http_request,
    is_transient_download_error,
)


def _http_error(status: int) -> req.HTTPError:
    """An HTTPError carrying a response with the given status, as raised by
    ``raise_for_status`` on a real response."""
    resp = MagicMock()
    resp.status_code = status
    return req.HTTPError(str(status), response=resp)


class TestIsTransientDownloadError:
    def test_connection_errors_are_transient(self) -> None:
        assert is_transient_download_error(req.ConnectionError("reset"))
        assert is_transient_download_error(req.Timeout("timed out"))
        assert is_transient_download_error(
            req.exceptions.ChunkedEncodingError("dropped")
        )
        assert is_transient_download_error(
            req.exceptions.ContentDecodingError("gzip stream truncated")
        )

    def test_http_statuses(self) -> None:
        assert not is_transient_download_error(_http_error(404))
        assert not is_transient_download_error(_http_error(403))
        assert is_transient_download_error(_http_error(429))
        assert is_transient_download_error(_http_error(503))

    def test_http_error_without_response_is_permanent(self) -> None:
        assert not is_transient_download_error(req.HTTPError("boom"))

    def test_hard_dns_failures_are_permanent(self) -> None:
        """Hard resolution failures are permanent via both the cause chain
        and MaxRetryError.reason."""
        from urllib3.exceptions import MaxRetryError, NameResolutionError

        gai = socket.gaierror(socket.EAI_NONAME, "nodename nor servname provided")

        chained = req.ConnectionError("resolution failed")
        chained.__cause__ = gai
        assert not is_transient_download_error(chained)

        # The real urllib3 shape: gaierror on NameResolutionError.__cause__,
        # carried by MaxRetryError.reason.
        try:
            raise NameResolutionError("example.invalid", None, gai) from gai
        except NameResolutionError as nre:
            wrapped = req.ConnectionError(
                MaxRetryError(None, "http://example.invalid/", reason=nre)
            )
        assert not is_transient_download_error(wrapped)

        # A garden-variety connection reset stays transient.
        assert is_transient_download_error(req.ConnectionError("reset by peer"))

    def test_temporary_dns_failure_stays_transient(self) -> None:
        """EAI_AGAIN (flaky resolver) stays retryable."""
        gai = socket.gaierror(socket.EAI_AGAIN, "temporary failure in name resolution")
        chained = req.ConnectionError("resolution failed")
        chained.__cause__ = gai

        assert is_transient_download_error(chained)

    def test_implicit_context_does_not_reclassify(self) -> None:
        """A gaierror riding along as implicit __context__ must not turn a
        genuine connection reset permanent."""
        try:
            try:
                raise socket.gaierror(socket.EAI_NONAME, "first attempt")
            except socket.gaierror:
                raise req.ConnectionError("reset by peer") from None
        except req.ConnectionError as reset:
            assert reset.__context__ is not None
            assert is_transient_download_error(reset)

    def test_gaierror_without_errno_stays_transient(self) -> None:
        """A gaierror carrying no EAI code cannot prove a hard failure."""
        chained = req.ConnectionError("resolution failed")
        chained.__cause__ = socket.gaierror("no errno")

        assert is_transient_download_error(chained)

    def test_mixed_chain_hard_failure_wins(self) -> None:
        """EAI_AGAIN in the chain does not mask a hard failure elsewhere."""
        again = socket.gaierror(socket.EAI_AGAIN, "temporary failure")
        hard = socket.gaierror(socket.EAI_NONAME, "unknown host")

        outer = req.ConnectionError(hard)
        outer.__cause__ = again
        assert not is_transient_download_error(outer)

        outer = req.ConnectionError(again)
        outer.__cause__ = hard
        assert not is_transient_download_error(outer)

    def test_dns_walk_survives_exception_cycles(self) -> None:
        """A cyclic cause chain must terminate (and stay transient when no
        resolution failure is present)."""
        outer = req.ConnectionError("a")
        inner = ValueError("b")
        outer.__cause__ = inner
        inner.__cause__ = outer

        assert is_transient_download_error(outer)

    def test_exhausted_resume_attempts_are_permanent(self) -> None:
        """download_with_resume already spent its own resume attempts; its
        EsphomeError wrapper is not retried again at the sweep level."""
        wrapped = EsphomeError("Failed to download after 3 attempts")
        wrapped.__cause__ = req.ConnectionError("down")
        assert not is_transient_download_error(wrapped)

    def test_unrelated_errors_are_permanent(self) -> None:
        assert not is_transient_download_error(OSError("disk full"))
        assert not is_transient_download_error(EsphomeError("size mismatch"))


class TestFetchWithRetry:
    def test_logs_the_upcoming_attempt_number(
        self, caplog: pytest.LogCaptureFixture
    ) -> None:
        """The warning names the attempt about to run, not the failed one."""
        with (
            patch("esphome.net_retry.time.sleep") as mock_sleep,
            pytest.raises(req.ConnectionError),
        ):
            fetch_with_retry(
                "https://example.com/f",
                lambda: (_ for _ in ()).throw(req.ConnectionError("reset")),
            )

        assert mock_sleep.call_args_list == [call(2), call(4)]
        assert "(attempt 2/3)" in caplog.text
        assert "(attempt 3/3)" in caplog.text


class TestHttpRequest:
    def test_applies_happy_eyeballs_and_forwards_arguments(self) -> None:
        with (
            patch("esphome.net_retry.ensure_happy_eyeballs") as mock_he,
            patch("requests.get", return_value=MagicMock()) as mock_get,
        ):
            resp = http_request(
                "GET",
                "https://example.com/f",
                timeout=30,
                stream=True,
                headers={"Range": "bytes=4-"},
            )
        mock_he.assert_called_once_with()
        assert resp is mock_get.return_value
        assert mock_get.call_args == call(
            "https://example.com/f",
            timeout=30,
            stream=True,
            headers={"Range": "bytes=4-"},
            allow_redirects=True,
        )

    def test_dispatches_head_through_requests_head(self) -> None:
        """Dispatch goes through requests.get/head so tests patching those
        entry points keep working."""
        with patch("requests.head", return_value=MagicMock()) as mock_head:
            http_request("HEAD", "https://example.com/f", timeout=(5, 30))
        assert mock_head.call_args[1]["timeout"] == (5, 30)

    def test_no_status_handling(self) -> None:
        """Error statuses are the caller's problem; nothing raises here."""
        resp = MagicMock(status_code=404)
        with patch("requests.get", return_value=resp):
            assert http_request("GET", "https://example.com/f", timeout=1) is resp
