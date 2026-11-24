"""Serve update manifests: stable.json, beta.json and slow.json (answered after a delay); logs every request."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import sys
import time

PORT = int(sys.argv[1])
DELAY = float(sys.argv[2]) if len(sys.argv) > 2 else 4.0


def manifest(name: str, version: str) -> bytes:
    builds = [
        {
            "chipFamily": fam,
            "ota": {
                "path": f"/{name}.bin",
                "md5": "d41d8cd98f00b204e9800998ecf8427e",
                "summary": f"{name} channel",
            },
        }
        for fam in ("ESP8266", "ESP32")
    ]
    return json.dumps(
        {"name": f"Bench {name}", "version": version, "builds": builds}
    ).encode()


DOCS = {
    "/stable.json": manifest("stable", "1.0.0"),
    "/beta.json": manifest("beta", "2.0.0-beta1"),
    "/slow.json": manifest("slow", "3.0.0-slow"),
}


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:  # noqa: N802
        body = DOCS.get(self.path)
        print(
            f"{time.strftime('%H:%M:%S')} {self.client_address[0]} GET {self.path} -> {404 if body is None else 200}",
            flush=True,
        )
        if body is None:
            self.send_error(404)
            return
        if self.path == "/slow.json":
            time.sleep(DELAY)
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args) -> None:  # quiet default log
        return


ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
