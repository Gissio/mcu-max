#!/usr/bin/env python3
"""
Measures mcu-max's linked code size on Cortex-M0 (-Os, unused functions
removed by the linker), for an application using every non-FEN function
(size_app.c), and checks it against a budget.

Usage: size.py [--budget BYTES] [--verbose]

Requires: arm-none-eabi-gcc
"""

import argparse
import os
import subprocess
import sys
import tempfile

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(TESTS_DIR, "..", "src")

FLAGS = ["-Os", "-mcpu=cortex-m0", "-mthumb",
         "-ffunction-sections", "-fdata-sections", "-I" + SRC_DIR]


def run(args):
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode:
        sys.exit(result.stdout + result.stderr)
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--budget", type=int)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as build:
        lib = os.path.join(build, "mcu-max.o")
        app = os.path.join(build, "app.o")
        elf = os.path.join(build, "app.elf")

        run(["arm-none-eabi-gcc", *FLAGS, "-c",
             os.path.join(SRC_DIR, "mcu-max.c"), "-o", lib])
        run(["arm-none-eabi-gcc", *FLAGS, "-c",
             os.path.join(TESTS_DIR, "size_app.c"), "-o", app])
        run(["arm-none-eabi-gcc", *FLAGS, "-Wl,--gc-sections",
             "--specs=nosys.specs", app, lib, "-o", elf])

        # Code and read-only data of mcu-max symbols
        symbols = {}
        for line in run(["arm-none-eabi-nm", "-S", elf]).splitlines():
            fields = line.split()
            if (len(fields) == 4 and fields[3].startswith("mcumax") and
                    fields[2] in "TtRr"):
                symbols[fields[3]] = int(fields[1], 16)

    size = sum(symbols.values())

    if args.verbose:
        for name, symbol_size in sorted(symbols.items(),
                                        key=lambda item: -item[1]):
            print(f"{symbol_size:6d} {name}")

    print(f"Linked flash size (Cortex-M0, -Os): {size} bytes")

    if args.budget is not None and size > args.budget:
        print(f"Size test FAILED: exceeds budget of {args.budget} bytes")
        sys.exit(1)


if __name__ == "__main__":
    main()
