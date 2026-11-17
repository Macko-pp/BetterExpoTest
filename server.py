# A very small web server. It does two things:
#   1. shows index.html when you open the page
#   2. runs our C program (inside Docker) when the page asks for a calculation
#
# Start it with:  python3 server.py
# Then open:      http://localhost:8000
#
# Level 5 (monitoring): it also counts what it does and shows the numbers at
#   http://localhost:8000/metrics
# so that Prometheus can collect them and Grafana can draw them.

import json
import os
import subprocess
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs

IMAGE = "expo:1.0"   # the image we built in Level 1
PORT = 8000

# Which network address to listen on.
# "localhost" = only this computer can connect (the safe default).
# Level 5: Prometheus runs inside Docker, so it needs HOST=0.0.0.0 to reach us.
HOST = os.environ.get("HOST", "localhost")

# The numbers we keep for monitoring. They start at 0 and only go up
# (they reset when you restart the server).
#   ok_count / error_count : how many calculations worked / failed
#   seconds_total          : total time spent on all calculations
stats = {"ok_count": 0, "error_count": 0, "seconds_total": 0.0}


class Handler(BaseHTTPRequestHandler):

    def do_GET(self):
        url = urlparse(self.path)

        if url.path == "/":
            self.show_page()
        elif url.path == "/calculate":
            self.calculate(parse_qs(url.query))
        elif url.path == "/metrics":
            self.show_metrics()
        else:
            self.send_error(404)

    # Send index.html to the browser
    def show_page(self):
        with open("index.html", "rb") as f:
            page = f.read()
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.end_headers()
        self.wfile.write(page)

    # Run: docker run --rm expo:1.0 <base> <exponent>
    def calculate(self, query):
        base = query.get("base", [""])[0]
        exponent = query.get("exponent", [""])[0]

        # The inputs go in as separate arguments (not as a shell command),
        # so the user cannot sneak extra commands in.
        start = time.time()
        run = subprocess.run(
            ["docker", "run", "--rm", IMAGE, base, exponent],
            capture_output=True, text=True, timeout=20
        )
        stats["seconds_total"] += time.time() - start

        # Exit code 0 = success. Anything else = the C program reported an error.
        if run.returncode == 0:
            stats["ok_count"] += 1
            answer = {"ok": True, "output": run.stdout.strip()}
        else:
            stats["error_count"] += 1
            answer = {"ok": False, "output": run.stderr.strip() or "Something went wrong."}

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(answer).encode())

    # Send our numbers in the format Prometheus understands:
    # one "name{labels} value" per line, with # HELP / # TYPE comment lines.
    # A "counter" is a number that only goes up. Prometheus works out
    # "per second" from it, so we never have to.
    # A "summary" keeps a running total (_sum) and a count (_count):
    # total time / count = the average time.
    def show_metrics(self):
        total = stats["ok_count"] + stats["error_count"]
        text = (
            "# HELP expo_calculations_total Calculations done, by result.\n"
            "# TYPE expo_calculations_total counter\n"
            'expo_calculations_total{result="ok"} %d\n'
            'expo_calculations_total{result="error"} %d\n'
            "# HELP expo_calculation_seconds Time spent on one calculation.\n"
            "# TYPE expo_calculation_seconds summary\n"
            "expo_calculation_seconds_sum %f\n"
            "expo_calculation_seconds_count %d\n"
        ) % (stats["ok_count"], stats["error_count"], stats["seconds_total"], total)

        self.send_response(200)
        self.send_header("Content-Type", "text/plain; version=0.0.4")
        self.end_headers()
        self.wfile.write(text.encode())


print("Open http://localhost:%d in your browser (Ctrl+C to stop)" % PORT)
HTTPServer((HOST, PORT), Handler).serve_forever()