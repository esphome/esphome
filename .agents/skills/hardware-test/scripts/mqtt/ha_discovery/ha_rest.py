"""Home Assistant REST calls for the ha_discovery scripts.   env: HA_URL (default http://127.0.0.1:8129)"""

import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

BASE = os.environ.get("HA_URL", "http://127.0.0.1:8129")


def req(
    path: str, data: dict | None = None, *, token: str | None = None, form: bool = False
) -> dict | list:
    """GET, or POST DATA as JSON (form encoded with FORM); an HTTP error returns {"http_error": code, "body": text}."""
    h = {"Authorization": "Bearer " + token} if token else {}
    body = None
    if data is not None and form:
        body, h["Content-Type"] = (
            urllib.parse.urlencode(data).encode(),
            "application/x-www-form-urlencoded",
        )
    elif data is not None:
        body, h["Content-Type"] = json.dumps(data).encode(), "application/json"
    try:
        with urllib.request.urlopen(
            urllib.request.Request(BASE + path, body, h), timeout=30
        ) as r:
            return json.loads(r.read() or b"null")
    except urllib.error.HTTPError as e:
        return {"http_error": e.code, "body": e.read().decode(errors="replace")[:500]}


def failed(r: dict | list) -> bool:
    return isinstance(r, dict) and "http_error" in r


def req_or_exit(path: str, data: dict | None = None, **kw) -> dict | list:
    """req() that exits with the status and body on an HTTP error."""
    if failed(r := req(path, data, **kw)):
        sys.exit(f"{path}: HTTP {r['http_error']} {r['body']}")
    return r
