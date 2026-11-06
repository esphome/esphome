"""Onboard a throwaway Home Assistant: create a generated test user and save a long lived access token.

usage: onboard.py DIR   env: HA_URL (default http://127.0.0.1:8129)
Writes DIR/ha-test-login.txt (user and generated password) and DIR/ha-token.txt. Keep DIR out of any repository.
"""

import json
import os
import pathlib
import secrets
import sys
import urllib.parse
import urllib.request

BASE = os.environ.get("HA_URL", "http://127.0.0.1:8129")
CID = BASE + "/"
d = pathlib.Path(sys.argv[1])


def req(path: str, data: dict, form: bool = False) -> dict:
    if form:
        body, ctype = (
            urllib.parse.urlencode(data).encode(),
            "application/x-www-form-urlencoded",
        )
    else:
        body, ctype = json.dumps(data).encode(), "application/json"
    with urllib.request.urlopen(
        urllib.request.Request(BASE + path, body, {"Content-Type": ctype}), timeout=30
    ) as r:
        return json.loads(r.read() or b"null")


pw = secrets.token_urlsafe(16)
(d / "ha-test-login.txt").write_text(f"user hwtest\npassword {pw}\n", encoding="utf-8")
user = {
    "client_id": CID,
    "name": "hw test",
    "username": "hwtest",
    "password": pw,
    "language": "en",
}
code = req("/api/onboarding/users", user)["auth_code"]
tok = req(
    "/auth/token",
    {"grant_type": "authorization_code", "code": code, "client_id": CID},
    form=True,
)
(d / "ha-token.txt").write_text(tok["access_token"], encoding="utf-8")
print(f"token in {d / 'ha-token.txt'}")
