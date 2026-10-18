import argparse
import fcntl
import json
import logging
import signal
import sys

from .config import load, state_dir, validate


def main():
    parser = argparse.ArgumentParser(description="Pico Assistant for Ubuntu Linux")
    parser.add_argument("--config", help="Configuration JSON (default: ~/.config/pico-assistant/config.json)")
    parser.add_argument("mode", nargs="?", default="gui", choices=("gui", "agent", "diagnose", "snapshot", "check"))
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    try:
        if args.mode == "gui":
            from .gui import launch
            launch(args.config)
        elif args.mode == "diagnose":
            from .hardware import diagnostics
            print(json.dumps(diagnostics(), indent=2))
        elif args.mode == "snapshot":
            import time
            from .hardware import Hardware
            monitor = Hardware(load(args.config)["networkInterface"])
            time.sleep(0.2)
            print(json.dumps(monitor.read(), indent=2, allow_nan=False))
        else:
            config = load(args.config)
            validate(config)
            if args.mode == "check":
                print("Configuration valid; TLS verification enabled")
                return 0
            from .agent import Agent
            directory = state_dir()
            directory.mkdir(parents=True, exist_ok=True, mode=0o700)
            # A second agent would fight over retained presence and duplicate commands.
            with (directory / "agent.lock").open("w") as lock:
                try:
                    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError:
                    raise RuntimeError("A Linux agent is already running for this user") from None
                agent = Agent(config)
                for sig in (signal.SIGTERM, signal.SIGINT):
                    signal.signal(sig, lambda *_: agent.stop_event.set())
                agent.run()
        return 0
    except (ValueError, OSError, RuntimeError, ImportError) as error:
        print(f"Pico Assistant: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
