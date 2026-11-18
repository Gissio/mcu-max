#!/usr/bin/env python3
"""
Plays a match between two UCI engines and estimates their Elo difference.

Usage:
  strength.py ENGINE_A ENGINE_B [--games N]
              [--nodes N | --movetime MS | --tc BASE+INC]
              [--opt-b NAME=VALUE ...] [--seed N]

Each engine is a command line (e.g. "./mcu-max-uci" or "stockfish"), using
UCI; prefix it with "xboard:" for XBoard engines (e.g. "xboard:fairymax").
Games start from varied random 4-ply openings, played twice with colors
swapped. Games end by the rules (including draw claims by repetition and
the 50-move rule), or are adjudicated as draws after 300 plies. An illegal
move loses the game.

Requires: pip install chess
"""

import argparse
import math
import random
import shlex
import time

import chess
import chess.engine


def elo(score):
    score = min(max(score, 1e-3), 1 - 1e-3)
    return -400 * math.log10(1 / score - 1)


def play(white, black, board, tc):
    engines = {chess.WHITE: white, chess.BLACK: black}
    # Game clock (seconds), if playing with a time control
    clock = {chess.WHITE: tc[0], chess.BLACK: tc[0]} if tc else None
    while not board.is_game_over(claim_draw=True) and board.ply() < 300:
        engine = engines[board.turn]
        limit = None
        if clock:
            limit = chess.engine.Limit(white_clock=clock[chess.WHITE],
                                       black_clock=clock[chess.BLACK],
                                       white_inc=tc[1], black_inc=tc[1])
        start = time.monotonic()
        try:
            move = engine.play(board, limit).move
        except chess.engine.EngineError:
            move = None
        if clock:
            clock[board.turn] += tc[1] - (time.monotonic() - start)
            if clock[board.turn] < 0:
                # Lost on time
                return (0.0 if board.turn == chess.WHITE else 1.0), "time"
        if move is None or move not in board.legal_moves:
            # Illegal move or no move while moves exist: loss
            return 0.0 if board.turn == chess.WHITE else 1.0, "illegal"
        board.push(move)
    outcome = board.outcome(claim_draw=True)
    if outcome is None or outcome.winner is None:
        return 0.5, "draw"
    return (1.0 if outcome.winner == chess.WHITE else 0.0), "mate"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("engine_a")
    parser.add_argument("engine_b")
    parser.add_argument("--games", type=int, default=100)
    parser.add_argument("--nodes", type=int)
    parser.add_argument("--movetime", type=int, help="milliseconds")
    parser.add_argument("--tc", help="time control BASE+INC in seconds, "
                        "e.g. 10+0.1 (both engines)")
    parser.add_argument("--tc-b", help="time control for engine B only "
                        "(engine A uses its own limit)")
    parser.add_argument("--nodes-b", type=int, help="nodes for engine B")
    parser.add_argument("--depth", type=int, help="max depth (with --nodes)")
    parser.add_argument("--depth-b", type=int, help="max depth for engine B")
    parser.add_argument("--movetime-b", type=int, help="ms for engine B")
    parser.add_argument("--opt", action="append", default=[],
                        help="UCI option for engine A, NAME=VALUE")
    parser.add_argument("--opt-b", action="append", default=[],
                        help="UCI option for engine B, NAME=VALUE")
    parser.add_argument("--random", type=float, default=0,
                        help="probability that engine A plays a random move")
    parser.add_argument("--random-b", type=float, default=0,
                        help="probability that engine B plays a random move")
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()

    def limit(nodes, depth, movetime):
        if nodes:
            return chess.engine.Limit(nodes=nodes, depth=depth)
        return chess.engine.Limit(time=(movetime or 100) / 1000)

    limit_a = limit(args.nodes, args.depth, args.movetime)
    # Engine B: a time limit of its own takes priority over shared nodes
    limit_b = limit(None if args.movetime_b else (args.nodes_b or args.nodes),
                    args.depth_b if args.depth_b is not None else args.depth,
                    args.movetime_b or args.movetime)

    def popen(command):
        if command.startswith("xboard:"):
            return chess.engine.SimpleEngine.popen_xboard(
                shlex.split(command[len("xboard:"):]))
        return chess.engine.SimpleEngine.popen_uci(shlex.split(command))

    a = popen(args.engine_a)
    b = popen(args.engine_b)
    if args.opt:
        a.configure(dict(o.split("=", 1) for o in args.opt))
    if args.opt_b:
        b.configure(dict(o.split("=", 1) for o in args.opt_b))

    noise = random.Random(args.seed + 1000)

    class Limited:
        def __init__(self, engine, limit, random_probability, clock):
            self.engine, self.limit = engine, limit
            self.random_probability = random_probability
            self.clock = clock

        def play(self, board, limit):
            # Weakening by random moves (as an application could do)
            if noise.random() < self.random_probability:
                return chess.engine.PlayResult(
                    noise.choice(list(board.legal_moves)), None)
            return self.engine.play(board,
                                    limit if (limit and self.clock)
                                    else self.limit)

    pa = Limited(a, limit_a, args.random, bool(args.tc))
    pb = Limited(b, limit_b, args.random_b, bool(args.tc or args.tc_b))

    tc_arg = args.tc or args.tc_b
    tc = tuple(float(x) for x in tc_arg.split("+")) if tc_arg else None

    rnd = random.Random(args.seed)
    score = 0.0
    counts = {"win": 0, "draw": 0, "loss": 0}
    illegal = {"A": 0, "B": 0}
    for game in range(args.games):
        if game % 2 == 0:
            opening = chess.Board()
            for _ in range(4):
                opening.push(rnd.choice(list(opening.legal_moves)))
        board = opening.copy()
        a_white = (game % 2 == 0)
        result, reason = play(pa if a_white else pb,
                              pb if a_white else pa, board, tc)
        a_score = result if a_white else 1 - result
        if reason == "illegal":
            illegal["A" if a_score == 0 else "B"] += 1
        score += a_score
        counts["win" if a_score == 1 else "loss" if a_score == 0 else "draw"] += 1
        print(f"game {game + 1}: A {'white' if a_white else 'black'} "
              f"{a_score} ({reason}), total {score}/{game + 1}", flush=True)

    a.quit()
    b.quit()

    n = args.games
    s = score / n
    # 95% confidence interval from the per-game score variance
    var = (counts["win"] * (1 - s) ** 2 + counts["loss"] * s ** 2 +
           counts["draw"] * (0.5 - s) ** 2) / n
    margin = 1.96 * math.sqrt(var / n)
    print(f"\nA vs B: +{counts['win']} ={counts['draw']} -{counts['loss']}, "
          f"score {s:.3f}, Elo {elo(s):+.0f} "
          f"[{elo(s - margin):+.0f}, {elo(s + margin):+.0f}]")
    print(f"Illegal moves: A {illegal['A']}, B {illegal['B']}")


if __name__ == "__main__":
    main()
