# Chinese font

The same font as Spartan: Source Han Sans SC Regular from Adobe,
https://github.com/adobe-fonts/source-han-sans/tree/release/OTF/SimplifiedChinese

Licensed under SIL OFL 1.1 (included). `tools/pack_zh_font.py` uses Pillow to
generate the subset the UI needs at 12 px, 4 bpp, uncompressed, in
`platforms/passport/main/ui/zh_font.h`. Regenerate with
`python tools/pack_zh_font.py` after changing Chinese text. The 16 MB source font
is not copied here: the script reads `SourceHanSansSC-Regular.otf` from this
folder if present, otherwise from `../Spartan/assets/fonts/`, or from `--font`.
