# BlackQuartz

**BlackQuartz** is a modern UCI chess engine focused on practical playing strength, clean architecture, and easy integration with existing chess GUIs.

> UCI name: `BlackQuartz`  
> Author: `Brodie (BQ)`

---

## Features

- UCI protocol support (works with most chess GUIs)
- Alpha-beta search with **PVS** (Principal Variation Search)
- Quiescence search (captures / promotions / tactical continuations)
- Transposition table (Zobrist hashing)
- Move ordering heuristics (TT move, captures, etc.)
- Bitboard-based move generation with magic bitboards for sliders

---

## Getting started

### Build

BlackQuartz is written in **C++23**

#### Linux / macOS
```bash