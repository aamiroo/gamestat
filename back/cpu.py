from pathlib import Path
import psutil

# CPU metrics module - reads CPU temperature and usage from system files


def cpu_temp():

    """ Read CPU package temperature from hwmon.
        Returns:
        float | None: Temperature in Celsius, or None if unavailable."""
    
    hwmon_path = Path("/sys/class/hwmon")
    try:
        for hwmon in hwmon_path.glob("hwmon*"):
            name = (hwmon / "name").read_text().strip()

            if name != "coretemp":
                continue

            for label_file in hwmon.glob("temp*_label"):
                label = label_file.read_text().strip()


                if label == "Package id 0":
                    input_file = label_file.with_name(
                        label_file.name.replace("_label" , "_input")
                    )
                    temp = int (input_file.read_text().strip())
        
                    return temp / 1000
        return None
    except (OSError , ValueError) as error:
        print(f"[CPU TEMP ERROR] {error}")
        return None


def cpu_usage():

    """ Measure CPU usage percentage.
        Returns:
        float: CPU usage percentage (0-100)."""
    try:
        return psutil.cpu_percent(interval=1)
    except (OSError , ValueError) as error:
        print(f"[CPU USAGE ERROR] {error}")
        return None

    
if __name__ == "__main__":
    print(f"CPU Temp: {cpu_temp()} °C")
    print(f"CPU Usage: {cpu_usage()} %")

    