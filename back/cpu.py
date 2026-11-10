from pathlib import Path
import psutil


def cpu_temp():
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


def cpu_usage ():
    return psutil.cpu_percent(interval=1)

if __name__ == "__main__":
    print(f"CPU Temp: {cpu_temp()} °C")
    print(f"CPU Usage: {cpu_usage()} %")

    