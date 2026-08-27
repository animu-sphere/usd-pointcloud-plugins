"""Static range-serving HTTP origin for the Tier 2 resolver integration.

This is a measurement fixture, not a transport. It serves one file over
loopback, honours a single `Range: bytes=` request, and writes a request log so
the bytes an external resolver actually fetched can be divided by the source
size. Nothing in the product depends on it, and nothing here knows what a COPC
is.

    python tools/tier2_fixture_server.py --file <path> --route /data.copc \
        --log <path> [--port 0]

The chosen port is printed as `port=<n>` on the first line of stdout, so a
driver can bind port 0 and read back what the kernel assigned.
"""

from __future__ import annotations

import argparse
import http.server
import json
import os
import re
import threading
import time

RANGE = re.compile(r"^bytes=(\d*)-(\d*)$")


def _write_log() -> None:
    if not Handler.log_path:
        return
    payload = {"sourceBytes": len(Handler.payload), "requests": Handler.log}
    temporary = Handler.log_path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as output:
        json.dump(payload, output, indent=2)
    os.replace(temporary, Handler.log_path)


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    # Written by the server owner before serving.
    payload: bytes = b""
    route: str = "/"
    validator: str = ""
    log: list = []
    log_path: str = ""
    lock = threading.Lock()

    def log_message(self, *args):  # noqa: D102 - quiet; the JSON log is the record
        return

    def _record(self, method: str, status: int, sent: int) -> None:
        # Flushed on every request rather than at shutdown: the driver stops
        # this process by killing it, and a log that only exists after a clean
        # exit is a log that is not there when the measurement needs it.
        with Handler.lock:
            Handler.log.append({
                "method": method,
                "path": self.path,
                "range": self.headers.get("Range", ""),
                "status": status,
                "bytesSent": sent,
                "monotonic": time.monotonic(),
            })
            _write_log()

    def _reject(self, method: str, status: int) -> None:
        self.send_response(status)
        self.send_header("Content-Length", "0")
        self.end_headers()
        self._record(method, status, 0)

    def _common_headers(self, length: int) -> None:
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("ETag", Handler.validator)
        self.send_header("Content-Length", str(length))

    def do_HEAD(self) -> None:  # noqa: N802 - http.server naming
        if self.path != Handler.route:
            self._reject("HEAD", 404)
            return
        self.send_response(200)
        self._common_headers(len(Handler.payload))
        self.end_headers()
        self._record("HEAD", 200, 0)

    def do_GET(self) -> None:  # noqa: N802 - http.server naming
        if self.path != Handler.route:
            self._reject("GET", 404)
            return
        total = len(Handler.payload)
        header = self.headers.get("Range")
        if not header:
            self.send_response(200)
            self._common_headers(total)
            self.end_headers()
            self.wfile.write(Handler.payload)
            self._record("GET", 200, total)
            return

        match = RANGE.match(header.strip())
        if not match:
            self._reject("GET", 416)
            return
        first, last = match.group(1), match.group(2)
        if first == "":
            if last == "":
                self._reject("GET", 416)
                return
            start, end = max(0, total - int(last)), total - 1
        else:
            start = int(first)
            end = total - 1 if last == "" else min(int(last), total - 1)
        if start > end or start >= total:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{total}")
            self.send_header("Content-Length", "0")
            self.end_headers()
            self._record("GET", 416, 0)
            return

        body = Handler.payload[start:end + 1]
        self.send_response(206)
        self.send_header("Content-Range", f"bytes {start}-{end}/{total}")
        self._common_headers(len(body))
        self.end_headers()
        self.wfile.write(body)
        self._record("GET", 206, len(body))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--file", required=True)
    parser.add_argument("--route", default="/fixture.copc")
    parser.add_argument("--log", required=True)
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--validator", default="")
    arguments = parser.parse_args()

    with open(arguments.file, "rb") as source:
        Handler.payload = source.read()
    Handler.route = arguments.route
    Handler.validator = arguments.validator or '"{:x}-{:x}"'.format(
        len(Handler.payload), os.stat(arguments.file).st_mtime_ns)
    Handler.log = []
    Handler.log_path = arguments.log
    _write_log()

    server = http.server.ThreadingHTTPServer(("127.0.0.1", arguments.port),
                                             Handler)
    print(f"port={server.server_address[1]}", flush=True)
    print(f"size={len(Handler.payload)}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        with Handler.lock:
            _write_log()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
