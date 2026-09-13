# MatteLock Validation

Status: **Research gate — do not build a commercial plugin yet**

Date: 2026-09-12

## Executive conclusion

AETK 2.0 is technically capable of supporting a temporal matte-processing effect cleanly. In fact, the framework already contains most of the temporal plumbing that MatteLock would need: neighboring-frame SmartFX checkouts, checkout IDs that can be recovered during Smart Render, time-offset parameter access, 8/16/32-bit world handling, MFR-oriented infrastructure, and an AEGP Compute Cache wrapper.

The commercial thesis is weaker than initially assumed, however. After Effects already ships **Reduce Chatter / Chatter Reduction** inside Roto Brush / Refine Matte workflows, and Adobe documents the algorithm as a weighted average across adjacent frames intended specifically to stop matte edges from moving erratically. Adobe also exposes Refine Matte separately for mattes created by other techniques, so MatteLock cannot honestly be positioned merely as “the temporal matte stabilizer AE does not have.”

Current user complaints show a real quality problem around Object Matte / propagated roto edges in AE 2026, including jagged propagated edges and instability in motion. That means there may still be a product opportunity, but only if MatteLock can demonstrate a **substantial quality, diagnostic, or workflow advantage over Adobe’s built-in Reduce Chatter / Refine Matte**, not merely duplicate it.

**Commercial score for the original pitch: 4.5/10.**

**Recommendation:** do not spend a full Astra/Codex implementation cycle yet. First run an empirical A/B test against Adobe Refine Matte / Reduce Chatter on representative bad mattes. If a prototype cannot clearly beat the built-in effect on at least one important class of footage without smearing legitimate motion, kill the product.

---

## Phase 1 — AETK architecture audit

### Existing abstractions that directly help MatteLock

AETK already has the core temporal APIs needed for a SmartFX implementation.

### 1. Neighboring-frame layer checkouts already exist

`pre_render_context` already exposes:

- `checkout_layer(...)`
- `checkout_layer_at_offset(index, checkout_id, frame_offset)`
- a custom `render_request` overload of `checkout_layer_at_offset(...)`

The offset checkout computes:

```cpp
current_time + (frame_offset * time_step)
```

and passes that explicit time into AE’s SmartFX `checkout_layer` callback. This means a MatteLock pre-render can request, for example, frames `-2, -1, 0, +1, +2` with distinct checkout IDs without adding a new temporal checkout abstraction.

### 2. Smart Render can recover each checked-out world by ID

`context::checkout_pixels(checkout_id)` calls `checkout_layer_pixels(...)` during Smart Render and returns the result as an RAII `smart_world`. Therefore the expected MatteLock flow is already natural in AETK:

```cpp
// pre-render
ctx.checkout_layer_at_offset(INPUT, CHECKOUT_MINUS_2, -2);
ctx.checkout_layer_at_offset(INPUT, CHECKOUT_MINUS_1, -1);
ctx.checkout_layer_at_offset(INPUT, CHECKOUT_CURRENT, 0);
ctx.checkout_layer_at_offset(INPUT, CHECKOUT_PLUS_1, +1);
ctx.checkout_layer_at_offset(INPUT, CHECKOUT_PLUS_2, +2);

// smart-render
auto f_m2 = ctx.checkout_pixels(CHECKOUT_MINUS_2);
auto f_m1 = ctx.checkout_pixels(CHECKOUT_MINUS_1);
auto f_0  = ctx.checkout_pixels(CHECKOUT_CURRENT);
auto f_p1 = ctx.checkout_pixels(CHECKOUT_PLUS_1);
auto f_p2 = ctx.checkout_pixels(CHECKOUT_PLUS_2);
```

No one-off Adobe SDK plumbing is required for the basic temporal frame fetch.

### 3. Temporal parameter evaluation already exists

`context` also provides `param_at_time(...)` and `param_at_offset(...)`, which is useful if MatteLock controls are animated and must be evaluated consistently at neighboring times.

### 4. Compute Cache support already exists

AETK includes `aetk::effect::compute_cache<T>` wrapping `AEGP_ComputeCacheSuite1`, including RAII receipts and an MFR-conscious checkout context. This is relevant if a later implementation computes reusable per-frame descriptors such as:

- alpha-edge distance fields
- edge confidence maps
- local motion descriptors
- temporal instability maps
- low-resolution motion fields

Adobe’s own Compute Cache documentation uses temporal smoothing as the canonical example of cross-frame cached computation and reports large render-time improvements versus per-thread sequence-data recomputation.

For a first prototype, Compute Cache should be considered optional. Correctness comes first.

### 5. Bit depth

AETK’s `smart_world` system already supports ARGB8, ARGB16 and ARGB32/float workflows. A MatteLock alpha algorithm should normalize alpha to a float working representation internally and write back through AETK’s existing pixel abstraction so the same core logic works across host bit depths.

### 6. MFR

AETK already exposes `enable_mfr()` / `enable_threaded_rendering()` and documents the need to avoid mutable global/static per-instance state. MatteLock should keep all frame-local state on the render stack or in AE’s Compute Cache. Do not use sequence data as an unvalidated time-dependent cache when automatic wide-time input is enabled.

### 7. Wide-time dependency tracking

`global_setup_context::enable_temporal_checkouts()` currently enables the legacy `PF_OutFlag_WIDE_TIME_INPUT` flag.

For a SmartFX temporal effect, MatteLock should also set:

```cpp
ctx.add_out_flags2(PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT);
```

Adobe documents `PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT` as the preferred mechanism because AE tracks the exact times checked out by a render and can invalidate cached frames precisely instead of treating every upstream time change as affecting every output frame.

AETK’s PiPL generator already recognizes the raw `PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT` token.

A small future AETK ergonomic improvement could be a helper such as `enable_automatic_temporal_dependencies()`, but it is not required to prove MatteLock.

### 8. CPU / GPU

Start CPU-first. The first algorithm should be correct and benchmarked before introducing GPU-specific paths.

AETK has CUDA infrastructure, but a commercial MatteLock should not make CUDA the architectural foundation because macOS has no CUDA path. A later acceleration layer should either:

- keep a good CPU fallback and use CUDA opportunistically on Windows, or
- target host-native GPU frameworks / a portable compute abstraction once the algorithm is proven.

### 9. Premiere Pro

AETK explicitly treats SmartFX pre-render as After Effects-only in Premiere compatibility mode. Premiere support is therefore **not a free checkbox** for this design.

A future Premiere version may be possible through a classic-render / explicit `PF_CHECKOUT_PARAM` temporal path, but it should not constrain the AE prototype.

### Phase 1 verdict

**Technical framework fit: 9/10.**

AETK is not the risk. The product/algorithm quality is the risk.

---

## Phase 2 — Commercial validation

### The critical discovery: Adobe already ships the core concept

Adobe’s current documentation says that Roto Brush / Refine Matte includes **Reduce Chatter** specifically for erratic matte-edge motion. Adobe describes it as a weighted average across adjacent frames; higher values give surrounding frames more influence. Adobe also states that the **Refine Matte effect is available separately** for mattes created by techniques other than Roto Brush.

That overlaps directly with the original MatteLock promise.

Current Adobe references:

- Roto Brush and Refine Matte: https://helpx.adobe.com/after-effects/desktop/roto-brush-and-refine-matte/roto-brush/roto-brush-refine-matte.html
- AI-powered Object Matte: https://helpx.adobe.com/after-effects/desktop/roto-brush-and-refine-matte/roto-brush/object-matte.html
- What’s new in AE 26.5 (Object Matte disk cache): https://helpx.adobe.com/after-effects/desktop/what-s-new/whats-new.html

This is the primary reason the initial commercial score has been reduced.

### Demand / pain still appears real

There are current 2026 reports of Object Matte / Roto Brush propagation producing:

- clean initial frames followed by jagged/pixelated propagated edges
- inconsistent boundaries that look worse in motion
- edge refinements disappearing on some frames
- unstable / dispersed segmentation from frame to frame

Examples:

- https://www.reddit.com/r/AfterEffects/comments/1u1yt7u/rotobrush_produces_lowres_jagged_edges/
- https://www.reddit.com/r/AfterEffects/comments/1swkckn/anyone_else_having_pixelatedjagged_edges_with/
- https://www.reddit.com/r/AfterEffects/comments/1tgw8vx/has_anyone_been_having_issues_with_the_object/
- https://www.reddit.com/r/VideoEditing/comments/1tlch1b/rotoscope_not_working_properly_in_after_effects/

This supports the existence of a pain point, but it does **not** prove willingness to pay for a third-party solution when Adobe already provides Reduce Chatter / Refine Matte.

### Adjacent commercial products

#### Blacklight Composite Suite — $59

https://aescripts.com/blacklight-composite-suite/

Includes Smart Matte Choker and Fill Matte Holes. It is primarily spatial matte cleanup. The “Fill Matte Holes” page explicitly mentions avoiding flicker, but the suite is not marketed as a dedicated motion-aware temporal matte stabilizer.

#### EFX Keying-Alpha Plugin Suite — $49.99

https://aescripts.com/efx-keying-alpha-plugin-suite/

A large set of spatial alpha/key cleanup effects (choke, edge blur, speck cleanup, despill, etc.). Again, not a dedicated motion-aware temporal stabilization product.

#### Extendo — about €51

https://aescripts.com/extendo/

Edge-extension / edge-color repair for roto/key work. Useful adjacent evidence that users will pay around the $50–60 range for a narrowly focused matte/compositing utility, but it solves a different problem.

### Competitive position

The direct competitor that matters most is not aescripts. It is **Adobe itself**.

A paid MatteLock only makes sense if it can convincingly claim one or more of:

1. **Better temporal quality than Refine Matte / Reduce Chatter** on modern Object Matte failures.
2. **Motion-aware stabilization that avoids sticky/lagging edges** better than Adobe’s weighted-neighbor approach.
3. **Diagnostic visualization** that shows where a matte is temporally unstable and why.
4. **Automatic problem-frame detection / QC** so an artist can jump directly to frames needing manual roto correction.
5. **More predictable behavior on arbitrary imported/third-party mattes**, not only Adobe’s own segmentation workflow.
6. A substantially simpler “drop it on, inspect instability, fix it” workflow than chaining Refine Matte controls.

Without one of those, the product is redundant.

### Commercial score

| Dimension | Score |
|---|---:|
| Pain exists | 8/10 |
| Users spend on matte utilities | 7/10 |
| Direct competition | 3/10 (Adobe is built in/free) |
| AETK implementation fit | 9/10 |
| Differentiation as originally pitched | 3/10 |
| Licensing risk | 9/10 if self-contained |
| Support burden | 6/10 |
| Overall original product thesis | **4.5/10** |

### Phase 2 verdict

**NO-GO on “MatteLock = AE’s missing temporal matte stabilizer.”** That positioning is factually weak because AE already has temporal chatter reduction.

**Conditional GO** on a revised product only if testing proves a visible advantage over Adobe’s built-in Refine Matte / Reduce Chatter.

---

## Phase 4 — Algorithm design if the product is revisited

The correct technical question remains:

> Can we distinguish genuine boundary motion from frame-to-frame matte instability?

A naive temporal mean is unacceptable because it creates edge lag and ghosting.

A useful staged prototype would be:

### Stage A — Work only near the alpha transition band

Classify pixels into:

- confident background (`alpha <= bg_threshold`)
- confident foreground (`alpha >= fg_threshold`)
- transition / uncertain band

Do expensive temporal analysis primarily in the transition band plus a small dilation radius. This protects stable interiors and reduces cost.

### Stage B — Measure temporal instability

For each edge-region pixel, compute robust local temporal statistics from neighboring alpha frames, for example:

- median alpha
- median absolute deviation
- temporal sign changes
- edge-distance variance
- local gradient-direction consistency

This produces an **instability confidence map**.

### Stage C — Estimate actual motion

Before filtering, estimate local displacement between the current matte and neighboring mattes.

Prototype candidates, in increasing complexity:

1. small-window binary/alpha block matching
2. edge-distance-field matching
3. pyramidal local block matching
4. optical flow

The first prototype should avoid a huge CV dependency until simpler methods fail.

### Stage D — Motion-compensated robust temporal filter

Warp or sample neighbor mattes into current-frame coordinates, then compute a robust statistic such as a weighted median / trimmed estimate.

The blend weight should depend on:

- instability confidence
- motion confidence
- distance from current alpha boundary
- user Strength
- Motion Sensitivity
- Edge Protection

If motion confidence is low, bias strongly toward the current frame rather than inventing temporal continuity.

### Stage E — Diagnostic modes

The most commercially interesting differentiator may be diagnostic rather than merely smoothing:

- Original
- Result
- Difference
- Temporal Instability
- Motion Confidence

A strong “Temporal Instability” view could evolve into a separate QC product even if automatic stabilization is not good enough.

### Suggested prototype controls

- Strength
- Temporal Radius (1–3 initially)
- Motion Sensitivity
- Edge Protection
- View: Result / Original / Difference / Instability

Do not add presets or polished UI before quality is proven.

---

## Required empirical gate before implementation

Before investing in a full plugin, create a small benchmark set containing:

1. static subject + deliberately jittered edge
2. slowly translating edge + synthetic jitter
3. fast-moving hard edge
4. motion-blurred edge
5. fine hair / semi-transparent edge
6. intermittent 1–5 px holes / speckles
7. real AE 26.x Object Matte failure footage

For each clip compare:

- damaged/raw matte
- Adobe Refine Matte / Reduce Chatter at a carefully tuned setting
- candidate MatteLock result
- clean reference if available

Measure both visually and numerically where a reference exists:

- temporal edge variance
- IoU / alpha error to reference
- boundary displacement error
- lag on genuinely moving edges
- preservation of semi-transparent detail

### Pass condition

Proceed only if MatteLock is clearly better than Adobe on at least one commercially important class of failure **without being materially worse on legitimate motion**.

If it only matches Adobe, stop.

---

## Final recommendation

Do not write the sellable plugin yet.

The framework is ready. The market premise needs a stronger wedge.

The next valuable work is not more AETK architecture. It is a **direct quality bake-off against Adobe Refine Matte / Reduce Chatter**. If a custom motion-aware approach wins visibly, MatteLock becomes interesting again. If it does not, the research has still identified a potentially more defensible adjacent concept: a matte temporal QC / instability diagnostic tool.