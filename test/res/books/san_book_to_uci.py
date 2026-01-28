#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

import chess

RESULTS = {"1-0", "0-1", "1/2-1/2", "*"}

# Common PGN-ish junk you might encounter
MOVE_NUM_RE = re.compile(r"^\d+\.(\.\.)?$")  # "1." or "1..."
NAG_RE = re.compile(r"^\$\d+$")              # "$1"
COMMENT_BRACE_RE = re.compile(r"\{[^}]*\}")  # "{ ... }"


def normalize_tokens(line: str) -> list[str]:
    # Remove brace comments
    line = COMMENT_BRACE_RE.sub(" ", line)

    # Split on whitespace
    raw = line.strip().split()
    out: list[str] = []

    for t in raw:
        # Drop move numbers like "1." or "23..." (sometimes appear in PGNs)
        if MOVE_NUM_RE.match(t):
            continue

        # Drop NAGs like "$1"
        if NAG_RE.match(t):
            continue

        # Strip trailing annotation symbols that SAN often carries
        # Keep +/# because python-chess SAN parser accepts them, but strip !?
        t = t.strip()
        while t and t[-1] in ("!", "?"):
            t = t[:-1]

        if t:
            out.append(t)

    return out


def convert_game_tokens_to_uci(tokens: list[str]) -> tuple[list[str], str]:
    if not tokens:
        raise ValueError("empty line")

    result = tokens[-1]
    if result not in RESULTS:
        raise ValueError(f"missing/invalid result token at end (got '{result}')")

    san_moves = tokens[:-1]
    board = chess.Board()

    uci_moves: list[str] = []
    for san in san_moves:
        # python-chess expects SAN relative to current position
        try:
            move = board.parse_san(san)
        except Exception as e:
            raise ValueError(f"bad SAN '{san}' at ply {board.ply()+1}: {e}") from e

        uci_moves.append(move.uci())
        board.push(move)

    return uci_moves, result


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: san_book_to_uci.py <input_san.txt> <output_uci.txt>", file=sys.stderr)
        return 2

    in_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])

    lines = in_path.read_text(encoding="utf-8", errors="replace").splitlines()

    kept = 0
    skipped = 0

    with out_path.open("w", encoding="utf-8") as f_out:
        for idx, line in enumerate(lines, start=1):
            line = line.strip()
            if not line:
                continue

            tokens = normalize_tokens(line)
            try:
                uci_moves, result = convert_game_tokens_to_uci(tokens)
            except Exception as e:
                skipped += 1
                print(f"[skip line {idx}] {e}", file=sys.stderr)
                continue

            # Write: "e2e4 e7e5 ... 1-0"
            f_out.write(" ".join(uci_moves) + " " + result + "\n")
            kept += 1

    print(f"Done. kept={kept}, skipped={skipped}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
