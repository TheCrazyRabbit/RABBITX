# Crazy Rabbit website — Paddle Live checkout

Buildless static site with the existing purple theme. Deploy website/ only. No backend or C++ changes.

## Required manual configuration

File: website/assets/checkout.js

- Search PASTE_LIVE_CLIENT_TOKEN_HERE and replace it with the Paddle Live client-side token (live_...).
- Search PASTE_LIVE_PRICE_ID_HERE and replace it with RABBITX Pro's Live recurring monthly Price ID (pri_...). The prefix alone does not identify a price's environment; copy it from the Live catalog.

The user requested placeholders for this migration. Until both are filled, checkout is disabled and displays Live Paddle checkout is not configured. Only public client-side configuration belongs here; never use an API key, webhook secret or Cloudflare credential.

Paddle.js defaults to Live. There is no environment override. The official dark overlay receives one price with quantity 1. No customer is prefilled and no browser event grants Pro access. Server webhook records remain authoritative; this change implements no entitlement API or license provisioning.

## Preview and deployment

From the repository root: python -m http.server 8787 --bind 127.0.0.1 --directory website
Open http://127.0.0.1:8787/. Local file browsing also works for navigation. Test actual payment overlay opening from https://crazyrabbit.dev/checkout/ after configuring and deploying.

Cloudflare Pages imports GitHub main, framework None, output directory website. Push website changes to trigger deployment. The checkout JS URL includes a version query to avoid reusing older cached code. Bump it whenever changing the checkout configuration or script.

## Live verification

Check homepage, product, pricing, checkout and policy pages. Confirm product RABBITX Pro, US$24/month, monthly billing and final totals in Paddle checkout. Do not submit a paid transaction automatically. Any actual payment must be explicitly chosen by the user.

Paddle Live default payment link: https://crazyrabbit.dev/checkout/. Existing Live webhook URL: https://api.crazyrabbit.dev/webhooks/paddle/live. This task does not modify Worker, D1, products, tokens in the Paddle dashboard or notification destinations.

Privacy/Terms/Refund describe recurring billing, cancellation and Paddle payment processing. The public Beta still has no paid feature gate or automated license delivery; do not claim that checkout completion grants access.

## References
https://developer.paddle.com/build/checkout/build-overlay-checkout/
https://www.paddle.com/legal/privacy
https://www.paddle.com/legal/refund-policy
