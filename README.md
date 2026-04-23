# Linux 设备端送流 Demo

这是一个最小 Linux 设备端参考 Demo，主阅读入口是 `src/main.c`。

它只演示这一条固定流程：

`启动 -> 等待客户端连接 -> 立即送固定音视频 -> 断开后继续等待`

## 快速开始

要求：

- Linux x86_64
- `curl`
- `unzip`
- `python3`
- `gcc`
- `make`

执行顺序固定为：

```sh
./script/prepare.sh
./script/build.sh
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

## 三个脚本分别做什么

### `./script/prepare.sh`

作用：

1. 查询 `tangeai/tirtc-example-device` 的 latest release
2. 下载对应的 `<tag>.zip`
3. 回填当前仓库的 `assets/` 和 `3rd/`
4. 校验下面 5 个文件已经就位：
   - `assets/audio.g711a`
   - `assets/video.h264`
   - `3rd/include/tiRTC.h`
   - `3rd/include/basedef.h`
   - `3rd/lib/libtirtc.a`

查看说明：

```sh
./script/prepare.sh --help
```

### `./script/build.sh`

作用：编译生成 Demo 可执行文件。

输出：

```text
build/linux_device_uplink_demo
```

如果缺少下面任一文件，`build.sh` 会直接失败：

- `3rd/include/tiRTC.h`
- `3rd/include/basedef.h`
- `3rd/lib/libtirtc.a`

### `./script/run_demo.sh`

作用：按给定 `endpoint`、`device_id`、`device_secret_key` 拉起设备端送流 Demo。

如果缺少下面任一文件，`run_demo.sh` 会直接失败：

- `assets/audio.g711a`
- `assets/video.h264`
- `build/linux_device_uplink_demo`

## 运行后会发生什么

1. 校验启动参数和必需文件
2. 启动 TiRTC 设备端
3. 等待客户端连接
4. 客户端连接后立即开始送流
5. 首个视频发送优先输出 I 帧，尽快出图
6. 音视频固定循环送流
7. 客户端断开后停止当前连接送流
8. 进程继续等待下一次连接

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
│   ├── prepare.sh
│   ├── build.sh
│   └── run_demo.sh
├── src/
│   ├── device_demo_streamer.c
│   ├── device_demo_streamer.h
│   └── main.c
├── Makefile
└── README.md
```

各目录职责：

- `src/main.c`：进程入口、参数解析、TiRTC 生命周期、连接管理
- `src/device_demo_streamer.c/.h`：固定音视频文件读取、切片、循环送流
- `script/prepare.sh`：下载 latest release 运行时包并回填 `assets/`、`3rd/`
- `script/build.sh`：编译入口
- `script/run_demo.sh`：运行入口
- `assets/`：固定媒体文件
- `3rd/`：TiRTC Nano 头文件和静态库
