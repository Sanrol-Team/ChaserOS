启用 TrueType/OpenType（glyf 轮廓）矢量字体：
1. 下载 stb_truetype.h（单一头文件）放到本目录：
   https://github.com/nothings/stb/blob/master/stb_truetype.h
2. 重新 CMake 配置；脚本会检测文件并定义 CNOS_HAVE_STBTT。
3. 在 iso/boot/grub/grub.cfg 中为 ISO 添加字体模块，例如：
      module /boot/font.ttf
   并将 font.ttf 复制到 iso/boot/font.ttf。

未放置 stb 时，内核仍可使用帧缓冲 + 缩放位图字体（自适应分辨率）。
