import unittest
from gamestat_capture import FrameAnalyzer, FrameEvent

class AnalyzerTests(unittest.TestCase):
    def test_approx_60_fps(self):
        analyzer = FrameAnalyzer(window_seconds=5)
        stats = None
        base = 10_000_000_000
        for i in range(121):
            event = FrameEvent(1, "test", "swap_buffers", base + i * 16_666_667,
                               base + i * 16_666_667 + 100_000, 0, 1, 123, 42)
            stats = analyzer.add(event) or stats
        self.assertIsNotNone(stats)
        self.assertAlmostEqual(stats.fps, 60.0, delta=0.2)
        self.assertAlmostEqual(stats.average_fps, 60.0, delta=0.2)

    def test_first_event_only_seeds_clock(self):
        analyzer = FrameAnalyzer()
        event = FrameEvent(1, "test", "queue_present", 1_000_000, 1_100_000, 0, 1, 123, 42)
        self.assertIsNone(analyzer.add(event))

if __name__ == "__main__":
    unittest.main()
