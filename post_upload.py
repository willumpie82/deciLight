#!/usr/bin/env python3
"""
Post-upload script to force application boot on ESP32-S3
Fixes issue where application doesn't start after upload without power cycle
"""

import sys
import time
import subprocess

def run_app():
    """Force the device to exit bootloader and run application"""
    try:
        print("[POST-UPLOAD] Forcing device to start application...")
        result = subprocess.run([
            "/Users/willemoldemans/.platformio/penv/bin/esptool.py",
            "--port", "/dev/cu.usbmodem101",
            "--baud", "115200",
            "--after", "hard_reset",
            "run"
        ], capture_output=True, text=True, timeout=15)
        
        if result.returncode == 0:
            print("[POST-UPLOAD] ✓ Device reset successful")
            time.sleep(2)
            print("[POST-UPLOAD] Device should be running now")
        else:
            print(f"[POST-UPLOAD] esptool failed: {result.stderr}")
            return False
    except Exception as e:
        print(f"[POST-UPLOAD] Error: {e}")
        return False
    
    return True

if __name__ == "__main__":
    # Only run if this is actually a post-upload scenario
    success = run_app()
    sys.exit(0 if success else 1)
