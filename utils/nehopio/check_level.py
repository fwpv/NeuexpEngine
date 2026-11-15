"""Проверка карты уровня демо nehopio.

Запуск: python utils/nehopio/check_level.py [файл_уровня]
"""

import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[2]
LEVEL = Path(sys.argv[1]) if len(sys.argv) > 1 else PROJECT / "assets" / "nehopio" / "level.txt"

rows = LEVEL.read_text(encoding="utf-8").splitlines()
SOLID = set("#=BQSUM")
width = len(rows[0])
print("rows:", len(rows), "width:", width)

problems = []
for row_index, row in enumerate(rows):
    if len(row) != width:
        problems.append(f"row {row_index} width {len(row)}")
    for column, symbol in enumerate(row):
        if symbol in "oEPFV" and rows[row_index][column] in SOLID:
            problems.append(f"{symbol} inside solid at {column},{row_index}")
        if symbol == "E":
            below = rows[row_index + 1][column] if row_index + 1 < len(rows) else "."
            if below not in SOLID:
                problems.append(f"enemy without ground at {column},{row_index}")

ground_rows = range(len(rows) - 2, len(rows))
pits = [c for c in range(width) if all(rows[r][c] not in SOLID for r in ground_rows)]
groups = []
for column in pits:
    if groups and column == groups[-1][1] + 1:
        groups[-1][1] = column
    else:
        groups.append([column, column])
print("pits:", groups)

platforms = []
for column in range(width):
    if rows[8][column] == "B":
        if platforms and column == platforms[-1][1] + 1:
            platforms[-1][1] = column
        else:
            platforms.append([column, column])
print("bricks in row 8:", platforms)

spawn = [(c, r) for r in range(len(rows)) for c in range(width) if rows[r][c] == "P"]
flag = [(c, r) for r in range(len(rows)) for c in range(width) if rows[r][c] == "F"]
print("spawn:", spawn, "flag:", flag)
print("coins:", sum(row.count("o") for row in rows), "enemies:", sum(row.count("E") for row in rows),
    "flyers:", sum(row.count("V") for row in rows), "mushroom blocks:", sum(row.count("M") for row in rows))
print("problems:", problems or "none")
