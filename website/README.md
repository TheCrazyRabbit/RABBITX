# Crazy Rabbit website — free early evaluation

Buildless static site. Cloudflare Pages imports GitHub main, framework None, output directory website. Deploy only website changes; do not include backend or payment-account changes.

New paid checkout is paused. All pages state that paid subscriptions are unavailable. The checkout page has no Paddle SDK, purchase button or payment script. assets/checkout.js is an inert pause fallback for older pages. Existing-transaction support links remain in the policies.

/trial/ documents the early QA-derived v0.9 trial, authorization limits, license and SHA-256. /downloads/ contains the ZIP, checksum and plain-text documentation. This is not a formal commercial release. The executable is unchanged from v0.9.0-beta-qa.20261019. Public main source and the older GitHub v0.8 release remain distinct from this QA-derived trial.

Recruitment posts are pending separate user approval. Hosting the package does not mean anyone has enrolled. Support for the planned three-person cohort is arranged individually for 72 hours; the program does not expire after three days.

Before deployment, verify local links, absence of active payment SDKs/buttons, ZIP contents and both outer ZIP and extracted-file checksums. After deployment, verify the live pages and download integrity. Do not submit payments or change Paddle settings.