# Early trial delivery notes — 2026-10-20

- Delivery: RABBITX-v0.9.0-beta-trial.20261020-windows-x64.zip
- Program version remains RABBITX v0.9.0-beta. No source rebuild or executable modification.
- Source QA archive SHA-256: ed1c9ddadb843171ed1f014516d02a801a39bf8b760aba1a22ff2939856b5c49
- Packaging adds the existing license, trial guide, these notes and file checksums. Development/integration work reports are omitted. Existing local example reports are synthetic.
- Prior QA report: Debug/Release each 119 passed; native authorization 8; packaged acceptance 43. These are previous local QA results, not fresh real-customer tests.
- Case 1: 5 requests / 4 endpoints / 20 observations / 4 rule groups. Case 2: 4 / 3 / 32 / 8. Case 3: 3 / 2 / 0 / 0. Confirmed vulnerabilities: zero in all three synthetic cases.
- Current limitations: fetched-response discovery, no JavaScript browser crawler; risk candidates require manual verification; Mock is offline and no live model is enabled. No guaranteed detection or delivered paid-analysis capability is claimed.
- Source-available Beta license permits evaluation, research and internal security testing; it restricts commercial resale and commercial hosted/managed services without written permission. It is not an OSI open-source license. See LICENSE.txt for the exact terms.
- Contact: https://github.com/TheCrazyRabbit/RABBITX/issues — public non-sensitive feedback only. No credentials, private targets or raw reports.