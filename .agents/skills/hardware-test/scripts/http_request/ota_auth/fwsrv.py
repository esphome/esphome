"""Serve a www dir for ota.http_request tests; /auth/ needs basic auth otauser:otapass.
Usage: fwsrv.py WWW PORT"""

import base64
import functools
import http.server
import sys

WWW, PORT = sys.argv[1], int(sys.argv[2])
GOOD = "Basic " + base64.b64encode(b"otauser:otapass").decode()


class H(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/auth/") and self.headers.get("Authorization") != GOOD:
            self.send_response(401)
            self.send_header("WWW-Authenticate", 'Basic realm="ota"')
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        super().do_GET()

    def log_message(self, fmt, *args):
        print(
            f"{self.address_string()} auth={'Authorization' in self.headers} {fmt % args}",
            flush=True,
        )


http.server.ThreadingHTTPServer(
    ("0.0.0.0", PORT), functools.partial(H, directory=WWW)
).serve_forever()
