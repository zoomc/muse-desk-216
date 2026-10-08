# Fonts

`muse_font_cjk_16.c` is the CJK bitmap for captions and menus, built in with
`CONFIG_MUSE_CJK_FONT` (selected by `CONFIG_MUSE_UI_CHINESE`, default on).
It is the box3 Noto Sans CJK SC subset (16 px, 2 bpp, uncompressed, 6990
glyphs: 6763 GB2312 Han + ASCII + fullwidth + punctuation), derived from
Noto Sans CJK SC Regular 2.004 (Adobe, SIL OFL 1.1; see `OFL.txt`).
P0 port replaced the previous GNU Unifont bitmap with this file; see
`docs/PORTING.md`. Rare Han outside GB2312, most traditional characters and
Hangul are not covered. Caption paging wraps by UTF-8 code point and never
splits a multibyte character (`muse_chat_text.c` `next_line`).

To regenerate, install
[lv_font_conv](https://github.com/lvgl/lv_font_conv) **1.5.3**, download the
pinned Noto source OTF, then run from `esp32`:

```sh
python tools/muse/gen_cjk_font.py --font /path/to/NotoSansCJKsc-Regular.otf \
  --converter /path/to/node_modules/lv_font_conv/lv_font_conv.js --node node
```
