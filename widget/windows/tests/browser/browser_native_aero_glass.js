/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function waitForGlassPaint(win) {
  await win.promiseDocumentFlushed(() => {});
  await new Promise(resolve => {
    win.requestAnimationFrame(() => win.requestAnimationFrame(resolve));
  });
}

function snapshotGlassChrome(win) {
  const canvas = win.document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "canvas"
  );
  canvas.width = win.innerWidth;
  canvas.height = 160;
  const context = canvas.getContext("2d");
  context.drawWindow(
    win,
    0,
    0,
    canvas.width,
    canvas.height,
    "rgba(0, 0, 0, 0)",
    context.DRAWWINDOW_DRAW_VIEW |
      context.DRAWWINDOW_USE_WIDGET_LAYERS |
      context.DRAWWINDOW_DRAW_CARET
  );
  return (x, y) =>
    Array.from(context.getImageData(Math.floor(x), Math.floor(y), 1, 1).data);
}

async function withCaptionMaskPrefs(
  win,
  { width = 0, height = 0, radius = 0, rightRadius = 0 },
  callback
) {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "widget.native-controls.override-aero-caption-buttons-mask-width",
        width,
      ],
      [
        "widget.native-controls.override-aero-caption-buttons-mask-height",
        height,
      ],
      ["widget.native-controls.aero-caption-buttons-mask-radius", radius],
      [
        "widget.native-controls.aero-caption-buttons-mask-radius-right",
        rightRadius,
      ],
    ],
  });
  try {
    await waitForGlassPaint(win);
    await callback();
  } finally {
    await SpecialPowers.popPrefEnv();
    await waitForGlassPaint(win);
  }
}

add_task(async function native_glass_caption_mask_survives_scene_changes() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.inTitlebar", 1]],
  });
  // Widget-layer snapshots need an unoccluded native window.
  const win = await BrowserTestUtils.openNewBrowserWindow({
    features: "alwaysOnTop",
  });
  win.restore();
  await waitForGlassPaint(win);
  try {
    const appearances = ["-moz-win-borderless-glass", "-moz-win-exclude-glass"];
    await SpecialPowers.spawn(
      win.gBrowser.selectedBrowser,
      [appearances],
      values => {
        for (const appearance of values) {
          Assert.ok(
            !content.CSS.supports("appearance", appearance),
            `${appearance} remains unavailable to web content`
          );
        }
      }
    );

    const doc = win.document;
    const root = doc.documentElement;
    const style = doc.createElementNS("http://www.w3.org/1999/xhtml", "style");
    style.textContent = `
      :root[data-glass-test] {
        appearance: auto !important;
        -moz-default-appearance: -moz-win-borderless-glass !important;
        box-sizing: border-box !important;
        background: transparent !important;
      }
      :root[data-glass-test] body,
      :root[data-glass-test] #navigator-toolbox {
        background: transparent !important;
      }
      :root[data-glass-test] .titlebar-buttonbox-container {
        position: fixed !important;
        top: 0 !important;
        right: 0 !important;
      }
      :root[data-glass-test] .titlebar-buttonbox {
        appearance: -moz-window-button-box !important;
        align-self: start !important;
        margin: 0 !important;
      }
      :root[data-glass-test][sizemode="maximized"] .titlebar-buttonbox {
        appearance: -moz-window-button-box-maximized !important;
      }
      :root[data-glass-test] .titlebar-button {
        display: none !important;
      }
      #aero-mask-probe {
        position: fixed;
        inset: 0 0 auto;
        height: 160px;
        z-index: 2147483647;
        background: rgb(255, 0, 0);
        filter: invert(1);
        transform: translateZ(0);
        -moz-default-appearance: -moz-win-exclude-glass;
        pointer-events: none;
      }
      #aero-mask-probe.alternate {
        background: rgb(0, 0, 255);
      }
      #aero-opacity-probe {
        position: fixed;
        inset: auto 0 0;
        height: 64px;
        z-index: 2147483647;
        background: rgb(0, 255, 0);
        pointer-events: none;
      }
    `;
    doc.head.appendChild(style);
    const probe = doc.createElementNS("http://www.w3.org/1999/xhtml", "div");
    probe.id = "aero-mask-probe";
    doc.body.appendChild(probe);
    const footer = doc.createElementNS("http://www.w3.org/1999/xhtml", "div");
    footer.id = "aero-opacity-probe";
    doc.body.appendChild(footer);
    root.setAttribute("data-glass-test", "");
    await waitForGlassPaint(win);
    const footerIsOpaque = () =>
      win.windowUtils
        .getWidgetOpaqueRegion()
        .some(
          rect =>
            rect.left <= 32 &&
            rect.right > 32 &&
            rect.top <= win.innerHeight - 2 &&
            rect.bottom > win.innerHeight - 2
        );
    Assert.ok(
      footerIsOpaque(),
      "Opaque chrome outside the DWM margin exclusion remains opaque"
    );

    const captionRect = () =>
      [...doc.querySelectorAll(".titlebar-buttonbox")]
        .find(element => element.getBoundingClientRect().width > 0)
        .getBoundingClientRect();
    const captionPoint = () => {
      const rect = captionRect();
      return [rect.x + rect.width / 2, rect.y + rect.height / 2];
    };
    let point = captionPoint();
    let pixels = snapshotGlassChrome(win);
    Assert.deepEqual(
      pixels(32, 16),
      [0, 255, 255, 255],
      "Filtered opaque chrome is visible outside the native caption mask"
    );
    Assert.equal(
      pixels(...point)[3],
      0,
      "Native caption pixels remain transparent beneath filtered chrome"
    );
    Assert.deepEqual(
      pixels(point[0], Math.ceil(captionRect().bottom) + 1),
      [0, 255, 255, 255],
      "Restored chrome remains opaque immediately below the layout caption box"
    );

    style.textContent += `
      :root[data-tall-caption] .titlebar-buttonbox {
        height: ${captionRect().height + 12}px !important;
      }
    `;
    root.setAttribute("data-tall-caption", "");
    await waitForGlassPaint(win);
    pixels = snapshotGlassChrome(win);
    Assert.equal(
      pixels(point[0], Math.floor(captionRect().bottom) - 2)[3],
      0,
      "The restored mask follows caption layout changes without a window resize"
    );
    Assert.deepEqual(
      pixels(point[0], Math.ceil(captionRect().bottom) + 1),
      [0, 255, 255, 255],
      "A taller caption mask still stops at the layout boundary"
    );
    root.removeAttribute("data-tall-caption");
    await waitForGlassPaint(win);

    const nativeCaptionRect = captionRect();
    const maskDimensions = { width: 256, height: 96 };
    await withCaptionMaskPrefs(win, maskDimensions, async () => {
      const rect = captionRect();
      Assert.greater(
        rect.width,
        nativeCaptionRect.width,
        "The width override reserves more caption space without restarting"
      );
      Assert.greater(
        rect.height,
        nativeCaptionRect.height,
        "The height override reserves more caption space without restarting"
      );
      pixels = snapshotGlassChrome(win);
      Assert.equal(
        pixels(rect.left + 2, rect.bottom - 2)[3],
        0,
        "Both size overrides extend the visual mask beyond its native bounds"
      );
      Assert.deepEqual(
        pixels(rect.x + rect.width / 2, Math.ceil(rect.bottom) + 1),
        [0, 255, 255, 255],
        "The overridden mask stops at the caption layout's bottom"
      );

      await withCaptionMaskPrefs(
        win,
        { ...maskDimensions, radius: 32, rightRadius: 32 },
        async () => {
          pixels = snapshotGlassChrome(win);
          Assert.deepEqual(
            pixels(rect.left + 1, rect.bottom - 1),
            [0, 255, 255, 255],
            "A radius-only update preserves chrome outside the bottom-left arc"
          );
          Assert.equal(
            pixels(rect.right - 1, rect.bottom - 1)[3],
            0,
            "The right radius is ignored in restored windows"
          );
          await withCaptionMaskPrefs(
            win,
            { ...maskDimensions, radius: -1, rightRadius: -1 },
            () => {
              pixels = snapshotGlassChrome(win);
              Assert.equal(
                pixels(rect.left + 1, rect.bottom - 1)[3],
                0,
                "Negative radii leave square mask corners"
              );
            }
          );
          await withCaptionMaskPrefs(
            win,
            {
              ...maskDimensions,
              radius: 2147483647,
              rightRadius: 2147483647,
            },
            () => {
              pixels = snapshotGlassChrome(win);
              Assert.deepEqual(
                pixels(
                  rect.left + rect.height / 4,
                  rect.top + (3 * rect.height) / 4
                ),
                [0, 255, 255, 255],
                "An oversized radius is bounded by the mask height"
              );
              Assert.equal(
                pixels(rect.x + rect.width / 2, rect.bottom - 1)[3],
                0,
                "Clamped corners leave the central caption exclusion clear"
              );
            }
          );
        }
      );
    });
    Assert.deepEqual(
      [captionRect().width, captionRect().height],
      [nativeCaptionRect.width, nativeCaptionRect.height],
      "Resetting the size overrides restores the unmodified native metrics"
    );

    probe.classList.add("alternate");
    await waitForGlassPaint(win);
    pixels = snapshotGlassChrome(win);
    Assert.deepEqual(
      pixels(32, 16),
      [255, 255, 0, 255],
      "A partial scene update paints the new filtered content"
    );
    Assert.equal(
      pixels(...point)[3],
      0,
      "A partial scene update preserves the native caption mask"
    );

    const width = win.outerWidth > 900 ? 800 : 1000;
    const resized = BrowserTestUtils.waitForEvent(win, "resize");
    win.resizeTo(width, 700);
    await resized;
    await waitForGlassPaint(win);
    Assert.ok(
      footerIsOpaque(),
      "Resizing preserves opaque chrome at the new bottom edge"
    );
    point = captionPoint();
    pixels = snapshotGlassChrome(win);
    Assert.equal(
      pixels(...point)[3],
      0,
      "The native caption mask follows the resized window"
    );

    const maximized = BrowserTestUtils.waitForEvent(win, "sizemodechange");
    win.maximize();
    await maximized;
    await waitForGlassPaint(win);
    const bodyRect = doc.body.getBoundingClientRect();
    Assert.greater(
      bodyRect.top,
      0,
      "Maximized glass reserves the native frame above visible chrome"
    );
    Assert.lessOrEqual(
      Math.abs(bodyRect.bottom - win.innerHeight),
      1,
      "The native frame inset does not push browser content below the viewport"
    );
    pixels = snapshotGlassChrome(win);
    Assert.equal(
      pixels(32, bodyRect.top / 2)[3],
      0,
      "Filtered chrome cannot paint into the hidden maximized frame strip"
    );
    Assert.deepEqual(
      pixels(32, bodyRect.top + 1),
      [255, 255, 0, 255],
      "Visible maximized chrome retains its opaque background"
    );
    Assert.equal(
      pixels(...captionPoint())[3],
      0,
      "The maximized caption mask still excludes filtered chrome"
    );

    await withCaptionMaskPrefs(
      win,
      { ...maskDimensions, radius: 32, rightRadius: 32 },
      () => {
        const rect = captionRect();
        const maskBottom = rect.height;
        pixels = snapshotGlassChrome(win);
        Assert.deepEqual(
          pixels(rect.left + 1, maskBottom - 1),
          [255, 255, 0, 255],
          "The left corner stays rounded in maximized windows"
        );
        Assert.deepEqual(
          pixels(rect.right - 1, maskBottom - 1),
          [255, 255, 0, 255],
          "The right radius rounds the maximized bottom-right corner"
        );
        Assert.equal(
          pixels(rect.x + rect.width / 2, maskBottom - 1)[3],
          0,
          "A maximized rounded mask still clears the overridden height"
        );
      }
    );

    const maximizedPoint = captionPoint();
    root.removeAttribute("data-glass-test");
    await waitForGlassPaint(win);
    Assert.equal(
      doc.body.getBoundingClientRect().top,
      0,
      "Leaving maximized glass removes the native frame inset"
    );
    pixels = snapshotGlassChrome(win);
    Assert.deepEqual(
      pixels(...maximizedPoint),
      [255, 255, 0, 255],
      "Leaving maximized glass repaints the native caption exclusion"
    );
    root.setAttribute("data-glass-test", "");
    await waitForGlassPaint(win);
    Assert.greater(
      doc.body.getBoundingClientRect().top,
      0,
      "Entering glass while maximized restores the native frame inset"
    );
    pixels = snapshotGlassChrome(win);
    Assert.equal(
      pixels(...captionPoint())[3],
      0,
      "Entering glass while maximized restores the native caption exclusion"
    );

    const restored = BrowserTestUtils.waitForEvent(win, "sizemodechange");
    win.restore();
    await restored;
    await waitForGlassPaint(win);
    point = captionPoint();

    root.removeAttribute("data-glass-test");
    await waitForGlassPaint(win);
    pixels = snapshotGlassChrome(win);
    Assert.deepEqual(
      pixels(...point),
      [255, 255, 0, 255],
      "Leaving glass mode repaints the former caption exclusion"
    );
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
