import pynvml
# GPU metrics module - reads NVIDIA GPU info via pynvml

def init_gpu ():
    # Initialize NVML and get GPU handle
    try:
        pynvml.nvmlInit()
        handle = pynvml.nvmlDeviceGetHandleByIndex(0)
        return handle
    
    except pynvml.NVMLError as error:
        print(f"[GPU INIT ERROR] {error}")
        return None


def gpu_temp(handle):

    try:
        return pynvml.nvmlDeviceGetTemperature(
            handle,
            pynvml.NVML_TEMPERATURE_GPU
        )
    except pynvml.NVMLError as error:
        print(f"[GPU TEMP ERROR] {error}")
        return None
    
def gpu_usage(handle):

    try:
        utilization = pynvml.nvmlDeviceGetUtilizationRates(handle)

        return utilization.gpu
    except pynvml.NVMLError as error:
        print(f"[GPU USAGE ERROR] {error}")
        return None
        
def vram_info (handle):

    try:
        memory = pynvml.nvmlDeviceGetMemoryInfo(handle)

        return {
            "total" : round(memory.total / (1024 ** 3), 2),
            "used" : round(memory.used / (1024 ** 3), 2),
            "free" : round(memory.free / (1024 ** 3), 2),
            "percent" : round((memory.used / memory.total) * 100, 2)
        }
    except pynvml.NVMLError as error:
        print(f"[GPU VRAM INFORMATION ERROR] {error}")
        return None
        
def gpu_memory_usage(handle):

    try:
        utilization = pynvml.nvmlDeviceGetUtilizationRates(handle)

        return utilization.memory
    
    except pynvml.NVMLError as error:
        print(f"[GPU MEMORY USAGE ERROR] {error}")
        return None
        
def gpu_clock(handle):

    try:
        return pynvml.nvmlDeviceGetClockInfo(
            handle,
            pynvml.NVML_CLOCK_GRAPHICS
        )
    except pynvml.NVMLError as error:
        print(f"[GPU CLOCK ERROR] {error}")
        return None
        
def gpu_power(handle):

    try:
        power = pynvml.nvmlDeviceGetPowerUsage(handle)

        return power / 1000
    
    except pynvml.NVMLError as error:
        print(f"[GPU POWER ERROR] {error}")
        return None
        

if __name__ == "__main__":
    nvml_initialized = False

    try:
        gpu = init_gpu()
        if gpu is None:
            exit(1)

        nvml_initialized = True

        name = pynvml.nvmlDeviceGetName(gpu)
        vram = vram_info(gpu)
        power = gpu_power(gpu)

        print(f"GPU Name: {name}")
        print(f"GPU Temperature: {gpu_temp(gpu)} °C")
        print(f"GPU Usage: {gpu_usage(gpu)} %")
    
        if vram is not None:
            print(f"VRAM Total: {vram['total']} GB")
            print(f"VRAM Used: {vram['used']} GB")
            print(f"VRAM Free: {vram['free']} GB")
            print(f"VRAM Usage: {vram['percent']} %")

        print(f"GPU Memory Controller: {gpu_memory_usage(gpu)} %")
        print(f"GPU Clock: {gpu_clock(gpu)} MHz")

        if power is not None:
            print(f"GPU Power: {power:.2f} W")

    finally:
        if nvml_initialized:
            pynvml.nvmlShutdown()