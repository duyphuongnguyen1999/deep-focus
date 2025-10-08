import os
import json
import csv
import asyncio
import serial
import serial.tools.list_ports
from pathlib import Path
from fastapi import FastAPI, WebSocket
from fastapi.responses import HTMLResponse, FileResponse
from datetime import datetime
from contextlib import asynccontextmanager

SERIAL_PORT = os.getenv(
    "SERIAL_PORT", "COM5"
)  # "COM5" | "/dev/ttyUSB0" | "/dev/tty.SLAB_USBtoUART"
BAUD = int(os.getenv("BAUD", "115200"))

CSV_DIR = Path("./data")
CSV_DIR.mkdir(parents=True, exist_ok=True)
CSV_FILE = CSV_DIR / "telemetry.csv"

clients: set[WebSocket] = set()
HTML = Path("index.html").read_text(encoding="utf-8")


def ensure_csv():
    new = not CSV_FILE.exists()
    with CSV_FILE.open("a", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        if new:
            w.writerow(["ts", "device_id", "temp_c", "humidity"])


async def broadcast(msg: str):
    dead = []
    for ws in clients:
        try:
            await ws.send_text(msg)
        except Exception:
            dead.append(ws)
    for d in dead:
        clients.discard(d)


async def serial_reader():
    ensure_csv()
    port = SERIAL_PORT
    if not port:
        ports = list(serial.tools.list_ports.comports())
        if not ports:
            print("[WARN] No serial ports found. Set env SERIAL_PORT.")
            await asyncio.sleep(2)
            return
        preferred = [
            p
            for p in ports
            if any(k in p.device.lower() for k in ["usb", "slab", "wch", "acm"])
        ]
        port = preferred[0].device if preferred else ports[0].device
        print(f"[INFO] Auto-selected {port}")

    ser = serial.Serial(port=port, baudrate=BAUD, timeout=1)
    print(f"[INFO] Opened serial {port} @ {BAUD}")
    try:
        while True:
            lineb = ser.readline()
            if not lineb:
                await asyncio.sleep(0.01)
                continue
            try:
                data = json.loads(lineb.decode("utf-8", errors="ignore").strip())
                # gắn timestamp hệ thống (UTC)
                from datetime import timezone

                data["ts"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
            except Exception as e:
                print("[PARSE] skip:", lineb[:80], e)
                continue

            # ghi CSV
            with CSV_FILE.open("a", newline="", encoding="utf-8") as f:
                csv.writer(f).writerow(
                    [
                        data.get("ts"),
                        data.get("device_id"),
                        data.get("temp_c"),
                        data.get("humidity"),
                    ]
                )
            # phát realtime
            await broadcast(json.dumps(data))
    finally:
        ser.close()


@asynccontextmanager
async def lifespan(app: FastAPI):
    t = asyncio.create_task(serial_reader())
    yield
    t.cancel()
    try:
        await t
    except asyncio.CancelledError:
        pass


app = FastAPI(title="DeepFocus Offline UART Demo", lifespan=lifespan)


@app.get("/", response_class=HTMLResponse)
def index():
    return HTML


@app.websocket("/ws")
async def ws(ws: WebSocket):
    await ws.accept()
    clients.add(ws)
    try:
        while True:
            await ws.receive_text()  # optional ping
    except Exception:
        pass
    finally:
        clients.discard(ws)


@app.get("/download")
def download():
    return FileResponse(CSV_FILE, filename="telemetry.csv", media_type="text/csv")
