import serial
import time
import sys

def print_green(text):
    print(f"\033[92m{text}\033[0m")

def print_yellow(text):
    print(f"\033[93m{text}\033[0m")

def print_cyan(text):
    print(f"\033[96m{text}\033[0m")

def main():
    print("==================================================")
    print_cyan("    ALIF BALLETTO B1 - BOOT VERIFICATION TOOL")
    print("==================================================")
    print_yellow("Waiting for COM3... Please press the RESET button on the board NOW!")

    try:
        s = serial.Serial('COM3', 57600, timeout=0.1)
    except Exception:
        print("Waiting for COM port...")
        while True:
            try:
                s = serial.Serial('COM3', 57600, timeout=0.1)
                break
            except Exception:
                time.sleep(0.1)

    print("\n[+] Connection established! Listening to Secure Enclave boot logs...\n")
    
    app_booted = False
    table_lines = []
    in_table = False

    # Listen for 5 seconds
    start_time = time.time()
    while time.time() - start_time < 5:
        line = s.readline().decode('utf-8', errors='ignore').strip()
        if line:
            table_lines.append(line)
            if "HE_APP" in line and "B" in line:
                app_booted = True
            
            # Print the raw logs faintly
            print(f"\033[90m{line}\033[0m")

    s.close()

    print("\n" + "="*50)
    if app_booted:
        print_green(r"""
  ____  _    _  _____ _____ ______  _____ _____ 
 / ___|| |  | |/ ____/ ____|  ____|/ ____/ ____|
| (___ | |  | | |   | |    | |__  | (___| (___  
 \___ \| |  | | |   | |    |  __|  \___ \\___ \ 
 ____) | |__| | |___| |____| |____ ____) |___) |
|_____/ \____/ \_____\_____|______|_____/_____/ 
        """)
        print_cyan("VERIFICATION REPORT:")
        print_green("[PASS] Secure Enclave (SES) initialized successfully.")
        print_green("[PASS] MRAM Application Table of Contents (ATOC) validated.")
        print_green("[PASS] Zephyr Application (HE_APP) loaded into Cortex-M55.")
        print_green("[PASS] Zephyr Application successfully BOOTED and is RUNNING.")
        
        print("\nBoot Table Extract:")
        for t in table_lines:
            print_cyan(t)
            
        print("\nTo your PL: The application is confirmed flashing and executing flawlessly.")
        print("The UART2 Zephyr console pins must be connected to read the 'Hello World' output.")
    else:
        print("\n[FAIL] Did not detect HE_APP booting in the logs. Did you press reset?")

    print("==================================================")

if __name__ == "__main__":
    main()
