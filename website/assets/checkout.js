"use strict";
// New purchases are paused. No payment SDK or checkout is initialized.
(() => {
  const button = document.getElementById("subscribe-checkout");
  const status = document.getElementById("checkout-status");
  const panel = document.getElementById("paddle-checkout");
  if (button) button.disabled = true;
  if (status) status.textContent = "Paid subscriptions are currently unavailable. Free evaluation requires no payment.";
  if (panel) panel.dataset.checkoutEnabled = "false";
})();