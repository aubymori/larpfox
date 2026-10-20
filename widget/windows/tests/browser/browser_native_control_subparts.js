/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function unthemed_subparts_remain_visible() {
  const snapshots = {};
  Services.scriptloader.loadSubScript(
    "chrome://mochikit/content/tests/SimpleTest/WindowSnapshot.js",
    snapshots
  );
  const html = name =>
    document.createElementNS("http://www.w3.org/1999/xhtml", name);
  const container = html("div");
  container.style.cssText =
    "position: fixed; top: 150px; left: 150px; background: white;";
  const sheet = html("style");
  container.appendChild(sheet);
  document.documentElement.appendChild(container);
  registerCleanupFunction(() => container.remove());

  for (const [tag, pseudos] of [
    ["input", ["::-moz-range-thumb", "::slider-thumb"]],
    ["progress", ["::-moz-progress-bar", "::slider-fill"]],
  ]) {
    const control = html(tag);
    control.id = "native-subpart-paint-test";
    if (tag == "input") {
      control.type = "range";
    }
    control.max = 100;
    control.value = 50;
    control.style.cssText =
      "display: block; width: 200px; height: 40px; margin: 0;";
    container.appendChild(control);
    const rules = pseudos
      .map(
        pseudo => `
      #native-subpart-paint-test${pseudo} {
        appearance: none;
        width: 32px;
        height: 32px;
        outline: 8px solid rgb(255, 0, 255);
        outline-offset: -8px;
      }
    `
      )
      .join("\n");
    sheet.textContent = rules;

    function outlinePixels() {
      const canvas = snapshots.snapshotRect(
        window,
        control.getBoundingClientRect()
      );
      const pixels = canvas
        .getContext("2d")
        .getImageData(0, 0, canvas.width, canvas.height).data;
      let count = 0;
      for (let i = 0; i < pixels.length; i += 4) {
        if (pixels[i] == 255 && pixels[i + 1] == 0 && pixels[i + 2] == 255) {
          count++;
        }
      }
      return count;
    }

    ok(outlinePixels() >= 100, `${tag}: appearance:none subpart is painted`);
    sheet.textContent += pseudos
      .map(
        pseudo => `
      #native-subpart-paint-test${pseudo} { visibility: hidden; }
    `
      )
      .join("\n");
    is(
      outlinePixels(),
      0,
      `${tag}: hidden subpart is not painted by the parent theme`
    );
    control.remove();
  }
});
