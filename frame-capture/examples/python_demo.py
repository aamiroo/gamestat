from gamestat_capture import FrameAnalyzer, FrameCollector

collector = FrameCollector()
analyzer = FrameAnalyzer(window_seconds=5)
print("Listening for GameStat frame events. Start a captured game; Ctrl+C to stop.")
try:
    with collector:
        for event in collector.iter_events():
            stats = analyzer.add(event)
            if stats:
                print(
                    f"{stats.api:12} FPS {stats.fps:7.1f} | avg {stats.average_fps:7.1f} | "
                    f"frame {stats.frame_time_ms:5.2f} ms | 1% low {stats.fps_1_low:6.1f}"
                )
except KeyboardInterrupt:
    pass
