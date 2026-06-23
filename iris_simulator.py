import tkinter as tk
from tkinter import ttk
import serial
import serial.tools.list_ports
import threading
import time

class IrisSimulatorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Alif Balletto B1 - Real-Time NPU Simulator")
        self.root.geometry("600x500")
        self.root.configure(bg="#1E1E1E")
        
        # Style configuration
        style = ttk.Style()
        style.theme_use('clam')
        style.configure("TLabel", background="#1E1E1E", foreground="#FFFFFF", font=("Segoe UI", 12))
        style.configure("Header.TLabel", font=("Segoe UI", 24, "bold"), foreground="#00E5FF")
        style.configure("Result.TLabel", font=("Segoe UI", 20, "bold"), foreground="#FF007F")
        style.configure("TFrame", background="#1E1E1E")
        
        self.serial_port = None
        self.serial_thread = None
        self.running = True
        self.last_sent_time = 0
        
        self.setup_ui()
        self.connect_serial()
        
    def setup_ui(self):
        main_frame = ttk.Frame(self.root, padding="20")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(main_frame, text="Iris TinyML Simulator", style="Header.TLabel").pack(pady=(0, 20))
        
        # Connection status
        self.status_var = tk.StringVar(value="Status: Disconnected")
        ttk.Label(main_frame, textvariable=self.status_var, font=("Segoe UI", 10)).pack(pady=(0, 20))
        
        # Sliders Frame
        sliders_frame = ttk.Frame(main_frame)
        sliders_frame.pack(fill=tk.X, pady=10)
        
        self.vars = {
            "Sepal Length": tk.DoubleVar(value=5.1),
            "Sepal Width": tk.DoubleVar(value=3.5),
            "Petal Length": tk.DoubleVar(value=1.4),
            "Petal Width": tk.DoubleVar(value=0.2)
        }
        
        ranges = {
            "Sepal Length": (4.0, 8.0),
            "Sepal Width": (2.0, 5.0),
            "Petal Length": (1.0, 7.0),
            "Petal Width": (0.1, 3.0)
        }
        
        for name, var in self.vars.items():
            frame = ttk.Frame(sliders_frame)
            frame.pack(fill=tk.X, pady=5)
            
            lbl = ttk.Label(frame, text=f"{name}:", width=15)
            lbl.pack(side=tk.LEFT)
            
            val_lbl = ttk.Label(frame, text="0.0", width=5)
            val_lbl.pack(side=tk.RIGHT)
            
            def update_lbl(event, v=var, l=val_lbl):
                l.config(text=f"{v.get():.1f}")
                self.trigger_inference()
                
            slider = ttk.Scale(frame, from_=ranges[name][0], to=ranges[name][1], 
                               variable=var, orient=tk.HORIZONTAL, command=update_lbl)
            slider.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=10)
            
            # Initial text
            val_lbl.config(text=f"{var.get():.1f}")
            
        # Result Frame
        result_frame = ttk.Frame(main_frame, relief=tk.GROOVE, padding="20")
        result_frame.pack(fill=tk.BOTH, expand=True, pady=20)
        
        ttk.Label(result_frame, text="Ethos-U55 NPU Prediction:", font=("Segoe UI", 12, "italic")).pack()
        self.result_var = tk.StringVar(value="Waiting for inference...")
        self.result_lbl = ttk.Label(result_frame, textvariable=self.result_var, style="Result.TLabel")
        self.result_lbl.pack(pady=10)

    def connect_serial(self):
        try:
            self.serial_port = serial.Serial("COM3", 57600, timeout=1)
            self.status_var.set("Status: Connected to COM3")
            
            self.serial_thread = threading.Thread(target=self.read_serial_loop)
            self.serial_thread.daemon = True
            self.serial_thread.start()
        except Exception as e:
            self.status_var.set(f"Status: Failed to connect COM3 ({str(e)})")
            self.root.after(3000, self.connect_serial) # Retry in 3 seconds

    def read_serial_loop(self):
        while self.running and self.serial_port and self.serial_port.is_open:
            try:
                line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    if line.startswith("RESULT:"):
                        res = line.replace("RESULT:", "")
                        self.root.after(0, self.update_result, res)
                    else:
                        print(f"Board: {line}")
            except Exception as e:
                print(f"Serial read error: {e}")
                time.sleep(1)

    def update_result(self, res):
        # res format: "Setosa:96.5"
        parts = res.split(":")
        if len(parts) == 2:
            species = parts[0]
            prob = parts[1]
            self.result_var.set(f"{species} ({prob}%)")
            
            # Change color based on species
            if species == "Setosa":
                self.result_lbl.configure(foreground="#FF3333") # Red
            elif species == "Versicolor":
                self.result_lbl.configure(foreground="#33FF33") # Green
            elif species == "Virginica":
                self.result_lbl.configure(foreground="#3333FF") # Blue
        else:
            self.result_var.set(res)

    def trigger_inference(self):
        # Rate limit to avoid flooding
        now = time.time()
        if now - self.last_sent_time < 0.1: # Max 10 Hz
            return
        self.last_sent_time = now
        
        if self.serial_port and self.serial_port.is_open:
            f1 = self.vars["Sepal Length"].get()
            f2 = self.vars["Sepal Width"].get()
            f3 = self.vars["Petal Length"].get()
            f4 = self.vars["Petal Width"].get()
            
            cmd = f"INFER {f1:.1f} {f2:.1f} {f3:.1f} {f4:.1f}\n"
            try:
                self.serial_port.write(cmd.encode('utf-8'))
            except Exception as e:
                print(f"Failed to send: {e}")

    def on_close(self):
        self.running = False
        if self.serial_port:
            self.serial_port.close()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = IrisSimulatorApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()
