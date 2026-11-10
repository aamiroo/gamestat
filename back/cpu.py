from pathlib import Path
import psutil


def get_cpu_temp():
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

if __name__ == "__main__":
    print(get_cpu_temp())
    