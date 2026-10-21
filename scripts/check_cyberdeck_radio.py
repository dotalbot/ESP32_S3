"""Check startup without spoken setup, Radio screen retention and USB audio delivery."""

import argparse
import re
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbmodem101")
    parser.add_argument("--station", type=int, choices=range(4), default=0,
                        help="0 Groove Salad, 1 Drone Zone, 2 Secret Agent, 3 BBC World Service")
    parser.add_argument("--quiet-boot", action="store_true",
                        help="Reboot and verify voice activation is deferred before playing audio")
    parser.add_argument("--screen-only", action="store_true",
                        help="Open the real Radio screen and check that it stays visible")
    parser.add_argument("--home-only", action="store_true",
                        help="Check the new Home controls, layout and screen retention")
    parser.add_argument("--chime-only", action="store_true",
                        help="Play the startup chime without rebooting or changing volume")
    parser.add_argument("--startup-only", action="store_true",
                        help="Reboot and check the two-second duck, chime and home return")
    args = parser.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=0.25)
    # Avoid a modem-line reset; native USB can still restart when attaching.
    port.dtr = False
    port.rts = False
    port.port = args.port
    port.open()
    with port:
        port.reset_input_buffer()
        def command(text):
            port.write((text + "\n").encode())
            port.flush()

        def wait(pattern, timeout=45):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                line = port.readline().decode("utf-8", errors="replace")
                if any(marker in line for marker in ("Guru Meditation", "abort()", "panic")):
                    # Capture only the crash trace, ending before boot/network credentials.
                    with open("/tmp/cyberdeck-radio-crash.log", "w") as trace:
                        trace.write(line)
                        until = time.monotonic() + 2
                        while time.monotonic() < until:
                            detail = port.readline().decode("utf-8", errors="replace")
                            trace.write(detail)
                            if "ELF file SHA256" in detail or "Rebooting" in detail:
                                break
                    raise RuntimeError("Device crashed")
                if (args.quiet_boot or args.startup_only) and any(marker in line for marker in (
                    "Alert [link]", "Application: Activating...", "State: starting -> activating"
                )):
                    raise RuntimeError("Unexpected voice activation at startup")
                if "CyberdeckRadio:" in line:
                    # Only radio diagnostics; never print SSIDs or other startup data.
                    print(line[line.index("CyberdeckRadio:"):].strip(), flush=True)
                    if "Radio screen missing" in line:
                        raise RuntimeError("Radio screen left or controls missing")
                    if any(marker in line for marker in (
                        "Voice/device is busy", "Speaker test failed"
                    )):
                        raise RuntimeError("Audio blocked or stalled")
                match = re.search(pattern, line)
                if "CyberdeckStartup:" in line:
                    print(line[line.index("CyberdeckStartup:"):].strip(), flush=True)
                if "CyberdeckHome:" in line:
                    print(line[line.index("CyberdeckHome:"):].strip(), flush=True)
                if match:
                    return match
            raise RuntimeError(f"Timed out waiting for {pattern}")

        try:
            if args.home_only or args.screen_only:
                # Startup replaces screens; wait for its real completion, not a fixed delay.
                command("radio reboot")
                wait("Application: Rebooting...", 30)
                wait(r"Duck complete: \d+ ms; home ready", 45)
            if args.home_only:
                command("home screen")
                wait(r"Home screen ready: clock=\w+; weather=\w+; wifi=\w+; layout=fit", 15)
                time.sleep(3)
                command("home screen status")
                wait(r"Home screen ready: clock=\w+; weather=\w+; wifi=\w+; layout=fit", 15)
                print("PASS: Home controls fit and screen stayed visible; touch is a separate check", flush=True)
                return
            if args.chime_only:
                command("radio chime")
                wait("Startup chime complete", 15)
                print("PASS: startup chime playback completed; listening is a separate check", flush=True)
                return
            if args.screen_only:
                command("radio screen")
                wait("Radio screen ready", 15)
                time.sleep(2)
                command("radio screen status")
                wait("Radio screen ready", 10)
                print("PASS: Radio controls created and screen stayed visible", flush=True)
                return
            if args.quiet_boot or args.startup_only:
                command("radio reboot")
                wait("Voice activation deferred until Talk", 25)
                duck_pattern = r"Duck complete: (\d+) ms; home ready"
                pending = {re.escape("Startup chime complete"), duck_pattern}
                if not args.startup_only:
                    pending.add(re.escape("Application: Network connected"))
                while pending:
                    match = wait("|".join(pending), 45)
                    matched = next(pattern for pattern in pending if re.fullmatch(pattern, match.group(0)))
                    pending.remove(matched)
                    if matched == duck_pattern:
                        duration = int(re.search(r"(\d+) ms", match.group(0)).group(1))
                        if not 2000 <= duration <= 2300:
                            raise RuntimeError(f"Startup animation duration unexpected: {duration} ms")
                print("PASS: two-second duck completed, home returned, chime completed; voice activation deferred", flush=True)
                if args.startup_only:
                    return
            command("radio test")
            wait("Speaker test complete", 20)
            for iteration in range(2):
                command(f"radio play {args.station}")
                wait(r"MP3 (\d+) Hz, [12] channels -> 24000 Hz mono")
                samples = int(wait(r"Playback verified: (\d+) PCM samples queued").group(1))
                if samples < 100000:
                    raise RuntimeError("Too few decoded samples")
                command("radio stop")
                wait(r"CyberdeckRadio: Stopped\.\s*$", 12)
                print(f"PASS: stream {iteration + 1}, {samples} samples, stop completed", flush=True)
        finally:
            try:
                command("radio stop")
            except serial.SerialException:
                pass  # Preserve the original error if USB vanished during a check.
    print("PASS: tone playback completed; MP3 streaming, stop and restart passed. Listening is a separate check.")


if __name__ == "__main__":
    main()
