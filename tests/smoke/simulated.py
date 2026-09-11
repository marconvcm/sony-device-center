"""Exercise the actual daemon, CLI and QML app without Bluetooth hardware."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

sonyd, sonyctl, gui = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix="sony-smoke-") as directory:
    env = dict(os.environ, XDG_RUNTIME_DIR=directory, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software")
    endpoint = str(Path(directory) / "sony-device-center.sock")
    with tempfile.TemporaryFile(mode="w+") as daemon_log, tempfile.TemporaryFile(mode="w+") as gui_log:
        daemon = subprocess.Popen([sonyd, "--simulated"], env=env, stdout=daemon_log, stderr=daemon_log)
        app = None
        try:
            deadline = time.monotonic() + 10
            while not Path(endpoint).exists():
                assert daemon.poll() is None, "Daemon exited during startup"
                assert time.monotonic() < deadline, "Daemon did not open its endpoint"
                time.sleep(0.02)
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                client.settimeout(20)
                client.connect(endpoint)
                client.sendall(b'{"version":1,"id":42,"method":"snapshot"}\n')
                data = bytearray()
                while b"\n" not in data:
                    chunk = client.recv(4096)
                    assert chunk, "Daemon closed an incomplete response"
                    data.extend(chunk)
                snapshot = json.loads(data)
                assert snapshot["ok"] and snapshot["id"] == 42
            result = subprocess.run([sonyctl, "devices"], env=env, capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, result.stderr
            assert "WH-1000XM5" in result.stdout
            result = subprocess.run([sonyctl, "--direct", "status"], env=env, capture_output=True, text=True, timeout=5)
            assert result.returncode != 0 and "Stop sonyd" in result.stderr
            app = subprocess.Popen([gui], env=env, stdout=gui_log, stderr=gui_log)
            time.sleep(3)
            assert app.poll() is None, "QML app exited during startup"
            gui_log.seek(0)
            log = gui_log.read()
            assert not any(error in log for error in ["failed to load", "ReferenceError", "TypeError", "Cannot assign", "is not a type"]), log
            assert daemon.poll() is None
        finally:
            for process in [app, daemon]:
                if process is not None:
                    process.terminate()
                    try:
                        process.wait(timeout=15)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
            daemon_log.seek(0)
            print(daemon_log.read())
            gui_log.seek(0)
            print(gui_log.read())
