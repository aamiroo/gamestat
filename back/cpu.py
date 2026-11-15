from pathlib import Path
import psutil

# CPU metrics module - reads CPU temperature and usage from system files


def cpu_temp():
    """Read CPU temperature from hardware monitoring files.
    Scans /sys/class/hwmon for coretemp node, reads temperature input,
    converts from millidegrees to Celsius.
    Returns:
        float: CPU temperature in Celsius, or None if not found.
    """
    hwmon_path = Path("/sys/class/hwmon")

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


def cpu_usage():
    """Read current CPU usage percentage.

    Uses psutil to sample CPU percent.

    Returns:
        float: CPU usage percentage (0-100).
    """
    return psutil.cpu_percent(interval=1)

if __name__ == "__main__":
    print(f"CPU Temp: {cpu_temp()} °C")
    print(f"CPU Usage: {cpu_usage()} %")

    