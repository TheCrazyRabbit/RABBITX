"use strict";

// MANUAL CONFIGURATION: Sandbox client-side token and Sandbox monthly Price ID.
// Never paste an API key or a live token here.
const PADDLE_CLIENT_TOKEN = "test_29fd7d63174276d00fc0fa9bb8c";
const RABBITX_PRO_PRICE_ID = "pri_01m4crb2b3tqxc3d5hwmqyg0f8";

const TOKEN_PLACEHOLDER = "PASTE_TEST_CLIENT_TOKEN_HERE";
const PRICE_PLACEHOLDER = "PASTE_SANDBOX_PRICE_ID_HERE";

(() => {
  const button = document.getElementById("open-test-checkout");
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
    setStatus("Paddle Sandbox credentials not configured.");
    return;
  }

  // Basic format validation.
  if (
    !/^test_[a-zA-Z0-9]+$/.test(PADDLE_CLIENT_TOKEN) ||
    !/^pri_[a-zA-Z0-9]+$/.test(RABBITX_PRO_PRICE_ID)
  ) {
    setStatus(
      "Invalid Sandbox configuration. Use a test_ client-side token and a Sandbox pri_ Price ID."
    );
    return;
  }

  // Ensure Paddle.js loaded correctly.
  if (
    !window.Paddle ||
    !window.Paddle.Environment ||
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
    paddle.Environment.set("sandbox");

    paddle.Initialize({
      token: PADDLE_CLIENT_TOKEN,

      eventCallback: (event) => {
        if (!event) {
          return;
        }

        if (event.name === "checkout.loaded") {
          setStatus(
            "Paddle Sandbox checkout is open. Test checkout only — no real charge will be made."
          );
        } else if (event.name === "checkout.completed") {
          setStatus(
            "Sandbox test checkout completed. No real charge was made. Check Paddle Sandbox Transactions."
          );
        } else if (event.name === "checkout.closed") {
          setStatus(
            "Sandbox checkout closed. You can open another test checkout."
          );
        } else if (event.name === "checkout.error") {
          setStatus(
            "Sandbox checkout encountered an error. Check the Sandbox token, Price ID and Paddle configuration."
          );
        }
      }
    });

    initialized = true;
    button.disabled = false;
    panel.dataset.checkoutEnabled = "true";

    setStatus(
      "Ready to open Paddle Sandbox checkout. Test checkout only — no real charge will be made."
    );
  } catch (error) {
    disable();

    console.error("Paddle initialization error:", error);

    setStatus(
      "Paddle Sandbox initialization failed. Check the browser console for details."
    );

    return;
  }

  button.addEventListener("click", () => {
    if (!initialized || button.disabled) {
      return;
    }

    setStatus(
      "Opening Paddle Sandbox checkout. No real charge will be made."
    );

    const failed = (error) => {
      console.error("Paddle checkout error:", error);

      setStatus(
        "Could not open Sandbox checkout. Check the Sandbox configuration and browser console."
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

      if (result && typeof result.then === "function") {
        result.catch(failed);
      }
    } catch (error) {
      failed(error);
    }
  });
})();