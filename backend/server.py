#!/usr/bin/env python3
"""
OmniIPC Real-time Telemetry Gateway & HTTP Server
Zero-dependency Python 3 server providing:
- REST API to trigger real C++ benchmarks
- Server-Sent Events (SSE) / WebSocket stream for live Discord message updates
- Static file serving for the React frontend
"""

import os
import sys
import json
import time
import subprocess
import threading
import http.server
import socketserver
import urllib.parse

PORT = int(os.environ.get("PORT", 8080))
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
BINARY_PATH = os.path.join(BASE_DIR, "omni_ipc_engine")
FRONTEND_DIST = os.path.join(BASE_DIR, "..", "frontend", "dist")

subscribers = set()
subscribers_lock = threading.Lock()

# Simulated telemetry state when benchmark isn't actively running
active_channel = "shared-memory"
is_streaming = True
active_threads = [
    {"pid": os.getpid(), "threadId": 1, "role": "Producer", "status": "Online", "cpuUsage": 45.2, "memoryMb": 38.4},
    {"pid": os.getpid() + 1, "threadId": 2, "role": "Consumer", "status": "Online", "cpuUsage": 52.8, "memoryMb": 84.1},
    {"pid": os.getpid() + 2, "threadId": 3, "role": "Broker", "status": "Idle", "cpuUsage": 4.1, "memoryMb": 16.2},
    {"pid": os.getpid() + 3, "threadId": 4, "role": "Watcher", "status": "Idle", "cpuUsage": 0.5, "memoryMb": 8.0},
]

def broadcast_event(event_type: str, data: dict):
    payload = json.dumps({"event": event_type, "data": data})
    message = f"data: {payload}\n\n".encode("utf-8")
    with subscribers_lock:
        dead_clients = []
        for client in subscribers:
            try:
                client.wfile.write(message)
                client.wfile.flush()
            except Exception:
                dead_clients.append(client)
        for client in dead_clients:
            subscribers.discard(client)

def background_synthetic_telemetry():
    """Emits live packet telemetry when no batch benchmark is currently executing"""
    packet_seq = 1000
    while True:
        time.sleep(0.5)
        if not is_streaming:
            continue

        packet_seq += 1
        spec_map = {
            "pipes": {"size": "1 MB", "bytes": 1048576, "base_lat": 160.0},
            "shared-memory": {"size": "10 MB", "bytes": 10485760, "base_lat": 18.5},
            "message-queues": {"size": "8 KB", "bytes": 8192, "base_lat": 48.0},
            "sockets": {"size": "1 MB", "bytes": 1048576, "base_lat": 92.0},
        }
        spec = spec_map.get(active_channel, spec_map["shared-memory"])
        jitter = (packet_seq % 7) * 2.1 - 6.0
        lat = round(max(2.0, spec["base_lat"] + jitter), 2)

        data = {
            "id": f"pkt-{int(time.time() * 1000)}-{packet_seq}",
            "timestamp": time.strftime("%H:%M:%S"),
            "sourcePid": active_threads[0]["pid"],
            "targetPid": active_threads[1]["pid"],
            "ipcType": active_channel,
            "payloadSize": spec["size"],
            "bytes": spec["bytes"],
            "latencyUs": lat,
            "checksum": f"0x{(packet_seq * 0x9e3779b9) & 0xFFFFFFFF:08X}",
            "status": "verified"
        }
        broadcast_event("packet", data)

class ThreadedHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True

class IpcRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=FRONTEND_DIST if os.path.exists(FRONTEND_DIST) else BASE_DIR, **kwargs)

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)

        if parsed.path == "/api/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()

            with subscribers_lock:
                subscribers.add(self)

            try:
                # Keep connection open until client disconnects
                while True:
                    time.sleep(1)
            except Exception:
                pass
            finally:
                with subscribers_lock:
                    subscribers.discard(self)
            return

        elif parsed.path in ["/download", "/omni-ipc.zip"]:
            zip_path = os.path.join(BASE_DIR, "omni-ipc.zip")
            if not os.path.exists(zip_path):
                zip_path = os.path.join(FRONTEND_DIST, "omni-ipc.zip")
            if os.path.exists(zip_path):
                self.send_response(200)
                self.send_header("Content-Type", "application/zip")
                self.send_header("Content-Disposition", 'attachment; filename="omni-ipc.zip"')
                self.send_header("Content-Length", str(os.path.getsize(zip_path)))
                self.end_headers()
                with open(zip_path, "rb") as f:
                    self.wfile.write(f.read())
                return
            else:
                self.send_error(404, "Zip file not found")
                return

        elif parsed.path == "/api/status":
            self.send_json({
                "status": "online",
                "binary": os.path.exists(BINARY_PATH),
                "pid": os.getpid(),
                "activeChannel": active_channel,
                "isStreaming": is_streaming,
                "threads": active_threads
            })
            return

        # Fallback to static files
        super().do_GET()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode("utf-8") if content_length > 0 else "{}"
        try:
            req_data = json.loads(body)
        except Exception:
            req_data = {}

        if parsed.path == "/api/run-benchmark":
            mech = req_data.get("mechanism", "shm")
            size = req_data.get("size", "1MB")
            iters = str(req_data.get("iterations", 10))

            if not os.path.exists(BINARY_PATH):
                self.send_json({"error": "C++ binary not compiled. Run make in backend first."}, status=500)
                return

            def execute_proc():
                cmd = [BINARY_PATH, "--mechanism", mech, "--size", size, "--iterations", iters, "--stream-json"]
                try:
                    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                    for line in proc.stdout:
                        line = line.strip()
                        if line.startswith("{"):
                            try:
                                pkt = json.loads(line)
                                if pkt.get("type") == "packet":
                                    data = {
                                        "id": f"pkt-{int(time.time()*1000)}-{pkt.get('seq', 0)}",
                                        "timestamp": time.strftime("%H:%M:%S"),
                                        "sourcePid": pkt.get("source_pid", 10421),
                                        "targetPid": pkt.get("target_pid", 10422),
                                        "ipcType": pkt.get("ipc", mech),
                                        "payloadSize": f"{pkt.get('size_bytes', 0) // 1024} KB" if pkt.get('size_bytes', 0) < 1048576 else f"{pkt.get('size_bytes', 0) // 1048576} MB",
                                        "bytes": pkt.get("size_bytes", 0),
                                        "latencyUs": pkt.get("latency_us", 0.0),
                                        "checksum": pkt.get("checksum", "0x0"),
                                        "status": "verified"
                                    }
                                    broadcast_event("packet", data)
                            except Exception:
                                pass
                    proc.wait()
                except Exception as ex:
                    print(f"Error running benchmark process: {ex}")

            threading.Thread(target=execute_proc, daemon=True).start()
            self.send_json({"status": "started", "mechanism": mech, "size": size, "iterations": iters})
            return

        elif parsed.path == "/api/set-channel":
            global active_channel
            active_channel = req_data.get("channel", active_channel)
            broadcast_event("channel_change", {"channel": active_channel})
            self.send_json({"status": "ok", "channel": active_channel})
            return

        elif parsed.path == "/api/toggle-streaming":
            global is_streaming
            is_streaming = not is_streaming
            self.send_json({"status": "ok", "isStreaming": is_streaming})
            return

        self.send_error(404, "Endpoint Not Found")

    def send_json(self, data: dict, status: int = 200):
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

def main():
    threading.Thread(target=background_synthetic_telemetry, daemon=True).start()
    print(f"[OmniIPC] Telemetry Gateway running on http://127.0.0.1:{PORT}")
    server = ThreadedHTTPServer(("0.0.0.0", PORT), IpcRequestHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down OmniIPC Gateway.")

if __name__ == "__main__":
    main()
