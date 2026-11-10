import pynvml

def init_gpu ():
    pynvml.nvmlInit()


    handle = pynvml.nvmlDeviceGetHandleByIndex(0)

    return handle


def gpu_temp (handle):
    return pynvml.nvmlDeviceGetTemperature(
        handle,
        pynvml.NVML_TEMPERATURE_GPU
    )
def gpu_usage (handle):
    utilization = pynvml.nvmlDeviceGetUtilizationRates(handle)

    return utilization.gpu

def vram_info (handle):
    memory = pynvml.nvmlDeviceGetMemoryInfo(handle)

    return {
        "total" : round(memory.total / (1024 ** 3), 2),
        "used" : round(memory.used / (1024 ** 3), 2),
        "free" : round(memory.free / (1024 ** 3), 2),
        "percent" : round((memory.used / memory.total) * 100, 2)
    }

def gpu_memory_usage(handle):
    utilization = pynvml.nvmlDeviceGetUtilizationRates(handle)

    return utilization.memory
def gpu_clock(handle):
    return pynvml.nvmlDeviceGetClockInfo(
        handle,
        pynvml.NVML_CLOCK_GRAPHICS
    )
def gpu_power(handle):
    power = pynvml.nvmlDeviceGetPowerUsage(handle)

    return power / 1000


if __name__ == "__main__":
    pynvml.nvmlInit()

    try:
        gpu = pynvml.nvmlDeviceGetHandleByIndex(0)

        name = pynvml.nvmlDeviceGetName(gpu)
        vram = vram_info(gpu)

        print(f"GPU Name: {name}")
        print(f"GPU Temperature: {gpu_temp(gpu)} °C")
        print(f"GPU Usage: {gpu_usage(gpu)} %")

        print(f"VRAM Total: {vram['total']} GB")
        print(f"VRAM Used: {vram['used']} GB")
        print(f"VRAM Free: {vram['free']} GB")
        print(f"VRAM Usage: {vram['percent']} %")

        print(f"GPU Memory Controller: {gpu_memory_usage(gpu)} %")
        print(f"GPU Clock: {gpu_clock(gpu)} MHz")
        print(f"GPU Power: {gpu_power(gpu):.2f} W")

    finally:
        pynvml.nvmlShutdown()