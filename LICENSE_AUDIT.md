# MatteLock Licensing / Dependency Audit

Date: 2026-09-12

Status: **No third-party runtime dependency or model has been approved for MatteLock.**

This file records dependencies and external material considered during the MatteLock validation work so the commercial licensing mistake that affected SWARM is not repeated.

> This is an engineering provenance record, not legal advice. Final commercial release terms should be reviewed against the actual licenses/agreements in force at release time.

## Current implementation status

MatteLock has **no shipping implementation yet**. No external source code, ML model, model weights, image/video asset, or third-party binary has been imported for the proposed product.

The research branch intentionally stopped before implementation because Adobe already ships temporal chatter reduction in Refine Matte / Roto Brush. The market case must be proven before adding product code or dependencies.

## AETK 2.0

- Source: this repository
- Repository license: AGPL-3.0
- Commercial use concern: a closed-source commercial plugin cannot simply rely on the public AGPL grant while remaining proprietary.
- Existing repository README describes a dual-licensing path for commercial/closed-source users.
- For a first-party product built by the copyright owner, preserve an internal record of which AETK files are included and confirm that all included code is legally available for relicensing.
- Before a commercial release, specifically verify whether any AETK source file contains code copied/adapted from third parties or contributed by outside authors under terms that cannot be unilaterally relicensed.

**Action before release:** perform a repository provenance review, not merely a top-level LICENSE check.

## Adobe After Effects SDK

- Type: proprietary SDK / host API
- Role: required to build a native After Effects effect plugin
- Runtime redistribution: the plugin should link/use the SDK according to Adobe's SDK agreement; do not redistribute Adobe SDK source/header packages as part of a commercial plugin unless expressly permitted.
- No Adobe SDK code was copied into MatteLock during this validation session.

**Action before release:** confirm the current Adobe After Effects SDK license/agreement for the exact SDK version used for the commercial build.

## C++ standard library

- Type: compiler/runtime standard library
- Role: normal C++20 implementation support
- Risk: normal toolchain redistribution rules apply to runtime components where applicable.
- No special MatteLock concern identified at the research stage.

## OpenCV — considered, NOT adopted

- Project: OpenCV
- Current top-level license: Apache License 2.0 for modern OpenCV releases
- Potential role: optical flow / block matching / image processing helpers
- Commercial use: Apache-2.0 is generally permissive for commercial use and binary redistribution subject to license/notice requirements.
- Important caveat: OpenCV can be built with optional third-party components. A commercial binary must audit the *actual build configuration and transitive components*, not just the OpenCV top-level license.
- Patent considerations can also be separate from copyright licensing.

**Decision:** do not add OpenCV for the first prototype unless a simpler self-contained implementation proves insufficient.

## Neural-network runtimes / model weights — explicitly NOT approved

No ONNX Runtime, TensorFlow, PyTorch, neural-network model, checkpoint, or model weight is approved for MatteLock at this stage.

Reason:

1. The product does not need AI to prove its core concept.
2. Model weights often have licensing terms separate from their inference runtime.
3. SWARM demonstrated that accidentally shipping the wrong model material can invalidate an otherwise viable commercial release.

**Rule:** no model or weights may enter the product tree without a written entry here naming the exact source, exact version/hash, exact license, commercial-use rights, redistribution rights, attribution requirements, and any downstream model/data restrictions.

## Synthetic research assets

A future MatteLock algorithm prototype may generate its own synthetic binary/alpha mattes at runtime for testing. Procedurally generated test data created by our own code introduces no external asset-license dependency.

If real footage is later added to the repository for regression testing, record:

- creator/owner
- source
- explicit permission/license
- whether redistribution in a public repository is allowed
- whether use in commercial marketing/demo material is allowed

## aescripts licensing integration

AETK already contains an optional `USE_AESCRIPTS_LIC` integration layer / stub architecture. MatteLock should not add marketplace licensing code until the algorithm and commercial premise are validated.

Before release, document:

- exact aescripts licensing SDK/version used
- license/redistribution terms
- files linked or shipped
- platform-specific binaries
- whether any generated license wrapper code has additional restrictions

## Dependency policy for MatteLock

Order of preference:

1. AETK-owned code whose relicensing provenance is confirmed
2. New code written specifically for MatteLock
3. Permissive third-party libraries with simple, auditable redistribution terms (MIT/BSD/Apache-2.0)
4. Everything else only after explicit review

Disallowed by default for the closed-source commercial build:

- GPL dependencies
- AGPL dependencies that are not first-party/relicensed
- code with unknown provenance
- model weights with ambiguous or non-commercial terms
- copied snippets without a clear license
- assets scraped from the web

## Release gate

A commercial MatteLock build must not ship until this audit includes, for every external component actually included:

- component name
- exact version/commit/hash
- source URL/vendor
- license
- commercial-use status
- binary redistribution status
- attribution/notice requirement
- source-disclosure requirement
- transitive dependency notes
- shipped file list

At the current validation stage, the cleanest path remains a self-contained classical-image-processing implementation with **zero model files and ideally zero third-party runtime dependencies**.