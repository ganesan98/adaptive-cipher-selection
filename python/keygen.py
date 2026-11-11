"""Create a fresh 256-bit demo pre-shared key; refuse to overwrite."""
import argparse
import os
import secrets
from pathlib import Path

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", nargs="?", type=Path, default=Path("demo.key"))
    args = parser.parse_args()
    fd = os.open(args.path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w", encoding="ascii") as stream:
        stream.write(secrets.token_hex(32) + "\n")
    print(f"Created {args.path}. Keep this file private; do not commit it.")
