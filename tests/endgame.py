#!/usr/bin/env python3
"""
Endgame conversion test: can the engine win won endgames?

Plays the engine (attacker) from random won positions (KQ-K, KR-K and
won KP-K) against a strong defender, and counts mates achieved before a
draw by the 50-move rule, repetition, stalemate or insufficient material.

Usage:
  endgame.py ENGINE [--nodes N ...] [--positions N] [--defender CMD]

Requires: pip install chess, and a strong UCI defender (default:
stockfish), also used to select won KP-K positions.
"""

import argparse
import random
import shlex

import chess
import chess.engine

ENDGAMES = {
    "KQ-K": [chess.QUEEN],
    "KR-K": [chess.ROOK],
    "KP-K": [chess.PAWN],
}


def random_position(rnd, pieces, defender):
    """Random legal position, white to move, white winning."""
    while True:
        board = chess.Board(None)
        squares = rnd.sample(chess.SQUARES, 2 + len(pieces))
        board.set_piece_at(squares[0], chess.Piece(chess.KING, chess.WHITE))
        board.set_piece_at(squares[1], chess.Piece(chess.KING, chess.BLACK))
        for square, piece_type in zip(squares[2:], pieces):
            if (piece_type == chess.PAWN and
                    chess.square_rank(square) in (0, 7)):
                break
            board.set_piece_at(square, chess.Piece(piece_type, chess.WHITE))
        else:
            board.turn = chess.WHITE
            if not board.is_valid() or board.is_game_over():
                continue
            # Black's king must not be able to capture a piece next move
            board.turn = chess.BLACK
            if any(board.is_capture(move) for move in board.legal_moves):
                continue
            board.turn = chess.WHITE
            # Won according to the defender engine (needed for KP-K)
            info = defender.analyse(board, chess.engine.Limit(depth=20))
            score = info["score"].white()
            if score.is_mate() and score.mate() > 0 or \
                    (score.score() or 0) > 500:
                return board


def play(engine, defender, board, nodes):
    """Returns the number of moves to mate, or the reason for failing."""
    while True:
        outcome = board.outcome(claim_draw=True)
        if outcome:
            if outcome.winner == chess.WHITE:
                return (board.ply() + 1) // 2
            return outcome.termination.name.lower()
        if board.turn == chess.WHITE:
            move = engine.play(board, chess.engine.Limit(nodes=nodes)).move
            if move not in board.legal_moves:
                return "illegal move"
        else:
            move = defender.play(board, chess.engine.Limit(time=0.02)).move
        board.push(move)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("engine")
    parser.add_argument("--nodes", type=int, nargs="+",
                        default=[1, 4096, 16384, 65536])
    parser.add_argument("--positions", type=int, default=20)
    parser.add_argument("--defender", default="stockfish")
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()

    engine = chess.engine.SimpleEngine.popen_uci(shlex.split(args.engine))
    defender = chess.engine.SimpleEngine.popen_uci(
        shlex.split(args.defender))

    rnd = random.Random(args.seed)
    positions = {name: [random_position(rnd, pieces, defender)
                        for _ in range(args.positions)]
                 for name, pieces in ENDGAMES.items()}

    failed = False
    print(f"{'nodes':>8}  " + "  ".join(f"{name:>22}" for name in ENDGAMES))
    for nodes in args.nodes:
        results = []
        for name in ENDGAMES:
            mates, moves, failures = 0, [], {}
            for board in positions[name]:
                result = play(engine, defender, board.copy(), nodes)
                if isinstance(result, int):
                    mates += 1
                    moves.append(result)
                else:
                    failures[result] = failures.get(result, 0) + 1
            failed |= mates < len(positions[name])
            average = f"{sum(moves) / len(moves):.0f}" if moves else "-"
            results.append(f"{mates:>3}/{len(positions[name])} mates "
                           f"(avg {average:>3} moves)")
            if failures:
                print(f"    {nodes} nodes, {name} failures: {failures}")
        print(f"{nodes:>8}  " + "  ".join(f"{r:>22}" for r in results))

    engine.quit()
    defender.quit()


if __name__ == "__main__":
    main()
