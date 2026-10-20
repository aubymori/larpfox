/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function author_padding_overrides_native_padding() {
  const container = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "div"
  );
  container.style.cssText = "position: fixed; top: 100px; left: 100px;";
  document.documentElement.appendChild(container);
  registerCleanupFunction(() => container.remove());

  for (const tag of ["input", "select"]) {
    const control = document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      tag
    );
    control.style.cssText = "box-sizing: content-box; width: 120px;";
    if (tag == "select") {
      const option = document.createElementNS(
        "http://www.w3.org/1999/xhtml",
        "option"
      );
      option.textContent = "Native select";
      control.appendChild(option);
    }
    container.appendChild(control);

    const nativeWidth = control.getBoundingClientRect().width;
    control.style.padding = "0px";
    const zeroWidth = control.getBoundingClientRect().width;
    ok(
      nativeWidth > zeroWidth,
      `${tag}: explicit zero replaces native padding`
    );

    control.style.padding = "20px";
    is(
      control.getBoundingClientRect().width,
      zeroWidth + 40,
      `${tag}: author padding is not overwritten by native metrics`
    );

    const clone = control.cloneNode(true);
    container.appendChild(clone);
    is(
      clone.getBoundingClientRect().width,
      zeroWidth + 40,
      `${tag}: shared styles preserve author-padding eligibility`
    );
    clone.remove();

    control.style.removeProperty("padding");
    is(
      control.getBoundingClientRect().width,
      nativeWidth,
      `${tag}: removing author padding restores native metrics`
    );
    control.remove();
  }
});
