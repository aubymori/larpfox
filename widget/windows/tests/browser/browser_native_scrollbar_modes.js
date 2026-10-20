/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function scrollbar_modes_update_existing_frames() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["ui.useOverlayScrollbars", 0],
      ["widget.non-native-theme.scrollbar.enabled", 0],
      ["widget.non-native-theme.scrollbar.style", 4],
      ["widget.non-native-theme.win.scrollbar.use-system-size", false],
    ],
  });
  await BrowserTestUtils.withNewTab(
    "data:text/html," +
      encodeURIComponent(`<!doctype html>
        <style>
          .scroller, .stable {
            width: 240px; height: 180px; background: Canvas;
          }
          .scroller { overflow: scroll; }
          .stable { overflow: hidden; scrollbar-gutter: stable both-edges; }
          .scroller > div, .stable > div { width: 800px; height: 800px; }
          .light { color-scheme: light; }
          .dark { color-scheme: dark; }
          .custom { scrollbar-color: rgb(0, 255, 0) rgb(0, 0, 255); }
          .equal { scrollbar-color: rgb(128, 128, 128) rgb(128, 128, 128); }
        </style>
        <div id="light-auto" class="scroller light"><div></div></div>
        <div id="dark-auto" class="scroller dark"><div></div></div>
        <div id="light-custom" class="scroller light custom"><div></div></div>
        <div id="dark-custom" class="scroller dark custom"><div></div></div>
        <div id="light-equal" class="scroller light equal"><div></div></div>
        <div id="dark-equal" class="scroller dark equal"><div></div></div>
        <div id="stable-light-auto" class="stable light"><div></div></div>
        <div id="stable-dark-auto" class="stable dark"><div></div></div>
        <div id="stable-light-custom" class="stable light custom"><div></div></div>
        <div id="stable-dark-custom" class="stable dark custom"><div></div></div>
        <div id="stable-light-equal" class="stable light equal"><div></div></div>
        <div id="stable-dark-equal" class="stable dark equal"><div></div></div>`),
    async browser => {
      const nativeSize = await SpecialPowers.spawn(browser, [], () => {
        const scroller = content.document.getElementById("light-auto");
        return [
          scroller.offsetWidth - scroller.clientWidth,
          scroller.offsetHeight - scroller.clientHeight,
        ];
      });
      Assert.greater(nativeSize[0], 0, "Native vertical scrollbar has width");
      Assert.greater(
        nativeSize[1],
        0,
        "Native horizontal scrollbar has height"
      );

      // The override is only honored by the non-native backend. Make it
      // observably different from either native system metric, without assuming
      // a Windows theme, system DPI, or default scrollbar thickness.
      const nonNativeSize = Math.max(...nativeSize) + 20;
      await SpecialPowers.pushPrefEnv({
        set: [
          ["widget.non-native-theme.scrollbar.size.override", nonNativeSize],
        ],
      });

      // Pref transitions never change a style or reload: that would hide
      // missing live theme/layout invalidation. Separate mode-2 transitions
      // exercise style invalidation without changing the pref.
      for (const [mode, change] of [
        [0, ""],
        [1, ""],
        [2, ""],
        [2, "custom"],
        [2, "auto"],
        [2, "dark"],
        [2, "light"],
        [0, ""],
      ]) {
        if (!change) {
          await SpecialPowers.pushPrefEnv({
            set: [["widget.non-native-theme.scrollbar.enabled", mode]],
          });
        }
        await SpecialPowers.spawn(
          browser,
          [mode, change, nativeSize, nonNativeSize],
          async (currentMode, styleChange, native, nonNative) => {
            for (const id of ["light-auto", "stable-light-auto"]) {
              const scroller = content.document.getElementById(id);
              if (styleChange == "custom" || styleChange == "auto") {
                scroller.classList.toggle("custom", styleChange == "custom");
              } else if (styleChange == "dark" || styleChange == "light") {
                scroller.classList.toggle("dark", styleChange == "dark");
                scroller.classList.toggle("light", styleChange == "light");
              }
            }
            const children = node =>
              Array.from(
                content.InspectorUtils.getChildrenForNode(node, true, false)
              );
            const scrollers = [
              ...content.document.querySelectorAll(
                styleChange
                  ? "#light-auto, #stable-light-auto"
                  : ".scroller, .stable"
              ),
            ];
            const expectedSize = scroller => {
              const useNonNative =
                currentMode == 1 ||
                (currentMode == 2 &&
                  (scroller.classList.contains("dark") ||
                    scroller.classList.contains("custom") ||
                    scroller.classList.contains("equal")));
              const size = useNonNative ? [nonNative, nonNative] : native;
              return scroller.classList.contains("stable")
                ? [2 * size[0], 0]
                : size;
            };
            const thickness = scroller => [
              scroller.offsetWidth - scroller.clientWidth,
              scroller.offsetHeight - scroller.clientHeight,
            ];

            // Theme changes are delivered asynchronously to an existing
            // document; wait for the resulting layout, not a fixed delay.
            await ContentTaskUtils.waitForCondition(
              () =>
                scrollers.every(scroller =>
                  thickness(scroller).every(
                    (size, axis) => size == expectedSize(scroller)[axis]
                  )
                ),
              `Mode ${currentMode}, ${styleChange || "pref change"} updates both axes and stable gutters without reloading`
            );
            for (const scroller of scrollers) {
              Assert.deepEqual(
                thickness(scroller),
                expectedSize(scroller),
                `Mode ${currentMode}, ${scroller.id}: expected rendering backend on both axes`
              );
              if (scroller.classList.contains("stable")) {
                continue;
              }
              for (const vertical of [false, true]) {
                const label = `Mode ${currentMode}, ${scroller.id}, ${vertical ? "vertical" : "horizontal"}`;
                const scrollbar = children(scroller).find(
                  node =>
                    node.localName == "scrollbar" &&
                    node.hasAttribute("vertical") == vertical
                );
                const track = children(scrollbar).find(
                  node => node.localName == "slider"
                );
                const thumb = children(track).find(
                  node => node.localName == "thumb"
                );
                const trackRect = track.getBoundingClientRect();
                const thumbRect = thumb.getBoundingClientRect();
                Assert.greater(thumbRect.width, 0, `${label}: thumb has width`);
                Assert.greater(
                  thumbRect.height,
                  0,
                  `${label}: thumb has height`
                );
                Assert.lessOrEqual(
                  thumbRect.width,
                  trackRect.width,
                  `${label}: thumb fits track width and is not suppressed`
                );
                Assert.lessOrEqual(
                  thumbRect.height,
                  trackRect.height,
                  `${label}: thumb fits track height and is not suppressed`
                );
              }
            }
          }
        );
      }
    }
  );
});

add_task(async function non_native_scrollbar_parts_leave_the_track_visible() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["ui.useOverlayScrollbars", 0],
      ["widget.non-native-theme.scrollbar.enabled", 1],
      ["widget.non-native-theme.scrollbar.style", 4],
      ["widget.non-native-theme.scrollbar.size.override", 17],
      ["widget.non-native-theme.win.scrollbar.use-system-size", false],
      ["layout.css.devPixelsPerPx", "1.0"],
    ],
  });
  await BrowserTestUtils.withNewTab(
    "data:text/html," +
      encodeURIComponent(`<!doctype html>
        <style>
          #scroller {
            width: 240px; height: 180px; margin: 40px; overflow: scroll;
            scrollbar-color: rgb(0, 255, 0) rgb(0, 0, 255);
          }
          #scroller > div { width: 800px; height: 800px; }
        </style>
        <div id="scroller"><div></div></div>`),
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        const children = node =>
          Array.from(
            content.InspectorUtils.getChildrenForNode(node, true, false)
          );
        const snapshot = node => {
          const canvas = SpecialPowers.snapshotRect(
            content,
            node.getBoundingClientRect()
          );
          const { width, height } = canvas;
          const { data } = canvas
            .getContext("2d")
            .getImageData(0, 0, width, height);
          return {
            width,
            height,
            colorAt(x, y) {
              const offset = 4 * (y * width + x);
              return Array.from(data.slice(offset, offset + 4)).join(",");
            },
          };
        };
        const scroller = content.document.getElementById("scroller");
        for (const vertical of [false, true]) {
          const scrollbar = children(scroller).find(
            node =>
              node.localName == "scrollbar" &&
              node.hasAttribute("vertical") == vertical
          );
          const track = children(scrollbar).find(
            node => node.localName == "slider"
          );
          const thumb = snapshot(
            children(track).find(node => node.localName == "thumb")
          );
          const along = Math.floor((vertical ? thumb.height : thumb.width) / 2);
          const across = vertical ? thumb.width : thumb.height;
          for (const edge of [0, across - 1]) {
            Assert.equal(
              vertical
                ? thumb.colorAt(edge, along)
                : thumb.colorAt(along, edge),
              "0,0,255,255",
              "Both sides of the thumb expose the track color"
            );
          }
          Assert.equal(
            vertical
              ? thumb.colorAt(Math.floor(across / 2), along)
              : thumb.colorAt(along, Math.floor(across / 2)),
            "0,255,0,255",
            "The inset thumb retains its custom color"
          );
          for (const button of children(scrollbar).filter(
            node =>
              node.localName == "scrollbarbutton" &&
              node.getBoundingClientRect().width > 0
          )) {
            const image = snapshot(button);
            const colors = new Set();
            const centerLine = [];
            for (let y = 0; y < image.height; ++y) {
              for (let x = 0; x < image.width; ++x) {
                const color = image.colorAt(x, y);
                colors.add(color);
                if (
                  color == "255,255,255,255" &&
                  (vertical
                    ? x == Math.floor(image.width / 2)
                    : y == Math.floor(image.height / 2))
                ) {
                  centerLine.push(vertical ? y : x);
                }
              }
            }
            Assert.deepEqual(
              [...colors].sort(),
              ["0,0,255,255", "255,255,255,255"],
              "Scrollbar arrows have solid edges without antialiasing"
            );
            const center =
              centerLine.reduce((sum, value) => sum + value, 0) /
              centerLine.length;
            const middle = ((vertical ? image.height : image.width) - 1) / 2;
            if (button.getAttribute("type") == "decrement") {
              Assert.less(center, middle, "Arrow points up or left");
            } else {
              Assert.greater(center, middle, "Arrow points down or right");
            }
          }
        }
      });
    }
  );
});
