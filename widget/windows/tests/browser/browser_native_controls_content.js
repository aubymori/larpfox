/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function dark_controls_use_the_non_native_palette() {
  await BrowserTestUtils.withNewTab(
    "data:text/html," +
      encodeURIComponent(`<!doctype html>
        <style>body { background: Canvas; }</style>
        <button>Button</button><button disabled>Disabled</button>
        <input id="paint-probe" aria-label="Paint probe">
        <input value="Text"><input disabled value="Disabled">
        <select><option>Option</option></select><textarea>Text</textarea>
        <div id="canvas" style="color: CanvasText; background: Canvas">Page</div>`),
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        const doc = content.document;
        const controls = [
          ...doc.querySelectorAll("button, input, select, textarea"),
        ];
        const colors = element => {
          const style = content.getComputedStyle(element);
          return [style.color, style.backgroundColor];
        };
        doc.documentElement.style.colorScheme = "light";
        const lightColors = controls.map(colors);
        const lightCanvas = colors(doc.getElementById("canvas"));
        doc.documentElement.style.colorScheme = "dark";
        for (const [index, control] of controls.entries()) {
          Assert.notDeepEqual(
            colors(control),
            lightColors[index],
            `${control.localName}: dark controls use the dark non-native palette`
          );
        }
        Assert.notDeepEqual(
          colors(doc.getElementById("canvas")),
          lightCanvas,
          "Page canvas colors still follow the document color scheme"
        );

        const probe = doc.getElementById("paint-probe");
        const canvas = SpecialPowers.snapshotRect(
          content,
          probe.getBoundingClientRect()
        );
        const pixel = canvas
          .getContext("2d")
          .getImageData(
            Math.floor(canvas.width / 2),
            Math.floor(canvas.height / 2),
            1,
            1
          ).data;
        Assert.equal(
          `rgb(${pixel[0]}, ${pixel[1]}, ${pixel[2]})`,
          content.getComputedStyle(probe).backgroundColor,
          "The dark text field is painted with its non-native CSS field color"
        );

        const control = controls[0];
        control.style.cssText =
          "appearance: none; color: rgb(255, 0, 0); background: rgb(0, 0, 0)";
        Assert.deepEqual(
          colors(control),
          ["rgb(255, 0, 0)", "rgb(0, 0, 0)"],
          "Explicit author colors remain intact"
        );
      });
    }
  );
});

add_task(async function native_scrollbars_reserve_layout_space() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["ui.useOverlayScrollbars", 0],
      ["widget.non-native-theme.scrollbar.enabled", 0],
    ],
  });
  await BrowserTestUtils.withNewTab(
    "data:text/html," +
      encodeURIComponent(`<!doctype html>
        <div id="scroller" style="width: 200px; height: 100px; overflow: scroll">
          <div style="width: 400px; height: 300px"></div>
        </div>`),
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        const scroller = content.document.getElementById("scroller");
        const thickness = () => [
          scroller.offsetWidth - scroller.clientWidth,
          scroller.offsetHeight - scroller.clientHeight,
        ];
        const normal = thickness();
        Assert.greater(normal[0], 0, "Vertical scrollbar reserves its width");
        Assert.greater(
          normal[1],
          0,
          "Horizontal scrollbar reserves its height"
        );
        const children = node =>
          Array.from(
            content.InspectorUtils.getChildrenForNode(node, true, false)
          );
        const checkThumbsFit = () => {
          for (const vertical of [false, true]) {
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
            Assert.lessOrEqual(
              thumbRect.width,
              trackRect.width,
              "Native thumb width fits its track so layout does not suppress painting"
            );
            Assert.lessOrEqual(
              thumbRect.height,
              trackRect.height,
              "Native thumb height fits its track so layout does not suppress painting"
            );
          }
        };
        checkThumbsFit();
        scroller.style.scrollbarWidth = "thin";
        const thin = thickness();
        for (let axis = 0; axis < 2; ++axis) {
          Assert.greater(thin[axis], 0, "Thin scrollbars still reserve space");
          Assert.lessOrEqual(
            thin[axis],
            normal[axis],
            "Thin is no wider than normal"
          );
        }
        checkThumbsFit();
        scroller.style.scrollbarWidth = "none";
        Assert.deepEqual(
          thickness(),
          [0, 0],
          "Hidden scrollbars reserve no space"
        );
        scroller.scrollTo(100, 100);
        Assert.deepEqual(
          [scroller.scrollLeft, scroller.scrollTop],
          [100, 100],
          "Hiding native scrollbars does not disable scrolling"
        );
      });
    }
  );
});

add_task(async function dark_range_and_progress_paint_subparts_once() {
  await BrowserTestUtils.withNewTab(
    "data:text/html," +
      encodeURIComponent(`<!doctype html>
        <style>
          body { color-scheme: dark; background: Canvas; }
          input, progress {
            display: block;
            color-scheme: dark;
            accent-color: rgb(0, 255, 0);
            width: 200px;
            height: 40px;
            margin: 20px;
            pointer-events: none;
          }
          input::-moz-range-thumb {
            outline: 4px solid rgb(255, 0, 255);
            outline-offset: -4px;
          }
          input::slider-thumb {
            outline: 4px solid rgb(255, 0, 255);
            outline-offset: -4px;
          }
          progress::-moz-progress-bar {
            outline: 4px solid rgb(255, 0, 255);
            outline-offset: -4px;
          }
          progress::slider-fill {
            outline: 4px solid rgb(255, 0, 255);
            outline-offset: -4px;
          }
        </style>
        <input type="range" min="0" max="100" value="25">
        <progress max="100" value="25"></progress>`),
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        for (const control of content.document.querySelectorAll(
          "input, progress"
        )) {
          for (const value of [25, 75]) {
            control.value = value;
            const rect = control.getBoundingClientRect();
            const canvas = SpecialPowers.snapshotRect(content, rect);
            const context = canvas.getContext("2d");
            const pixels = context.getImageData(
              0,
              0,
              canvas.width,
              canvas.height
            ).data;
            const label = `${control.localName} at ${value}%`;
            let outlinePixels = 0;
            for (let i = 0; i < pixels.length; i += 4) {
              if (
                pixels[i] == 255 &&
                pixels[i + 1] == 0 &&
                pixels[i + 2] == 255
              ) {
                outlinePixels++;
              }
            }
            Assert.equal(
              outlinePixels,
              0,
              `${label}: themed anonymous subparts are not painted again over the parent`
            );
            const rgbAt = (x, y) =>
              Array.from(context.getImageData(x, y, 1, 1).data).slice(0, 3);
            Assert.equal(
              `rgb(${rgbAt(100, 4).join(", ")})`,
              content.getComputedStyle(content.document.body).backgroundColor,
              `${label}: non-native drawing is centered, not stretched to the control height`
            );
            Assert.deepEqual(
              rgbAt(25, 20),
              [0, 255, 0],
              `${label}: the filled track is visible at its expected position`
            );
            if (value == 25) {
              Assert.notDeepEqual(
                rgbAt(125, 20),
                [0, 255, 0],
                `${label}: the unfilled track is not painted as filled`
              );
            } else {
              Assert.deepEqual(
                rgbAt(125, 20),
                [0, 255, 0],
                `${label}: changing the value updates the painted fill geometry`
              );
            }
          }
        }
      });
    }
  );
});
