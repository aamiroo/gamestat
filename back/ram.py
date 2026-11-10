import psutil

def ram_info ():
    memoy = psutil.virtual_memory()

    return{
        "total" : round(memoy.total / (1024 ** 3), 2),
        "used" : round(memoy.used / (1024 ** 3) , 2),
        "available" : round(memoy.available / (1024 ** 3) , 2),
        "percent" : memoy.percent
    }

if __name__ == "__main__":
    ram = ram_info()

    print(f"RAM Total: {ram['total']} GB")
    print(f"RAM Used: {ram['used']} GB")
    print(f"RAM Available: {ram['available']} GB")
    print(f"RAM Usage: {ram['percent']} %")