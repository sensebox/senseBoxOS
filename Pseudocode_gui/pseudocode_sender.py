#!/usr/bin/env python3
"""
Pseudocode Sender GUI for ESP32-S2 (senseBox)
- Connect over USB Serial and send lines to your interpreter.
- Buttons for RUN, RUNLOOP, STOP.
- Multiline editor to paste your pseudocode; each line is sent with a trailing newline '\n'.
- Port dropdown + refresh + connect/disconnect.
- Shows device log output (from Serial) in the right pane.

Requirements:
    pip install pyserial

Tested with Python 3.8+ on Windows/macOS/Linux.
"""

import sys
import threading
import time
import queue
from dataclasses import dataclass
from typing import List, Optional

try:
    import serial
    from serial.tools import list_ports
except ImportError as e:
    print("Missing dependency: pyserial\nInstall with: pip install pyserial")
    sys.exit(1)

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext

BAUD = 115200

@dataclass
class PortInfo:
    device: str
    desc: str
    hwid: str

def list_serial_ports() -> List[PortInfo]:
    ports = []
    for p in list_ports.comports():
        ports.append(PortInfo(device=p.device, desc=p.description, hwid=p.hwid))
    return ports

class McuClient:
    def __init__(self):
        self.ser: Optional[serial.Serial] = None
        self.reader_thread: Optional[threading.Thread] = None
        self.stop_event = threading.Event()
        self.rx_queue: "queue.Queue[str]" = queue.Queue()

    def connect(self, port: str, baud: int = BAUD):
        if self.ser and self.ser.is_open:
            return
        try:
            self.ser = serial.Serial(port=port, baudrate=baud, timeout=0.1)
            # DTR/RTS dance can help some boards to start streaming
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
        except Exception as e:
            raise RuntimeError(f"Failed to open {port}: {e}")
        # start reader
        self.stop_event.clear()
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()

    def disconnect(self):
        self.stop_event.set()
        if self.reader_thread and self.reader_thread.is_alive():
            self.reader_thread.join(timeout=1.0)
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None

    def is_connected(self) -> bool:
        return self.ser is not None and self.ser.is_open

    def _reader_loop(self):
        # Read bytes, push lines to queue
        buf = bytearray()
        while not self.stop_event.is_set():
            try:
                if not self.ser:
                    break
                data = self.ser.read(256)
                if data:
                    buf.extend(data)
                    # split on newlines; keep partial in buf
                    while True:
                        nl = buf.find(b'\n')
                        if nl == -1:
                            break
                        line = buf[:nl]
                        # drop optional '\r'
                        if line.endswith(b'\r'):
                            line = line[:-1]
                        try:
                            text = line.decode('utf-8', errors='replace')
                        except Exception:
                            text = repr(line)
                        self.rx_queue.put(text)
                        buf = buf[nl+1:]
                else:
                    time.sleep(0.01)
            except Exception as e:
                self.rx_queue.put(f"[Reader error] {e}")
                time.sleep(0.2)

    def send_line(self, line: str):
        if not self.is_connected():
            raise RuntimeError("Not connected")
        # normalize newline to LF only; device ignores CR
        data = (line + "\n").encode("utf-8")
        self.ser.write(data)
        self.ser.flush()

    def send_lines(self, lines: List[str], delay_ms: int = 30):
        for i, ln in enumerate(lines, 1):
            ln = ln.rstrip("\r\n")
            if ln == "":
                continue
            self.send_line(ln)
            if delay_ms > 0:
                time.sleep(delay_ms / 1000.0)

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ESP32 Pseudocode Sender")
        self.geometry("920x560")
        self.minsize(820, 480)

        self.client = McuClient()
        self.current_ports: List[PortInfo] = []

        self._build_ui()
        self._refresh_ports()
        self.after(50, self._drain_rx_queue)

    def _build_ui(self):
        # Top control bar
        top = ttk.Frame(self, padding=8)
        top.pack(side=tk.TOP, fill=tk.X)

        ttk.Label(top, text="Port:").pack(side=tk.LEFT)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(top, textvariable=self.port_var, state="readonly", width=40)
        self.port_combo.pack(side=tk.LEFT, padx=4)

        self.refresh_btn = ttk.Button(top, text="Refresh", command=self._refresh_ports)
        self.refresh_btn.pack(side=tk.LEFT, padx=4)

        self.connect_btn = ttk.Button(top, text="Connect", command=self._toggle_connect)
        self.connect_btn.pack(side=tk.LEFT, padx=4)

        ttk.Separator(top, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)

        self.run_btn = ttk.Button(top, text="RUN", command=lambda: self._send_quick("RUN"), state=tk.DISABLED)
        self.runloop_btn = ttk.Button(top, text="RUNLOOP", command=lambda: self._send_quick("RUNLOOP"), state=tk.DISABLED)
        self.stop_btn = ttk.Button(top, text="STOP", command=lambda: self._send_quick("STOP"), state=tk.DISABLED)
        self.run_btn.pack(side=tk.LEFT, padx=2)
        self.runloop_btn.pack(side=tk.LEFT, padx=2)
        self.stop_btn.pack(side=tk.LEFT, padx=2)

        ttk.Separator(top, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)

        ttk.Label(top, text="Line delay (ms):").pack(side=tk.LEFT)
        self.delay_var = tk.StringVar(value="30")
        self.delay_entry = ttk.Entry(top, textvariable=self.delay_var, width=6)
        self.delay_entry.pack(side=tk.LEFT, padx=4)

        self.status_var = tk.StringVar(value="Disconnected")
        ttk.Label(top, textvariable=self.status_var).pack(side=tk.RIGHT)

        # Main panes
        main = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        main.pack(fill=tk.BOTH, expand=True, padx=8, pady=8)

        # Left: script editor
        left = ttk.Frame(main)
        ttk.Label(left, text="Pseudocode (each line will be sent with \\n):").pack(anchor="w")
        self.script = scrolledtext.ScrolledText(left, wrap="none", height=20, undo=True)
        self.script.pack(fill=tk.BOTH, expand=True, pady=(4, 4))

        send_row = ttk.Frame(left)
        send_row.pack(fill=tk.X, pady=(0, 4))
        self.send_btn = ttk.Button(send_row, text="Send Lines", command=self._send_script, state=tk.DISABLED)
        self.send_btn.pack(side=tk.LEFT)
        ttk.Label(send_row, text="Tip: You can also send single commands in the editor and click Send Lines.").pack(side=tk.LEFT, padx=8)

        main.add(left, weight=3)

        # Right: log output
        right = ttk.Frame(main)
        ttk.Label(right, text="Device Log (Serial RX):").pack(anchor="w")
        self.log = scrolledtext.ScrolledText(right, wrap="word", height=20, state=tk.DISABLED)
        self.log.pack(fill=tk.BOTH, expand=True, pady=(4, 4))

        clear_row = ttk.Frame(right)
        clear_row.pack(fill=tk.X)
        ttk.Button(clear_row, text="Clear Log", command=self._clear_log).pack(side=tk.LEFT)
        main.add(right, weight=2)

        # Footer
        footer = ttk.Frame(self, padding=(8, 0, 8, 8))
        footer.pack(fill=tk.X)
        ttk.Label(footer, text="Baud: 115200 • Newline: LF (\\n)").pack(side=tk.LEFT)

        # Styling niceties
        for w in (self.run_btn, self.runloop_btn, self.stop_btn, self.send_btn, self.connect_btn, self.refresh_btn):
            w.configure(width=10)

    def _refresh_ports(self):
        self.current_ports = list_serial_ports()
        display = [f"{p.device} — {p.desc}" for p in self.current_ports]
        self.port_combo["values"] = display
        if display and not self.port_var.get():
            self.port_combo.current(0)

    def _toggle_connect(self):
        if self.client.is_connected():
            self.client.disconnect()
            self._set_connected(False)
            return

        idx = self.port_combo.current()
        if idx < 0 or idx >= len(self.current_ports):
            messagebox.showwarning("Select port", "Please select a serial port first.")
            return
        port = self.current_ports[idx].device
        try:
            self.client.connect(port, BAUD)
        except Exception as e:
            messagebox.showerror("Connection failed", str(e))
            return
        self._set_connected(True)

    def _set_connected(self, connected: bool):
        self.connect_btn.configure(text="Disconnect" if connected else "Connect")
        for b in (self.run_btn, self.runloop_btn, self.stop_btn, self.send_btn):
            b.configure(state=(tk.NORMAL if connected else tk.DISABLED))
        self.status_var.set(f"Connected to {self.current_ports[self.port_combo.current()].device}" if connected else "Disconnected")

    def _send_quick(self, word: str):
        try:
            self.client.send_line(word)
            self._append_log(f">>> {word}")
        except Exception as e:
            messagebox.showerror("Send failed", str(e))

    def _send_script(self):
        text = self.script.get("1.0", tk.END)
        lines = [ln.rstrip("\r\n") for ln in text.splitlines()]
        try:
            delay_ms = int(self.delay_var.get())
        except ValueError:
            delay_ms = 30
            self.delay_var.set("30")
        try:
            self.client.send_lines(lines, delay_ms=delay_ms)
            # Echo what we sent
            for ln in lines:
                if ln.strip():
                    self._append_log(f">>> {ln}")
        except Exception as e:
            messagebox.showerror("Send failed", str(e))

    def _append_log(self, text: str):
        self.log.configure(state=tk.NORMAL)
        self.log.insert(tk.END, text + "\n")
        self.log.see(tk.END)
        self.log.configure(state=tk.DISABLED)

    def _clear_log(self):
        self.log.configure(state=tk.NORMAL)
        self.log.delete("1.0", tk.END)
        self.log.configure(state=tk.DISABLED)

    def _drain_rx_queue(self):
        try:
            while True:
                line = self.client.rx_queue.get_nowait()
                self._append_log(line)
        except queue.Empty:
            pass
        # Schedule next drain
        self.after(50, self._drain_rx_queue)

def main():
    app = App()
    app.mainloop()

if __name__ == "__main__":
    main()
