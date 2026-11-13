# 中文字体

与 Spartan 使用同一字体：Adobe 的思源黑体简体中文常规字重（Source Han Sans SC Regular），
https://github.com/adobe-fonts/source-han-sans/tree/release/OTF/SimplifiedChinese

采用 SIL OFL 1.1 授权（已附）。`tools/pack_zh_font.py` 使用 Pillow 生成界面所需的文字子集：
12 像素、4 bpp、无压缩，输出到 `platforms/passport/main/ui/zh_font.h`。修改中文文本后运行
`python tools/pack_zh_font.py` 重新生成。16 MB 的源字体没有复制到这里：脚本优先读取本目录下的
`SourceHanSansSC-Regular.otf`，否则读取 `../Spartan/assets/fonts/`，也可用 `--font` 指定。
