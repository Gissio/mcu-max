#!/usr/bin/env python3
"""
Cross-checks mcu-max against python-chess on random games:

- Legal moves, after playing moves and after setting the position by FEN.
- Game state (ongoing, check, checkmate, stalemate).
- FEN output, and FEN round trip.

Usage: test_engine.py HARNESS [--games N] [--seed N]

Requires: pip install chess
"""

import argparse
import random
import subprocess
import sys

import chess


class Engine:
    def __init__(self, path):
        self.process = subprocess.Popen([path], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, text=True)

    def send(self, command):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def ask(self, command):
        self.send(command)
        return self.process.stdout.readline().strip()


def expected_moves(board):
    # mcu-max always promotes to queen: compare from/to squares only
    return sorted({move.uci()[:4] for move in board.legal_moves})


def expected_state(board):
    if board.is_checkmate():
        return 3
    if board.is_stalemate():
        return 2
    return 1 if board.is_check() else 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("harness")
    parser.add_argument("--games", type=int, default=100)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()

    engine = Engine(args.harness)
    rnd = random.Random(args.seed)
    errors = 0
    positions = 0

    def check(what, got, expected, board):
        nonlocal errors
        if got != expected:
            errors += 1
            if errors <= 10:
                print(f"FAIL {what}: {board.fen()}\n"
                      f"  expected: {expected}\n  got:      {got}")

    for game in range(args.games):
        board = chess.Board()
        engine.send("init")

        for ply in range(200):
            positions += 1
            fen = board.fen(en_passant="fen")
            moves = expected_moves(board)

            # Position reached by playing moves
            check("moves", sorted(engine.ask("moves").split()), moves, board)
            check("fen", engine.ask("getfen").split()[:4],
                  fen.split()[:4], board)

            # Same position set by FEN
            engine.send("fen " + fen)
            check("moves (fen)", sorted(engine.ask("moves").split()),
                  moves, board)
            check("fen round trip", engine.ask("getfen").split()[:4],
                  fen.split()[:4], board)
            check("state", int(engine.ask("state")),
                  expected_state(board), board)

            if not moves or board.is_insufficient_material():
                break

            move = rnd.choice(list(board.legal_moves))
            if move.promotion:
                move.promotion = chess.QUEEN
            check("play", engine.ask("play " + move.uci()[:4]), "1", board)
            board.push(move)

    print(f"{args.games} games, {positions} positions, {errors} errors")
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
