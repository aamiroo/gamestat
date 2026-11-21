import configparser
from pathlib import Path

DEFAULTS= {
    "cpu-temp" : True,
    "cpu-use": True,
    "ram-use" : True,
    "gpu-temp" : True,
    "gpu-use" : True,
    "vram" : True,
    "fps" : True,
}

CONFIG_PATH = Path(__file__).parent / "config"

def load_config ():
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
    except (OSError , configparser.Error):
        pass

    result = {}

    for key, default in DEFAULTS.items():
        try:
            result[key] = config.getboolean(
                "display" , key , fallback=default
            )
        except ValueError:
            result[key] = default
    return result
