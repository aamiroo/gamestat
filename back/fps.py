from dataclasses import asdict
import threading
import time

from gamestat_capture import FrameCollector, FrameAnalyzer

_lock = threading.Lock()
_collector = None
_analyzer = None
_latest_status = None
_latest_event_time = 0.0

def _on_event(event):
    global _latest_status, _latest_event_time
    with _lock:
        status = _analyzer.add(event)

        if status is not None:
        
            _latest_status = asdict(status)
            _latest_event_time = time.monotonic()

def start_capture ():
    global _collector , _analyzer

    with _lock:
        if _collector is not None:
            return

        _analyzer = FrameAnalyzer()
        _collector = FrameCollector(on_event=_on_event)
        _collector.start()

def fps_info():

    start_capture()

    with _lock:
        if (
            _latest_status is None or time.monotonic() - _latest_event_time > 2):
            return{
                "available": False,
                "fps": None,
                "average_fps": None,
                "frame_time_ms": None,
                "p99_frame_time_ms": None,
                "p999_frame_time_ms": None,
                "fps_1_low": None,
                "fps_01_low": None,
            }


        return {
            "available": True,
            **_latest_status
        }