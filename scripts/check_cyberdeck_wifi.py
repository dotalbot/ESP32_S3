"""Check a connected board's real survey and subsequent saved-network reconnect."""

import argparse
import re
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbmodem101")
    parser.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("timeout must be positive")
    print("Open Wi-Fi and press Refresh on the connected board.", flush=True)
    count = None
    with serial.Serial(args.port, 115200, timeout=0.25) as port:
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            line = port.readline().decode("utf-8", errors="replace")
            if any(marker in line for marker in ("Guru Meditation", "abort()", "panic")):
                raise SystemExit("FAIL: device crashed")
            if "DominicCyberdeck: Survey failed:" in line:
                raise SystemExit("FAIL: survey failed")
            result = re.search(r"Survey complete: (\d+) networks; reconnecting=(\d)", line)
            if result:
                count = int(result.group(1))
                if count == 0 or result.group(2) != "1":
                    raise SystemExit("FAIL: no networks or board was not connected before scanning")
                print(f"Found {count} networks; waiting for saved-network reconnect.", flush=True)
            if count is not None and "Connected to WiFi:" in line:
                print(f"PASS: {count} networks and saved-network reconnect")
                return
    raise SystemExit("FAIL: no complete survey/reconnect received before timeout")


if __name__ == "__main__":
    main()
