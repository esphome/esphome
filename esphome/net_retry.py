"""Retry policy for HTTP downloads.

Kept import-light on purpose: this module is imported at config time, so it
must not pull in requests (a heavy import, ~85ms) at module scope.
"""

from __future__ import annotations

from collections.abc import Callable
import logging
import time

_LOGGER = logging.getLogger(__name__)

# 3 tries with 2s/4s backoff, matching git.py's _NETWORK_MAX_ATTEMPTS.
# Callers memoize failures so a flaky host pays this once per file per run.
NETWORK_MAX_ATTEMPTS = 3


def _is_permanent_dns_failure(e: BaseException) -> bool:
    """Whether a hard socket.gaierror hides in ``e``'s exception chain.

    EAI_AGAIN (flaky resolver) stays retryable; anything else is permanent
    so offline builds fall back to their cache without sleeping first.
    Narrower than git.py, which retries NXDOMAIN too.

    Walks ``__cause__``, ``args`` (requests wraps MaxRetryError without
    ``from``) and MaxRetryError's ``reason``, but not implicit
    ``__context__``: an unrelated earlier attempt's resolution failure
    must not reclassify an error it did not cause.
    """
    import socket

    seen: set[int] = set()
    stack: list[BaseException] = [e]
    while stack:
        exc = stack.pop()
        if id(exc) in seen:
            continue
        if (
            isinstance(exc, socket.gaierror)
            and exc.errno is not None
            and exc.errno != socket.EAI_AGAIN
        ):
            return True
        seen.add(id(exc))
        stack.extend(
            nxt
            for nxt in (
                exc.__cause__,
                getattr(exc, "reason", None),  # urllib3 MaxRetryError
                *exc.args,
            )
            if isinstance(nxt, BaseException)
        )
    return False


def is_transient_download_error(e: Exception) -> bool:
    """Return True when a download failure is worth retrying.

    Connection-level failures and HTTP 429/5xx are transient; hard DNS
    failures, other HTTP errors, and local errors are permanent.
    """
    # Imported lazily: requests is a heavy import (~85ms) and is only
    # needed when actually downloading, never during config validation.
    import requests

    if isinstance(e, requests.exceptions.HTTPError):
        resp = e.response
        return resp is not None and (resp.status_code == 429 or resp.status_code >= 500)
    if isinstance(e, requests.exceptions.ConnectionError) and _is_permanent_dns_failure(
        e
    ):
        return False
    # SSLError (a ConnectionError subclass) stays transient on purpose: it
    # also covers mid-handshake connection drops, not just bad certificates.
    return isinstance(
        e,
        (
            requests.exceptions.ConnectionError,
            requests.exceptions.Timeout,
            requests.exceptions.ChunkedEncodingError,
            requests.exceptions.ContentDecodingError,
        ),
    )


def fetch_with_retry[T](url: str, fetch: Callable[[], T], what: str = "Download") -> T:
    """Run ``fetch``, retrying transient failures with 2s/4s backoff.

    Permanent failures and the final attempt propagate to the caller;
    ``what`` names the operation in the retry warning.
    """
    import requests

    for attempt in range(1, NETWORK_MAX_ATTEMPTS):
        try:
            return fetch()
        except requests.exceptions.RequestException as e:
            if not is_transient_download_error(e):
                raise
            delay = 2**attempt
            _LOGGER.warning(
                "%s of %s failed: %s. Retrying in %d seconds... (attempt %d/%d)",
                what,
                url,
                e,
                delay,
                attempt + 1,
                NETWORK_MAX_ATTEMPTS,
            )
            time.sleep(delay)
    return fetch()
