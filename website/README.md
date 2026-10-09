# Crazy Rabbit website

Buildless static site. Deploy only `website/`, never the repository root or release archives.
Original copy and styling adapted from docs/pricing.html, PRICING.md, BETA_OUTREACH.md and CONFIGURATION.md. No C++ changes.

## Local preview
Run `python -m http.server 8787 --bind 127.0.0.1 --directory website` from the repository root and visit http://127.0.0.1:8787/.

## Cloudflare Pages
Connect TheCrazyRabbit/RABBITX in Workers & Pages → Pages → Import an existing Git repository. Choose the intended production branch, framework None, build command empty, root directory repository root, output directory website. Deploy. In the Pages project → Custom domains → Set up a custom domain, add crazyrabbit.dev and confirm the DNS record Cloudflare proposes. Verify HTTPS and all seven routes before submitting the domain to Paddle.

## Before live sales / domain submission
The site honestly describes the current Beta, proposal and disabled payment flow. Approval is not guaranteed. Review policies and identify the seller consistently with the Paddle account; confirm a suitable private support/billing contact before live sales. The GitHub public contact is for general questions only. Publish specific refund eligibility and time window, cancellation/renewal rules, final plan limits and tax information before collecting money.

## Paddle Sandbox checkout
Only website/assets/checkout.js holds the manual configuration. Search PASTE_TEST_CLIENT_TOKEN_HERE and replace it with the Sandbox client-side token (test_...). Search PASTE_SANDBOX_PRICE_ID_HERE and replace it with the Sandbox RABBITX Pro US$24/month recurring Price ID (pri_...). Never use an API key or live token. A pri_ prefix alone does not identify the environment: copy it from Sandbox.

Client-side token: Paddle Sandbox → Developer tools → Authentication → Client-side tokens (dashboard labels may also appear under account Settings → Authentication).
Price ID: Paddle Sandbox → Catalog → Products → RABBITX Pro → $24/month price → Price ID.

The button remains disabled if either value is a placeholder, the format is invalid, or the SDK cannot initialize. Paddle.Environment.set("sandbox") always precedes Initialize. Clicking opens a dark overlay with one price and quantity 1. No PricePreview, backend, API key, webhook, entitlement or license provisioning is included.

Set Paddle Sandbox → Checkout → Checkout configuration → Default payment link to https://crazyrabbit.dev/checkout/ if not already configured. After replacing the values, commit and push website/ to main for Cloudflare Pages to redeploy. Test from https://crazyrabbit.dev/checkout/ (local file browsing is for visual review, not the end-to-end Paddle test).

Use card 4242 4242 4242 4242, any valid future expiry, CVV 100, any name, an email you own and valid country/postal details. Complete the test and check Sandbox Transactions. No real payment or product access is created. Never treat a browser success event as fulfillment confirmation.

## Sources
https://developers.cloudflare.com/pages/framework-guides/deploy-anything/
https://developers.cloudflare.com/pages/configuration/custom-domains/
https://www.paddle.com/help/start/account-verification/what-is-domain-verification

https://developer.paddle.com/build/checkout/build-overlay-checkout/
