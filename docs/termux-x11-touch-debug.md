# Termux:X11 Touch & Scroll Fix Notes

## 背景

在 Termux + Termux:X11 的 `linux-host` 本地模拟中，出现了以下现象：

- 按钮点击正常（翻页/缩放可用）
- 页面滚动需要“长按后拖动”才生效，直接拖动不稳定

## 根因总结

### 1) SDL 事件队列被提前消费（导致输入异常）

`platform_sdl2.c` 的 `platform_should_quit()` 原先使用 `SDL_PollEvent()` 循环读取事件。

问题在于：LVGL SDL 输入驱动与应用层共用 SDL 全局事件队列。应用层先把事件消费掉后，LVGL 输入侧会丢失触摸/鼠标事件。

### 2) 图片对象抢占拖动手势（导致需要长按）

滚动区域中的 `g_img`（`lv_image`）默认可点击，某些触摸后端上会拦截拖动起始，导致父容器 `g_scroll_area` 无法立即进入 scroll 状态。

## 代码修复

### A. `src/platform/platform_sdl2.c`

将退出判断从“消费事件”改为“窥探事件”：

- 旧：`SDL_PollEvent()`
- 新：`SDL_PeepEvents(..., SDL_PEEKEVENT, SDL_QUIT, SDL_QUIT)`

效果：仅检测 `SDL_QUIT`，不移除队列事件，保证 LVGL 输入驱动可正常接收触控。

### B. `src/ui/ui_main.c`

1. 渲染后显式同步图片尺寸：

```c
lv_obj_set_size(g_img, (int32_t)dsc->header.w, (int32_t)dsc->header.h);
lv_obj_align(g_img, LV_ALIGN_TOP_MID, 0, 0);
```

目的：确保滚动容器能正确感知内容高度（稳定滚动范围）。

2. 让图片不抢占拖动，交由滚动容器处理：

```c
lv_obj_clear_flag(g_img, LV_OBJ_FLAG_CLICKABLE);
lv_obj_add_flag(g_img, LV_OBJ_FLAG_EVENT_BUBBLE);
lv_obj_add_flag(g_img, LV_OBJ_FLAG_GESTURE_BUBBLE);
```

目的：直接拖动即可触发滚动，不再依赖长按。

## 验证结论

在 `1280x720`、`zlib.3.pdf` 场景下验证通过：

- 缩放按钮可用
- 页面可直接拖动滚动（无需长按）
- 左右手势翻页仍可用

## 本地运行（Termux:X11）

```bash
# 先确保已安装 termux-x11-nightly 包 + Termux:X11 Android App
am start -n com.termux.x11/com.termux.x11.MainActivity
termux-x11 :0 &

export DISPLAY=:0
export XDG_RUNTIME_DIR=$PREFIX/tmp

cd /data/data/com.termux/files/home/lvgl-pdf-viewer
./build/linux-host/lvgl_pdf_viewer third_party/mupdf/thirdparty/zlib/zlib.3.pdf 1280 720
```

## 备注

调试日志代码已回收（不保留在主分支），仅保留功能修复与文档说明。
