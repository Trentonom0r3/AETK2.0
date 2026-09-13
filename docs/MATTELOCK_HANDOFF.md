# MatteLock Validation Handoff

Date: 2026-09-12
Branch: `research/mattelock-validation`
Base examined: `bde4d0f89d1664e5c1d070c1dba30d622d99a487`

## Current status

**Do not build the polished commercial product yet.**

The repository/framework side is in good shape for the concept, but the original market hypothesis was too optimistic because After Effects already ships temporal matte chatter reduction in Roto Brush / Refine Matte.

The revised hypothesis is narrower:

> A paid plugin may still be viable if a motion-aware approach can visibly outperform Adobe’s built-in Reduce Chatter / Refine Matte on modern Object Matte / Roto Brush failures, and/or provide substantially better temporal diagnostics / problem-frame QC.

Commercial score for the original pitch: **4.5/10**.

Technical AETK fit: **9/10**.

## What was discovered

### AETK already supports the important temporal plumbing

`include/aetk/effect/context/context.hpp` already contains:

- `context::param_at_time(...)`
- `context::param_at_offset(...)`
- `pre_render_context::checkout_layer_at_offset(...)`
- `context::checkout_pixels(checkout_id)` for retrieving checked-out SmartFX layer worlds
- `render_request` helpers
- `smart_world` support across AE bit depths

This means a temporal radius can be expressed naturally as distinct SmartFX checkout IDs. No new frame-checkout subsystem is required.

### Wide-time support

`global_setup_context::enable_temporal_checkouts()` currently wraps `PF_OutFlag_WIDE_TIME_INPUT`.

The PiPL generator also recognizes `PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT`.

For a production SmartFX temporal effect, prefer automatic wide-time dependency tracking so AE can invalidate exact time dependencies. A small convenience wrapper can be added later if the product proceeds.

### Compute Cache

AETK already contains `aetk::effect::compute_cache<T>` around `AEGP_ComputeCacheSuite1`. This could later cache per-frame descriptors / motion data and avoid redundant cross-frame computation under MFR.

Do not introduce it until profiling shows a reason.

### Premiere Pro

Do not promise Premiere support in the first product iteration. AETK explicitly treats SmartFX pre-render as AE-only in Premiere compatibility mode. Premiere would need a different temporal render path and should not constrain the proof.

## Market finding that changed the plan

Adobe already exposes Refine Matte separately for non-Roto-Brush mattes and documents Reduce Chatter / Chatter Reduction as a temporal neighboring-frame operation intended to reduce frame-to-frame edge instability.

Therefore this positioning is weak:

> “AE doesn’t have temporal matte stabilization; MatteLock fills that gap.”

It does have it.

Current user reports still show real Object Matte / Roto Brush edge failures, so the problem is not imaginary. The open question is whether a third-party algorithm can be enough better to justify a purchase.

Adjacent aescripts products also show a market for specialized compositing/matte tools around the ~$50–60 level, but none of that overrides the built-in Adobe competitor.

See `docs/MATTELOCK_VALIDATION.md` for the detailed market summary and links.

## Licensing status

See root `LICENSE_AUDIT.md`.

No model files, model weights, third-party image assets, OpenCV binaries, or other runtime dependency were added.

OpenCV was considered but deliberately not adopted for the first proof.

AETK’s own AGPL / commercial dual-license story needs a real provenance review before a closed-source first-party commercial release, especially for any code copied/adapted from third-party samples or outside contributions.

## Algorithm proof

A dependency-free synthetic proof was added at:

`research/mattelock/temporal_matte_poc.py`

The proof compares:

1. damaged moving matte
2. naive 3-frame temporal median
3. motion-aligned 3-frame temporal median

Baseline deterministic result:

```text
damaged IoU:      0.9506
naive median IoU: 0.9527
motion-aware IoU: 0.9795
```

Interpretation:

- blind temporal filtering barely helps when an edge is legitimately moving
- even crude motion alignment makes the same robust temporal statistic much more effective
- this supports the algorithmic direction without requiring ML

Limitations are intentionally severe: binary alpha, global translation, synthetic corruption, no hair/motion blur/local deformation.

## Files added

- `docs/MATTELOCK_VALIDATION.md`
- `docs/MATTELOCK_HANDOFF.md`
- `LICENSE_AUDIT.md`
- `research/mattelock/README.md`
- `research/mattelock/temporal_matte_poc.py`

No production AETK source/header was modified during this validation pass.

## Build / test status

No native After Effects build was performed in this session.

Reason: the market gate changed before an AE plugin implementation was justified, and the available execution environment did not contain the AE SDK/toolchain required for a meaningful native build.

The synthetic algorithm proof was independently executed against its deterministic test design and produced the IoU numbers recorded above.

## Exact next steps if continuing

### Step 1 — Collect real failure material

Create a tiny benchmark pack containing:

- Object Matte result with visible temporal jitter
- Roto Brush result with edge chatter
- fast-moving hard edge
- motion-blurred edge
- hair / semi-transparent edge
- intermittent holes / speckles

Prefer footage the developer owns or can legally redistribute; otherwise keep the benchmark local and document its provenance.

### Step 2 — Establish Adobe baseline

For each clip, tune Adobe Refine Matte / Reduce Chatter carefully and render the best achievable baseline.

Do not compare against default settings only.

### Step 3 — Build the smallest AETK effect

Copy the AETK Skeleton sample into a research effect, for example `samples/Effect/MatteLock_Prototype`.

Initial controls only:

- Strength
- Temporal Radius (1–3)
- Motion Sensitivity
- Edge Protection
- View: Result / Original / Difference / Instability

No custom Drawbot UI, presets, licensing, installer, CUDA, branding, or marketplace packaging.

### Step 4 — SmartFX temporal checkouts

During pre-render, request neighboring frames with stable unique checkout IDs via `checkout_layer_at_offset(...)`.

During Smart Render, retrieve each world with `checkout_pixels(checkout_id)`.

Set exact wide-time dependencies, preferably `PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT` for the SmartFX path.

### Step 5 — Implement CPU reference algorithm

Start with:

1. alpha normalization to float
2. transition-band detection
3. instability map
4. simple multiscale/block motion estimate near the boundary
5. motion-compensated robust temporal estimate
6. confidence-weighted blend back toward the current frame

If motion confidence is poor, preserve the current frame rather than hallucinating continuity.

### Step 6 — A/B gate

Compare against Adobe on the benchmark pack.

Proceed only if at least one commercially important class of failure is visibly better without unacceptable edge lag / detail loss.

If the stabilizer does not beat Adobe, investigate whether the **Instability / QC visualization** itself is a better product: identify bad frames/regions, visualize temporal matte variance, and direct the artist to where manual roto attention is needed.

### Step 7 — Only after quality is proven

Then consider:

- Compute Cache
- optimization
- GPU acceleration
- custom UI
- presets
- licensing integration
- installers/signing/notarization
- marketing demo clips
- aescripts submission

## Kill criteria

Stop the product if:

- it merely matches Adobe’s built-in chatter reduction
- it needs heavy ML/model dependencies to become marginally better
- it introduces visible motion lag/ghosting on common footage
- it becomes so slow that the workflow advantage disappears
- support complexity is disproportionate to a ~$50 product

The purpose of this branch is to make it cheap to reach a confident **go / no-go**, not to create sunk-cost pressure to ship.