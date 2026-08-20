"""Tests for esphome.net_retry."""

import socket
from unittest.mock import MagicMock, call, patch

import pytest
import requests as req

from esphome.core import EsphomeError
from esphome.net_retry import fetch_with_retry, is_transient_download_error


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
        """Hard name resolution failures won't heal within a retry window;
        offline builds must fall back to their cache without sleeping first.
        requests can surface the gaierror via the cause chain or via
        urllib3's MaxRetryError.reason attribute."""
        gai = socket.gaierror(socket.EAI_NONAME, "nodename nor servname provided")

        chained = req.ConnectionError("resolution failed")
        chained.__cause__ = gai
        assert not is_transient_download_error(chained)

        class _FakeMaxRetryError(Exception):
            def __init__(self, reason: BaseException) -> None:
                super().__init__("max retries exceeded")
                self.reason = reason

        wrapped = req.ConnectionError(_FakeMaxRetryError(gai))
        assert not is_transient_download_error(wrapped)

        # A garden-variety connection reset stays transient.
        assert is_transient_download_error(req.ConnectionError("reset by peer"))

    def test_temporary_dns_failure_stays_transient(self) -> None:
        """EAI_AGAIN is a temporary resolver failure (flaky container DNS)
        and does heal, matching git.py's policy of retrying DNS flakes."""
        gai = socket.gaierror(socket.EAI_AGAIN, "temporary failure in name resolution")
        chained = req.ConnectionError("resolution failed")
        chained.__cause__ = gai

        assert is_transient_download_error(chained)

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
        """The warning names the attempt about to run, not the one that just
        failed, so a user sees attempt 2/3 and 3/3 before the final failure."""
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
