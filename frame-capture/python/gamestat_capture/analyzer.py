"""Small rolling frame-time analyzer. Results are estimates from intercepted API calls."""
from __future__ import annotations
from collections import deque
from dataclasses import dataclass
from statistics import mean
from typing import Deque
from .collector import FrameEvent

@dataclass(frozen=True, slots=True)
class FrameStats:
    api: str
    pid: int
    source_id: int
    samples: int
    fps: float
    average_fps: float
    frame_time_ms: float
    p99_frame_time_ms: float
    p999_frame_time_ms: float
    fps_1_low: float
    fps_01_low: float

class FrameAnalyzer:
    def __init__(self, window_seconds: float = 5.0, max_samples: int = 3000) -> None:
        self.window_ns = int(window_seconds * 1_000_000_000)
        self.max_samples = max_samples
        self._last: dict[tuple[str, int, int], int] = {}
        self._times: dict[tuple[str, int, int], Deque[tuple[int, float]]] = {}

    def add(self, event: FrameEvent) -> FrameStats | None:
        if event.event not in ("queue_present", "swap_buffers") or event.call_start_ns <= 0:
            return None
        api = event.api
        stream = (api, event.pid, event.source_id)
        previous = self._last.get(stream)
        self._last[stream] = event.call_start_ns
        if previous is None or event.call_start_ns <= previous:
            return None
        delta_ms = (event.call_start_ns - previous) / 1_000_000.0
        if delta_ms <= 0 or delta_ms > 10_000:
            return None
        samples = self._times.setdefault(stream, deque())
        samples.append((event.call_start_ns, delta_ms))
        cutoff = event.call_start_ns - self.window_ns
        while samples and (samples[0][0] < cutoff or len(samples) > self.max_samples):
            samples.popleft()
        values = sorted(v for _, v in samples)
        if not values:
            return None
        p99 = self._percentile(values, 0.99)
        p999 = self._percentile(values, 0.999)
        elapsed = (samples[-1][0] - samples[0][0]) / 1e9
        avg_fps = (len(samples) - 1) / elapsed if elapsed > 0 else 0.0
        return FrameStats(
            api=api,
            pid=event.pid,
            source_id=event.source_id,
            samples=len(values),
            fps=1000.0 / delta_ms,
            average_fps=avg_fps,
            frame_time_ms=mean(values),
            p99_frame_time_ms=p99,
            p999_frame_time_ms=p999,
            fps_1_low=1000.0 / p99 if p99 else 0.0,
            fps_01_low=1000.0 / p999 if p999 else 0.0,
        )

    @staticmethod
    def _percentile(sorted_values: list[float], quantile: float) -> float:
        if not sorted_values:
            return 0.0
        index = min(len(sorted_values) - 1, max(0, int(round((len(sorted_values) - 1) * quantile))))
        return sorted_values[index]
