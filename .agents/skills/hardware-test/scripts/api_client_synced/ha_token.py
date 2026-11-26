"""Log in to a throwaway Home Assistant and write a fresh access token to HADIR/ha-token.txt.
Usage: ha_token.py HADIR   env: HA_URL (default http://127.0.0.1:8129)
HADIR/ha-test-login.txt holds two lines, `user NAME` and `password SECRET`, of the test login made at onboarding.
Onboarding tokens expire after 30 min; REST calls then return 401."""

from pathlib import Path
import sys

sys.path.insert(
    0, str(Path(__file__).resolve().parent.parent / "mqtt" / "ha_discovery")
)
from ha_rest import BASE, req_or_exit  # noqa: E402

d = Path(sys.argv[1])
lines = [
    line
    for line in d.joinpath("ha-test-login.txt").read_text().splitlines()
    if line.strip()
]
user, pwd = (line.split(None, 1)[1].strip() for line in lines[:2])
cid = BASE + "/"
f = req_or_exit(
    "/auth/login_flow",
    {"client_id": cid, "handler": ["homeassistant", None], "redirect_uri": cid},
)
r = req_or_exit(
    f"/auth/login_flow/{f['flow_id']}",
    {"client_id": cid, "username": user, "password": pwd},
)
t = req_or_exit(
    "/auth/token",
    {"grant_type": "authorization_code", "code": r["result"], "client_id": cid},
    form=True,
)
d.joinpath("ha-token.txt").write_text(t["access_token"])
print("token ok, expires in", t["expires_in"])
