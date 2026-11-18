#!/usr/bin/env python3
"""
Regression test: compares the engine's legal moves and best moves on a
fixed set of positions, and a self-play game, against a golden file.

Any change in output fails the test. Changes that intentionally alter the
search (e.g. evaluation tweaks) must regenerate the golden file:

  regression.py HARNESS --update

Usage: regression.py HARNESS [--golden FILE] [--update]

Requires: pip install chess
"""

import argparse
import difflib
import os
import random
import subprocess
import sys

import chess


def run(harness):
    process = subprocess.Popen([harness], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, text=True)

    def ask(command):
        process.stdin.write(command + "\n")
        process.stdin.flush()
        return process.stdout.readline().strip()

    def send(command):
        process.stdin.write(command + "\n")
        process.stdin.flush()

    # Fixed positions from random games
    rnd = random.Random(42)
    fens = []
    for _ in range(25):
        board = chess.Board()
        for _ in range(rnd.randint(5, 80)):
            moves = list(board.legal_moves)
            if not moves:
                break
            board.push(rnd.choice(moves))
        fens.append(board.fen())

    lines = []
    for fen in fens:
        send("fen " + fen)
        lines.append(f"{fen}: moves {ask('moves')}")
        for nodes in (300, 5000, 40000):
            lines.append(f"{fen}: best {nodes} {ask(f'best {nodes}')}")

    # Self-play
    send("init")
    for ply in range(60):
        move = ask("best 3000")
        lines.append(f"self-play {ply}: {move}")
        if move == "none":
            break
        ask("play " + move)

    process.stdin.close()
    process.wait()
    return lines


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("harness")
    parser.add_argument("--golden", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "regression.txt"))
    parser.add_argument("--update", action="store_true")
    args = parser.parse_args()

    lines = run(args.harness)

    if args.update:
        with open(args.golden, "w") as f:
            f.write("\n".join(lines) + "\n")
        print(f"Updated {args.golden}")
        return

    with open(args.golden) as f:
        golden = f.read().splitlines()

    diff = list(difflib.unified_diff(golden, lines, "golden", "current",
                                     lineterm="", n=0))
    if diff:
        print("\n".join(diff[:40]))
        print("Regression test FAILED")
        sys.exit(1)

    print(f"Regression test passed ({len(lines)} lines)")


if __name__ == "__main__":
    main()
