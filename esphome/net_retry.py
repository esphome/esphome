"""Retry policy for HTTP downloads.

Kept import-light on purpose: this module is imported at config time, so it
must not pull in requests (a heavy import, ~85ms) at module scope.
"""

from __future__ import annotations

from collections.abc import Callable
import logging
import time

_LOGGER = logging.getLogger(__name__)

# Transient network failures are retried with 2s/4s backoff, matching
# git.py's _NETWORK_MAX_ATTEMPTS and framework_helpers' mirror downloads.
# Worst case per fetch is NETWORK_MAX_ATTEMPTS timeouts plus 6s of sleeps;
# callers are expected to memoize failures so a flaky host pays that at
# most once per file per run.
NETWORK_MAX_ATTEMPTS = 3


def _is_permanent_dns_failure(e: BaseException) -> bool:
    """Whether a permanent socket.gaierror hides in ``e``'s exception chain.

    EAI_AGAIN ("temporary failure in name resolution", the flaky container
    resolver case) stays retryable; only hard resolution failures like
    NXDOMAIN count. requests wraps urllib3's MaxRetryError, which carries
    the underlying NameResolutionError in its ``reason`` attribute rather
    than the __cause__ chain, so attributes and args are walked as well.
    """
    import socket

    seen: set[int] = set()
    stack: list[BaseException] = [e]
    while stack:
        exc = stack.pop()
        if id(exc) in seen:
            continue
        if isinstance(exc, socket.gaierror):
            # Everything except EAI_AGAIN is treated as permanent. Codes
            # like EAI_NONAME can occur during a brief network outage too,
            # but a 6s backoff rarely outlives one, and permanent means
            # callers fall back to their cache immediately instead of
            # sleeping first (the offline-build case).
            return exc.errno != socket.EAI_AGAIN
        seen.add(id(exc))
        stack.extend(
            nxt
            for nxt in (
                exc.__cause__,
                exc.__context__,
                getattr(exc, "reason", None),  # urllib3 MaxRetryError
                *exc.args,
            )
            if isinstance(nxt, BaseException)
        )
    return False


def is_transient_download_error(e: Exception) -> bool:
    """Return True when a download failure is worth retrying.

    Connection-level failures and HTTP 429/5xx are transient. Hard name
    resolution failures (NXDOMAIN, offline hosts; not EAI_AGAIN), other
    HTTP errors, local errors, and exhausted-attempts EsphomeError
    wrappers (their per-mirror retries are already spent) are permanent.
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
    """Run ``fetch``, retrying failures that ``is_transient_download_error``
    classifies as transient with 2s/4s backoff. Other failures and the
    final attempt's failure propagate to the caller's fallback handling.
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
