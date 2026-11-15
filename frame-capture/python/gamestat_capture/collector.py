"""Unix datagram collector for the GameStat native frame capture libraries."""
from __future__ import annotations

import json
import os
import socket
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterator


@dataclass(frozen=True, slots=True)
class FrameEvent:
    schema: int
    api: str
    event: str
    call_start_ns: int
    call_end_ns: int
    result: int
    presented_swapchain_count: int
    source_id: int = 0
    pid: int = 0
    clock: str = "monotonic_raw"

    @classmethod
    def from_payload(cls, payload: bytes) -> "FrameEvent":
        data = json.loads(payload.decode("utf-8"))
        if data.get("schema") != 1:
            raise ValueError(f"Unsupported event schema: {data.get('schema')!r}")
        return cls(
            schema=1,
            api=str(data["api"]),
            event=str(data["event"]),
            call_start_ns=int(data["call_start_ns"]),
            call_end_ns=int(data["call_end_ns"]),
            result=int(data.get("result", 0)),
            presented_swapchain_count=int(data.get("presented_swapchain_count", 1)),
            source_id=int(data.get("source_id", 0)),
            pid=int(data.get("pid", 0)),
            clock=str(data.get("clock", "unknown")),
        )


class FrameCollector:
    """Receives native capture events on a Unix datagram socket.

    The native hooks are best-effort and never wait for this collector. Start this
    collector before launching a game so the socket exists when the first frame arrives.
    """
    def __init__(self, socket_path: str | os.PathLike[str] = "/tmp/gamestat.sock",
                 on_event: Callable[[FrameEvent], None] | None = None) -> None:
        self.socket_path = Path(socket_path)
        self.on_event = on_event
        self._socket: socket.socket | None = None
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()
        self._events: list[FrameEvent] = []
        self._lock = threading.Lock()
        self.errors = 0

    def start(self) -> "FrameCollector":
        if self._thread and self._thread.is_alive():
            return self
        self.socket_path.parent.mkdir(parents=True, exist_ok=True)
        try:
            self.socket_path.unlink()
        except FileNotFoundError:
            pass
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        sock.bind(str(self.socket_path))
        os.chmod(self.socket_path, 0o600)
        sock.settimeout(0.25)
        self._socket = sock
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, name="gamestat-frame-collector", daemon=True)
        self._thread.start()
        return self

    def _run(self) -> None:
        assert self._socket is not None
        while not self._stop.is_set():
            try:
                payload = self._socket.recv(2048)
            except socket.timeout:
                continue
            except OSError:
                break
            try:
                event = FrameEvent.from_payload(payload)
                with self._lock:
                    self._events.append(event)
                    if len(self._events) > 10000:
                        del self._events[:5000]
                if self.on_event:
                    self.on_event(event)
            except (ValueError, KeyError, TypeError, UnicodeDecodeError, json.JSONDecodeError):
                self.errors += 1

    def drain(self) -> list[FrameEvent]:
        with self._lock:
            events, self._events = self._events, []
        return events

    def close(self) -> None:
        self._stop.set()
        if self._socket:
            self._socket.close()
            self._socket = None
        if self._thread:
            self._thread.join(timeout=1.0)
            self._thread = None
        try:
            self.socket_path.unlink()
        except FileNotFoundError:
            pass

    def __enter__(self) -> "FrameCollector":
        return self.start()

    def __exit__(self, *_: object) -> None:
        self.close()

    def iter_events(self, poll_interval: float = 0.05) -> Iterator[FrameEvent]:
        while not self._stop.is_set():
            events = self.drain()
            if events:
                yield from events
            else:
                time.sleep(poll_interval)
