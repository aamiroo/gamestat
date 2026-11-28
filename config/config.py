import configparser
import json
from pathlib import Path

DEFAULTS = {
    "cpu-temp": True,
    "cpu-use": True,
    "ram-use": True,
    "gpu-temp": True,
    "gpu-use": True,
    "gpu-power": True,
    "gpu-clock": True,
    "gpu-mem-ctrl": True,
    "vram": True,
    "fps": True,
}

POSITIONS = ("top-left", "top-right", "bottom-right", "bottom-left")
DEFAULT_POSITION = "top-left"

CONFIG_PATH = Path(__file__).parent / "config"


def load_config():
    config = configparser.ConfigParser()

    config.read_dict({
        "display": {
            key: str(value).lower()
            for key, value in DEFAULTS.items()
        }
    })
    try:
        if CONFIG_PATH.exists():
            config.read(CONFIG_PATH)
    except (OSError, configparser.Error):
        pass

    result = {}

    for key, default in DEFAULTS.items():
        try:
            result[key] = config.getboolean("display", key, fallback=default)
        except ValueError:
            result[key] = default

    position = config.get("display", "position", fallback=DEFAULT_POSITION).strip().lower()
    result["position"] = position if position in POSITIONS else DEFAULT_POSITION
    return result
