# Crazy Rabbit website

Buildless static site. Deploy only `website/`, never the repository root or release archives.
Original copy and styling adapted from docs/pricing.html, PRICING.md, BETA_OUTREACH.md and CONFIGURATION.md. No C++ changes.

## Local preview
Run `python -m http.server 8787 --bind 127.0.0.1 --directory website` from the repository root and visit http://127.0.0.1:8787/.

## Cloudflare Pages
Connect TheCrazyRabbit/RABBITX in Workers & Pages → Pages → Import an existing Git repository. Choose the intended production branch, framework None, build command empty, root directory repository root, output directory website. Deploy. In the Pages project → Custom domains → Set up a custom domain, add crazyrabbit.dev and confirm the DNS record Cloudflare proposes. Verify HTTPS and all seven routes before submitting the domain to Paddle.

## Before live sales / domain submission
The site honestly describes the current Beta, proposal and disabled payment flow. Approval is not guaranteed. Review policies and identify the seller consistently with the Paddle account; confirm a suitable private support/billing contact before live sales. The GitHub public contact is for general questions only. Publish specific refund eligibility and time window, cancellation/renewal rules, final plan limits and tax information before collecting money.

## Sandbox integration slot
checkout/index.html reserves #paddle-checkout with data-payment-environment="sandbox" and data-checkout-enabled="false". No Paddle.js, token, price ID, billing form or network payment calls are included. Configure a separate explicitly labeled test flow with a real sandbox client-side token and sandbox price ID only after reviewing official Paddle documentation. Never put API keys in client files. Live enablement requires a separate deliberate implementation and policy update.

## Sources
https://developers.cloudflare.com/pages/framework-guides/deploy-anything/
https://developers.cloudflare.com/pages/configuration/custom-domains/
https://www.paddle.com/help/start/account-verification/what-is-domain-verification
