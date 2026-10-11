"use strict";

// MANUAL CONFIGURATION: Live client-side token (live_...) and Live monthly Price ID (pri_...).
// Never paste an API key or any server-side credential here.
const PADDLE_CLIENT_TOKEN = "live_1ce5cfd0750b4873b5a6fadb116";
const RABBITX_PRO_PRICE_ID = "pri_01m4keg9gs2tx03q6f2mk8nvfa";

const TOKEN_PLACEHOLDER = "PASTE_LIVE_CLIENT_TOKEN_HERE";
const PRICE_PLACEHOLDER = "PASTE_LIVE_PRICE_ID_HERE";

(() => {
  const button = document.getElementById("subscribe-checkout");
  const status = document.getElementById("checkout-status");
  const panel = document.getElementById("paddle-checkout");

  if (!button || !status || !panel) {
    return;
  }

  let initialized = false;

  const setStatus = (message) => {
    status.textContent = message;
  };

  const disable = () => {
    button.disabled = true;
    panel.dataset.checkoutEnabled = "false";
  };

  disable();

  // Detect only the original configuration placeholders.
  if (
    !PADDLE_CLIENT_TOKEN ||
    !RABBITX_PRO_PRICE_ID ||
    PADDLE_CLIENT_TOKEN === TOKEN_PLACEHOLDER ||
    RABBITX_PRO_PRICE_ID === PRICE_PLACEHOLDER
  ) {
    setStatus("Live Paddle checkout is not configured.");
    return;
  }

  // Basic format validation.
  if (
    !/^live_[a-zA-Z0-9]+$/.test(PADDLE_CLIENT_TOKEN) ||
    !/^pri_[a-zA-Z0-9]+$/.test(RABBITX_PRO_PRICE_ID)
  ) {
    setStatus(
      "Invalid Live configuration. Use a live_ client-side token and the Live monthly pri_ Price ID."
    );
    return;
  }

  // Ensure Paddle.js loaded correctly.
  if (
    !window.Paddle ||
    !window.Paddle.Initialize ||
    !window.Paddle.Checkout
  ) {
    setStatus(
      "Paddle.js could not load. Check your connection or content blocker, then reload this page."
    );
    return;
  }

  const paddle = window.Paddle;

  try {

    // Paddle.js defaults to Live. No environment override is used.
    paddle.Initialize({
      token: PADDLE_CLIENT_TOKEN,

      eventCallback: (event) => {
        if (!event) {
          return;
        }

        if (event.name === "checkout.loaded") {
          setStatus(
            "Paddle checkout is open."
          );
        } else if (event.name === "checkout.completed") {
          setStatus(
            "Payment completed successfully. Your subscription is being processed. Subscription access must be confirmed by the server."
          );
        } else if (event.name === "checkout.closed") {
          setStatus(
            "Paddle checkout closed."
          );
        } else if (event.name === "checkout.error") {
          setStatus(
            "Paddle checkout encountered an error. Please try again or contact support."
          );
        }
      }
    });

    initialized = true;
    button.disabled = false;
    panel.dataset.checkoutEnabled = "true";

    setStatus(
      "Ready to open Paddle checkout."
    );
  } catch (error) {
    disable();

    console.error("Paddle checkout initialization failed");

    setStatus(
      "Paddle checkout initialization failed. Please reload or contact support."
    );

    return;
  }

  button.addEventListener("click", () => {
    if (!initialized || button.disabled) {
      return;
    }

    setStatus(
      "Opening Paddle checkout..."
    );

    const failed = (error) => {
      console.error("Paddle checkout could not open");

      setStatus(
        "Could not open Paddle checkout. Please try again or contact support."
      );
    };

    try {
      const result = paddle.Checkout.open({
        items: [
          {
            priceId: RABBITX_PRO_PRICE_ID,
            quantity: 1
          }
        ],

        settings: {
          displayMode: "overlay",
          theme: "dark"
        }
      });

      Promise.resolve(result).catch(failed);
    } catch (error) {
      failed(error);
    }
  });
})();