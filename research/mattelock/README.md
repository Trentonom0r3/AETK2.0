# MatteLock research prototype

This folder contains a deliberately small, dependency-free proof of concept for one narrow question:

> Does motion-aligning neighboring mattes before temporal filtering meaningfully outperform a naive frame-to-frame average/median when the matte is moving?

It is **not** production code and **not** an After Effects plugin.

## Prototype

`temporal_matte_poc.py` generates a deterministic moving circular matte, deliberately corrupts the matte boundary and punches intermittent holes into it, then compares:

1. the damaged matte
2. a naive three-frame temporal median
3. a motion-aware three-frame median after integer-translation alignment

The script uses only the Python standard library and writes PGM comparison images so it introduces no new package/runtime dependency.

Run:

```bash
python research/mattelock/temporal_matte_poc.py
```

## Baseline result

With the current deterministic test parameters, the validation run produced:

```text
damaged IoU:      0.9506
naive median IoU: 0.9527
motion-aware IoU: 0.9795
```

The important observation is not the absolute score. The synthetic test is intentionally simple. The useful result is that a naive temporal median barely improves the moving matte, while even crude motion alignment before the same robust temporal median improves it substantially.

That supports the technical premise that **motion compensation is necessary if MatteLock is going to beat a simple weighted-neighbor smoother without producing sticky/lagging boundaries.**

## What this proves

- A classical, non-ML approach is technically plausible.
- Temporal stabilization should be motion-aware rather than a blind neighboring-frame blend.
- A first production prototype does not need neural-network weights.
- There is enough algorithmic signal to justify a targeted AE A/B experiment if the commercial case survives.

## What this absolutely does NOT prove

The prototype currently models only:

- binary alpha
- a simple translating object
- global integer motion
- synthetic edge chatter
- synthetic small holes

It does **not** solve or validate:

- articulated/local motion
- hair / fine semi-transparent detail
- motion blur
- subpixel edges
- soft alpha
- occlusion/disocclusion
- independently moving regions
- camera + subject motion
- optical-flow failures
- production performance
- any superiority over Adobe Refine Matte / Reduce Chatter

## Next technical gate

Do not expand this into a full plugin yet.

The next meaningful test is a direct comparison on real problematic mattes:

- raw Object Matte / Roto Brush output
- Adobe Refine Matte / Reduce Chatter, carefully tuned
- a minimal AETK motion-aware prototype

AETK already supports neighboring-frame SmartFX checkouts via `pre_render_context::checkout_layer_at_offset(...)`, and Smart Render can recover those worlds by checkout ID through `context::checkout_pixels(...)`.

Proceed only if the custom approach can clearly outperform Adobe on at least one important class of failure without materially damaging legitimate motion.