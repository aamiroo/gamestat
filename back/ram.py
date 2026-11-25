import psutil

def ram_info ():

    """Get RAM information.
        Returns:
            dict: RAM total, used, available (GB) and usage percentage.
            None: If an error occurs.
"""
    try:
        memoy = psutil.virtual_memory()
    
        return{
            "total" : round(memoy.total / (1024 ** 3), 2),
            "used" : round(memoy.used / (1024 ** 3) , 2),
            "available" : round(memoy.available / (1024 ** 3) , 2),
            "percent" : memoy.percent
        }
    except (OSError , ValueError , RuntimeError) as error:
        print (f"[RAM INFORMATION ERROR] {error}")

if __name__ == "__main__":
    ram = ram_info()

    print(f"RAM Total: {ram['total']} GB")
    print(f"RAM Used: {ram['used']} GB")
    print(f"RAM Available: {ram['available']} GB")
    print(f"RAM Usage: {ram['percent']} %")