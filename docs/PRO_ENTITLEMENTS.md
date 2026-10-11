# RABBITX Pro capabilities and release boundary

Audit date: 2026-10-11. Baseline: public v0.8.0-beta and committed main.

## Free functionality remains Free

Endpoint discovery/classification, parameter analysis, risk ranking, scope controls,
request budget/rate limit, bounded probes, evidence, project save/resume/replay,
JSON and standalone HTML reports all remain available without account sign-in.
This change does not wrap the normal audit pipeline in a subscription check.

`docs/PRICING.md` and the root README contain historical proposed plan boundaries.
Those proposals are not enforced or implemented as paid restrictions. The current
website also discloses that the public Beta has no paid feature gate. It must not
imply that already-free reports or saved projects are exclusive subscription benefits.

## Delivered by this development change

Optional email sign-in, Windows credential storage, online subscription status,
server-session logout, and a diagnostic Pro feature gate:

```text
RABBITX.exe --auth login
RABBITX.exe --auth status
RABBITX.exe --auth check
RABBITX.exe --auth logout
```

The `pro-foundation` gate is a diagnostic integration point. It does not add a
paid analysis/report capability. Its response explicitly has `commercial_feature: false`.
Existing public v0.8 binaries do not gain these commands without a new build.

## Future capabilities are not delivered

AI Triage, hosted inference, collaboration, customer portal, account dashboard,
device binding and license provisioning are not subscription benefits delivered by
this change. Concurrent uncommitted AI prototype files observed during the audit
are separate work, not a shipped, validated paid feature. Do not advertise them as Pro.

## Commercial launch blocker

There is currently **no unique deliverable Pro feature**. Authorization infrastructure
and successful billing do not resolve that product gap. A paid capability, its
support/cost limits and release must be decided and delivered before marketing Pro
as a functional upgrade. Reclassifying existing Free features requires the owner's
explicit product decision and is outside this implementation.

Website approval, verified email sending and read-only Paddle API credentials also
remain required. No real payment or $0 subscription was performed for this change.
