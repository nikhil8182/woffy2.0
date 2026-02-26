#!/usr/bin/env python3
import serial
import time
import sys
import os
import signal

# Kill any existing processes using the serial port
os.system('lsof -t /dev/cu.usbserial-0001 2>/dev/null | xargs kill -9 2>/dev/null')
os.system('lsof -t /dev/tty.usbserial-0001 2>/dev/null | xargs kill -9 2>/dev/null')
time.sleep(0.5)

ports = ['/dev/cu.usbserial-0001', '/dev/cu.usbserial-0002', '/dev/tty.usbserial-0001']

for port in ports:
    try:
        s = serial.Serial(port, 115200, timeout=0.5)
        print(f"=== Connected to {port} ===")
        print("Press Ctrl+C to exit")
        print("")
        
        while True:
            if s.in_waiting:
                line = s.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(line)
            time.sleep(0.01)
    except serial.serialutil.SerialException as e:
        if "No such file" not in str(e):
            print(f"{port}: {e}")
        continue
    except KeyboardInterrupt:
        print("\n--- Monitor closed ---")
        try:
            s.close()
        except:
            pass
        break
    except Exception as e:
        print(f"Error: {e}")
        continue

print("No device found!")
