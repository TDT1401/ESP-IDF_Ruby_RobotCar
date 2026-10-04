"""Build the ST7789 240x240 L298N variant with the 'Hi ESP' wake word.

Run manually inside an initialized ESP-IDF environment. Extra arguments are
forwarded to the canonical build script (for example, --language vi-VN).
"""

import subprocess
import sys
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[3]
    return subprocess.call(
        [
            sys.executable,
            str(root / "scripts" / "build.py"),
            "bread-compact-wifi-lcd",
            "--name",
            "bread-compact-wifi-lcd-st7789-240x240-l298n",
            "--wake-word",
            "wn9_hiesp",
            *sys.argv[1:],
        ],
        cwd=root,
    )


if __name__ == "__main__":
    sys.exit(main())
