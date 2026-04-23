# Linux 设备端送流 Demo

这个仓库是一个最小 Linux 设备端参考 Demo，代码阅读主入口是 `src/main.c`。

Demo 只演示这一条固定流程：

`启动 -> 等待客户端连接 -> 立即送固定音视频 -> 断开后继续等待`

## 目录结构

```text
.
├── 3rd/
│   ├── include/
│   │   ├── basedef.h
│   │   └── tiRTC.h
│   └── lib/
│       └── libtirtc.a
├── assets/
│   ├── audio.g711a
│   └── video.h264
├── script/
│   ├── build.sh
│   └── run_demo.sh
├── src/
│   ├── device_demo_streamer.c
│   ├── device_demo_streamer.h
│   └── main.c
├── Makefile
└── README.md
```

各目录职责如下：

- `src/main.c`：进程入口、参数解析、TiRTC 生命周期、连接管理。
- `src/device_demo_streamer.c/.h`：固定音视频文件读取、切片、循环送流。
- `script/build.sh`：编译入口。
- `script/run_demo.sh`：运行入口。
- `assets/`：固定媒体文件。
- `3rd/`：TiRTC Nano 头文件和静态库。

## 运行前需要准备什么

源码仓库默认不包含 `assets/` 和 `3rd/` 内容，这两部分需要提前下载并放到下面位置：

### 1. 下载媒体文件

放到：

```text
assets/audio.g711a
assets/video.h264
```

### 2. 下载 TiRTC Nano 头文件和静态库

放到：

```text
3rd/include/tiRTC.h
3rd/include/basedef.h
3rd/lib/libtirtc.a
```

其中 `libtirtc.a` 需要基于 `.refers/tirtc-nano/TiRTC` 自行编译得到，再放入 `3rd/lib/`。

## 编译

要求：

- Linux x86_64
- 已安装 `gcc` 和 `make`

执行：

```sh
./script/build.sh
```

编译成功后输出：

```text
build/linux_device_uplink_demo
```

如果缺少下面任一文件，`build.sh` 会直接失败：

- `3rd/include/tiRTC.h`
- `3rd/include/basedef.h`
- `3rd/lib/libtirtc.a`

## 运行

Demo 只接受三个输入参数：

- `endpoint`
- `device_id`
- `device_secret_key`

执行：

```sh
./script/run_demo.sh \
  --endpoint http://your-endpoint \
  --device-id your_device_id \
  --device-secret-key your_device_secret_key
```

例如：

```sh
./script/run_demo.sh \
  --endpoint http://ep-test-tirtc.tange365.com \
  --device-id TESTFENGJUN4 \
  --device-secret-key PRoGgX6lDelY4y9df2WW9U9NR10rBfyx
```

如果缺少下面任一文件，`run_demo.sh` 会直接失败：

- `assets/audio.g711a`
- `assets/video.h264`
- `build/linux_device_uplink_demo`

## 运行行为

启动后预期行为如下：

1. 校验启动参数和必需文件。
2. 启动 TiRTC 设备端。
3. 等待客户端连接。
4. 客户端连接后立即开始送流。
5. 首个视频发送优先输出 I 帧，尽快出图。
6. 音视频固定循环送流。
7. 客户端断开后停止当前连接送流。
8. 进程继续等待下一次连接。
