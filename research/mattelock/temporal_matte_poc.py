#!/usr/bin/env python3
"""Dependency-free MatteLock temporal proof of concept.

This is intentionally NOT production code and NOT an After Effects plugin.
It answers one narrow algorithmic question before spending implementation time:

    Does motion-aligning neighboring mattes before a robust temporal median
    materially outperform a naive temporal median on a moving matte with
    deliberately introduced edge chatter / holes?

The synthetic sequence is deterministic and uses only Python's standard
library.  It models a translating circular matte, corrupts its boundary, then
compares:

1. damaged matte
2. naive 3-frame median with no motion compensation
3. motion-aware 3-frame median using a small exhaustive translation search

The algorithm here handles global translation only.  It deliberately does NOT
claim to solve articulated motion, hair, semi-transparent alpha, motion blur,
local deformation, occlusion, or production optical flow.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

WIDTH = 96
HEIGHT = 64
FRAME_COUNT = 15
SEARCH_RADIUS = 5
RANDOM_SEED = 0

Mask = list[list[int]]


def clean_mask(frame: int) -> Mask:
    """Generate a moving circular reference matte."""
    cx = 20 + 3 * frame
    cy = 32 + int(round(3 * math.sin(frame * 0.45)))
    radius = 13
    return [
        [
            1 if (x - cx) ** 2 + (y - cy) ** 2 <= radius * radius else 0
            for x in range(WIDTH)
        ]
        for y in range(HEIGHT)
    ]


def boundary_pixels(mask: Mask) -> list[tuple[int, int]]:
    points: list[tuple[int, int]] = []
    for y in range(1, HEIGHT - 1):
        for x in range(1, WIDTH - 1):
            value = mask[y][x]
            if any(
                mask[y + dy][x + dx] != value
                for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1))
            ):
                points.append((y, x))
    return points


def damage_sequence(reference: list[Mask]) -> list[Mask]:
    """Introduce deterministic edge chatter and intermittent interior holes."""
    rng = random.Random(RANDOM_SEED)
    damaged: list[Mask] = []

    for reference_mask in reference:
        mask = [row[:] for row in reference_mask]

        # Boundary chatter: randomly flip a subset of edge-adjacent pixels.
        for y, x in boundary_pixels(reference_mask):
            if rng.random() < 0.14:
                mask[y][x] = 1 - mask[y][x]

        # Small intermittent holes.
        inside = [
            (y, x)
            for y in range(HEIGHT)
            for x in range(WIDTH)
            if reference_mask[y][x]
        ]
        rng.shuffle(inside)
        for y, x in inside[:8]:
            mask[y][x] = 0

        damaged.append(mask)

    return damaged


def shift(mask: Mask, dx: int, dy: int) -> Mask:
    """Translate a mask into current-frame coordinates; outside becomes 0."""
    result = [[0] * WIDTH for _ in range(HEIGHT)]
    for y in range(HEIGHT):
        src_y = y - dy
        if not 0 <= src_y < HEIGHT:
            continue
        for x in range(WIDTH):
            src_x = x - dx
            if 0 <= src_x < WIDTH:
                result[y][x] = mask[src_y][src_x]
    return result


def mean_abs_error(a: Mask, b: Mask) -> float:
    total = 0
    for y in range(HEIGHT):
        for x in range(WIDTH):
            total += abs(a[y][x] - b[y][x])
    return total / (WIDTH * HEIGHT)


def best_translation(neighbor: Mask, current: Mask) -> tuple[int, int]:
    """Find the global integer translation minimizing matte disagreement."""
    best_error = float("inf")
    best_dx = 0
    best_dy = 0

    for dy in range(-SEARCH_RADIUS, SEARCH_RADIUS + 1):
        for dx in range(-SEARCH_RADIUS, SEARCH_RADIUS + 1):
            error = mean_abs_error(shift(neighbor, dx, dy), current)
            if error < best_error:
                best_error = error
                best_dx = dx
                best_dy = dy

    return best_dx, best_dy


def temporal_median(frames: list[Mask]) -> Mask:
    result = [[0] * WIDTH for _ in range(HEIGHT)]
    middle = len(frames) // 2

    for y in range(HEIGHT):
        for x in range(WIDTH):
            values = sorted(frame[y][x] for frame in frames)
            result[y][x] = values[middle]

    return result


def naive_filter(damaged: list[Mask], frame: int) -> Mask:
    prev_frame = damaged[max(0, frame - 1)]
    current = damaged[frame]
    next_frame = damaged[min(FRAME_COUNT - 1, frame + 1)]
    return temporal_median([prev_frame, current, next_frame])


def motion_aware_filter(damaged: list[Mask], frame: int) -> Mask:
    current = damaged[frame]
    aligned = [current]

    for neighbor_index in (max(0, frame - 1), min(FRAME_COUNT - 1, frame + 1)):
        neighbor = damaged[neighbor_index]
        dx, dy = best_translation(neighbor, current)
        aligned.append(shift(neighbor, dx, dy))

    return temporal_median(aligned)


def iou(a: Mask, b: Mask) -> float:
    intersection = 0
    union = 0

    for y in range(HEIGHT):
        for x in range(WIDTH):
            a_on = bool(a[y][x])
            b_on = bool(b[y][x])
            intersection += int(a_on and b_on)
            union += int(a_on or b_on)

    return intersection / union if union else 1.0


def write_pgm(mask: Mask, path: Path) -> None:
    """Write a viewable grayscale PGM without an imaging dependency."""
    with path.open("w", encoding="ascii") as handle:
        handle.write(f"P2\n{WIDTH} {HEIGHT}\n255\n")
        for row in mask:
            handle.write(" ".join("255" if value else "0" for value in row))
            handle.write("\n")


def average_iou(sequence: list[Mask], reference: list[Mask]) -> float:
    return sum(iou(sequence[i], reference[i]) for i in range(FRAME_COUNT)) / FRAME_COUNT


def main() -> None:
    reference = [clean_mask(frame) for frame in range(FRAME_COUNT)]
    damaged = damage_sequence(reference)
    naive = [naive_filter(damaged, frame) for frame in range(FRAME_COUNT)]
    motion_aware = [
        motion_aware_filter(damaged, frame) for frame in range(FRAME_COUNT)
    ]

    print("MatteLock synthetic proof-of-concept")
    print(f"damaged IoU:      {average_iou(damaged, reference):.4f}")
    print(f"naive median IoU: {average_iou(naive, reference):.4f}")
    print(f"motion-aware IoU: {average_iou(motion_aware, reference):.4f}")

    output_dir = Path(__file__).with_name("output")
    output_dir.mkdir(exist_ok=True)

    demo_frame = FRAME_COUNT // 2
    write_pgm(reference[demo_frame], output_dir / "reference.pgm")
    write_pgm(damaged[demo_frame], output_dir / "damaged.pgm")
    write_pgm(naive[demo_frame], output_dir / "naive_median.pgm")
    write_pgm(motion_aware[demo_frame], output_dir / "motion_aware.pgm")

    print(f"wrote frame {demo_frame} PGM comparisons to: {output_dir}")


if __name__ == "__main__":
    main()
