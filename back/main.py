import cpu
import ram
import gpu
import json
import time

# System information collector module
# Gathers CPU, RAM, and GPU metrics from the system and returns them as a JSON object.


def sys_info():
    
    """Collects CPU, RAM, and GPU metrics.
    Initializes GPU handle, then gathers temperature and usage data.
    Returns:
        dict: System metrics organized by component (cpu, ram, gpu).
    """

    gpu_handle = gpu.init_gpu()

    try:
        return{

            "cpu": {
                "temp": cpu.cpu_temp(),
                "usage": cpu.cpu_usage()
            },
            "ram": ram.ram_info(),
            "gpu": {
                "temp": gpu.gpu_temp(gpu_handle),
                "usage": gpu.gpu_usage(gpu_handle),
                "vram": gpu.vram_info(gpu_handle),
                "memory_usage": gpu.gpu_memory_usage(gpu_handle),
                "clock": gpu.gpu_clock(gpu_handle),
                "power": gpu.gpu_power(gpu_handle)
        }
        }
    finally:
        gpu.pynvml.nvmlShutdown()

if __name__ == "__main__":
    while True:

        info = sys_info()

        print(json.dumps(info, indent=4))
        # Wait 1 second before next reading
        time.sleep(1)