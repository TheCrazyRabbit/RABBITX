# Beta recruitment drafts

These are drafts to post after the source repository and release asset have public URLs. Replace the bracketed links before posting. Ask for feedback; do not send unsolicited direct messages or post inside a target program's vulnerability reports.

## Reddit / security community

**Title:** Looking for feedback: a Windows CLI that prioritizes API endpoints for manual review

I built RABBITX, a small Windows CLI for authorized web security reviews. It extracts endpoints and parameters from fetched responses, ranks review candidates with explainable factors, and saves bounded runs as JSON/HTML reports. Scope, request budget, and minimum interval are enforced during requests.

This is an early beta, not an automatic vulnerability scanner. The demo runs against a local-only lab and the report is clearly marked as sample data. I’m looking for 10–20 researchers to try it on a target they are authorized to test and tell me where the endpoint triage or report is useful or misleading.

The current binary has no paid checkout or license gate. I’m also testing whether the proposed Pro workflow would be worth US$24/month; there is no payment request.

- Project: https://github.com/TheCrazyRabbit/RABBITX
- Windows beta: https://github.com/TheCrazyRabbit/RABBITX/releases/download/v0.8.0-beta/RABBITX-v0.8.0-beta-windows-x64.zip
- Local demos and sample report: https://github.com/TheCrazyRabbit/RABBITX/blob/main/examples/README.md

Feedback questions: Did RABBITX change what you inspected first? What did you still have to do manually? Would you pay for the proposed workflow, and what would need to be included?

## Hacker News — Show HN

**Title:** Show HN: RABBITX – prioritize risky API endpoints with scoped, budgeted requests

I built RABBITX to make the first pass through a web/API attack surface more useful. It extracts endpoints and parameters from fetched responses, assigns explainable risk factors, plans bounded probes, and produces a replayable project plus JSON or standalone HTML report.

It is a Windows CLI and currently uses rules and structured evidence; there is no AI or automatic exploit confirmation in this beta. Scope, request budgets, and request spacing are applied at the HTTP boundary. I included three local demos so people can try it without touching a public service.

I’m looking for feedback from researchers who already have authorization to test their targets. The beta has no checkout or license enforcement. I’m testing a proposed US$24/month Pro plan, but I’m not asking anyone to pay.

- Source: https://github.com/TheCrazyRabbit/RABBITX
- Download: https://github.com/TheCrazyRabbit/RABBITX/releases/download/v0.8.0-beta/RABBITX-v0.8.0-beta-windows-x64.zip
- Demo guide: https://github.com/TheCrazyRabbit/RABBITX/blob/main/examples/README.md

## Bug bounty researcher invitation

RABBITX beta feedback wanted: I’m testing a Windows CLI that turns fetched API routes and parameters into a prioritized review list, then keeps the run bounded by scope, request count, and request interval. It exports project history and standalone reports.

I’m looking for 10–20 researchers to try the local demo and, if useful, run it only on assets they are authorized to test. I’d like honest notes on missed endpoints, misleading risk scores, request behavior, and whether the report is useful in practice. No submission of target data is required.

There is no active payment flow. The proposed Pro price is US$24/month; this is a pricing question, not an offer to buy.

Links: https://github.com/TheCrazyRabbit/RABBITX · https://github.com/TheCrazyRabbit/RABBITX/releases/download/v0.8.0-beta/RABBITX-v0.8.0-beta-windows-x64.zip · https://github.com/TheCrazyRabbit/RABBITX/blob/main/examples/README.md

Before posting, check each community's current self-promotion rules and use the right feedback channel. Do not include live target URLs or sensitive program data in public feedback.
