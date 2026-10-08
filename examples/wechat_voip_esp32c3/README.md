# TiRTC ESP32-C3 微信 VoIP 快速验证示例

版本：**0.1.0**

环境：**ESP32-C3 / ESP-IDF 5.5.5 / TiRTC 2.5.0 / 微信正式版 / 2 MB Flash**

本工程用于快速验证完整的设备联网、六位验证码绑定、微信联系人授权、双向呼叫和 Opus 音频收发。示例不依赖屏幕、摄像头、麦克风、扬声器或 PSRAM：设备上行发送 Flash 中的预录 Opus，下行接收微信侧 Opus 并输出统计和可导出数据。

## 1. 准备环境

- ESP32-C3 开发板，Flash 不小于 2 MB。
- 2.4 GHz Wi-Fi。
- 已安装 ESP-IDF 5.5.5。
- 已开通微信 VoIP 能力的开发者平台账号和正式版小程序。

工程已经包含 TiRTC 2.5.0 的 ESP32-C3 头文件和静态库，不要混用其他芯片、其他 ESP-IDF ABI 或其他版本 SDK。

## 2. 编译与烧录

```sh
idf.py build
idf.py -p COMx flash monitor
```

将 `COMx` 替换为实际串口。Linux 可使用 `/dev/ttyACM0`，串口波特率为 115200。

常规更新不要执行整片擦除，否则会清除 NVS 中的 Wi-Fi 和设备身份。

## 3. 配网与六位码绑定

串口输入：

```text
wifi {"ssid":"YOUR_SSID","password":"YOUR_PASSWORD"}
```

设备联网后会输出六位验证码：

```text
BIND CODE: 123456
```

在开发者平台或配套小程序的“验证码绑定”页面输入这六位数字。验证码在平台侧输入，不需要再次发送给串口。看到下面的日志后表示设备身份已经保存，并已切换到正式 MQTT：

```text
binding persisted and grant ACK PUBACK confirmed
```

随后在正式版小程序完成微信通话授权，并在串口执行：

```text
id
contacts
status
```

`device_id` 应非空，联系人数量应大于 0，运行状态应满足 `platform=1 rtc=1 profile=1`。验证码过期或网络中断时，恢复网络后执行 `retry`，不要通过清空 NVS 反复试错。

## 4. 快速验证

BOOT 按键默认使用 GPIO9，低电平有效：

| 当前状态 | 按一次 BOOT |
|---|---|
| 空闲 | 呼叫第 1 个微信联系人 |
| 收到来电 | 接听 |
| 呼叫中、等待接听或通话中 | 挂断 |

也可以使用串口命令：

| 命令 | 作用 |
|---|---|
| `contacts` | 刷新已授权联系人 |
| `call 1` | 呼叫第 1 个联系人 |
| `answer` | 接听来电 |
| `reject` | 拒绝来电 |
| `hangup` | 结束当前呼叫或通话 |
| `status` | 查看平台、RTC、呼叫和内存状态 |
| `memory` | 输出内存水位 |
| `rx-record` | 录制约一秒微信下行 Opus 包 |
| `rx-dump` | 空闲时导出录制数据 |
| `retry` | 在失败原因消除后显式重试 |
| `help` | 显示命令帮助 |

验证顺序：

1. 空闲按 BOOT，确认正式版微信收到呼叫。
2. 微信接听，确认设备收到 `0x2000` 并开始双向 Opus。
3. 微信端确认可以听到设备预录音频。
4. 设备确认持续收到微信下行 Opus 包。
5. 通话中按 BOOT，确认双方结束通话，设备回到 `idle`。
6. 从微信呼叫设备，使用 BOOT 或 `answer` 接听；使用 BOOT 或 `hangup` 挂断。

## 5. 成品效果日志

下面是示例正常链路的精简日志格式。验收时以当前构建和当前设备实际输出为准。

### 设备发起呼叫并接通

```text
BOOT: idle -> call contact 1
DIALING: contact=1 wx_version_type=0
thing_http: done: http=200 code=0 result=ESP_OK
voip: outbound room arrived; connect immediately, wait for 0x2000/0x2001
voip: WHIP connected: state=waiting-peer
voip: SIGNAL | cmd=0x2000 state=waiting-peer
audio: START | TX=C3->WeChat RX=WeChat->C3 codec=Opus rate=16000Hz channels=1
audio: TX FIRST | C3->WeChat codec=Opus stream=10 payload=60B
audio: RX FIRST | WeChat->C3 stream=0 media=4(Opus) packet=9B rate=16000Hz channels=1
voip: FLOW | waiting-peer->active
```

| 日志 | 含义 |
|---|---|
| `wx_version_type=0` | 使用微信正式版 |
| `http=200 code=0` | 业务服务器接受设备呼叫请求 |
| `WHIP connected` | 设备与 TiRTC 微信 VoIP 房间连接成功，此时仍等待微信接听 |
| `cmd=0x2000` | 微信侧已经接听，设备从此刻开始计时并启动媒体 |
| `TX FIRST` | 设备到微信的第一包 Opus 已提交 |
| `RX FIRST` | 微信到设备的第一包 Opus 已收到 |

### 设备挂断并完成回收

```text
BOOT: active -> hangup
audio: STOP | active_ms=5329
audio: TOTAL | TX packets=267 failed=0 backlog_drop=0 | RX packets=259 bad_format=0
voip: HANGUP submit
peer_connection state change e=7
voip: call resources ready
voip: FLOW | closing->idle
```

| 日志 | 含义 |
|---|---|
| `failed=0 backlog_drop=0` | 上行发送无失败、无积压丢弃 |
| `bad_format=0` | 下行 Opus 格式检查通过 |
| `e=7` | RTC 连接进入断开状态 |
| `call resources ready` | 本轮连接及媒体资源已经释放，可再次呼叫 |
| `closing->idle` | 呼叫状态机回到空闲 |

## 6. 音频格式

| 方向 | 格式 |
|---|---|
| 设备到微信 | Flash 预录 Opus，16 kHz、单声道、24 kbit/s CBR、20 ms/包 |
| 微信到设备 | Opus packet，16 kHz、单声道、20 ms/包 |

预录文件位于 `components/wechat_voip/assets/audio.opuspkt`，它存放在 Flash，不占主 SRAM。替换音频：

```sh
python tools/prepare_opus.py voice.wav components/wechat_voip/assets/audio-new.opuspkt
```

下行导出：通话中执行 `rx-record`，回到空闲后执行 `rx-dump`，保存串口日志后运行：

```sh
python tools/rx_to_wav.py serial.log received.opus
python tools/rx_to_wav.py serial.log received.wav
```

## 7. 内存与固件空间

ESP-IDF 链接结果：应用静态 DRAM 127710 B；应用固件 1379952 B，2 MB Flash 的应用分区剩余 651664 B。

下表运行时水位来自同一工程前一组 SDK 构建，用于估算产品集成余量，不属于本次静态库的真机内存验收结果。更换 SDK 或应用配置后必须重新测量。

| 状态 | 剩余空闲 | 最大连续块 |
|---|---:|---:|
| 联网待机 | 131248 B | 114688 B |
| 已接通通话最低点 | 73460 B | 69632 B |
| 包含挂断回收的全流程最低点 | 47604 B | 43008 B |

实测最低水位不是新增功能的完整预算。接入实时编解码、I2S、AEC 或更大的网络缓存后，应重新测量空闲内存、最大连续块和任务栈余量。

## 8. 工程结构与移植范围

### 8.1 目录职责

| 目录或文件 | 职责 | 移植时的处理方式 |
|---|---|---|
| [`main/app_main.c`](main/app_main.c) | 应用入口、串口命令、启动顺序 | 替换成目标产品入口时，只调用公开接口，不直接操作 TiRTC 句柄 |
| [`main/app_config.h`](main/app_config.h) | Wi-Fi、服务发现地址、小程序版本、BOOT GPIO | 保留为应用配置；私有值放到不提交的 `app_config.local.h` |
| [`main/boot_button.c`](main/boot_button.c) | BOOT 消抖和主操作映射 | 没有按键时可换成 UI/按键事件，最终仍调用 `wechat_voip_command()` |
| [`components/demo_platform/`](components/demo_platform/) | Wi-Fi、校时、NVS、服务发现、设备绑定、HTTP、MQTT | 对接目标网络和存储平台时主要改这一层 |
| [`components/wechat_voip/`](components/wechat_voip/) | 联系人镜像、主被叫状态机、微信信令、Opus 示例媒体 | 保留业务状态机；实时音频只替换媒体实现 |
| [`components/rtc_adapter/`](components/rtc_adapter/) | TiRTC 生命周期、WHIP、回调转事件、媒体发送 | TiRTC API 的唯一调用边界，不应散落到 UI 或网络层 |
| [`components/tirtc_sdk/`](components/tirtc_sdk/) | TiRTC 2.5.0 头文件、静态库、版本锁 | 整体复制，不混用其他芯片、IDF ABI 或版本文件 |
| [`partitions.csv`](partitions.csv) | 2 MB Flash 分区 | 目标产品可重新规划，但应用分区必须容纳实际固件并保留升级余量 |
| [`tools/`](tools/) | Opus 准备和下行音频导出 | 可选复制，不参与设备运行 |

### 8.2 分层边界

```text
产品 UI / 按键 / CLI
        |
        v
wechat_voip_command()              只提交呼叫意图
        |
        v
微信 VoIP 控制任务                 拥有呼叫状态、代次和当前连接
   |                         |
   v                         v
demo_platform               rtc_adapter
绑定/HTTP/MQTT               TiRTC 生命周期/WHIP/媒体 API
   |                         |
   +------------+------------+
                v
          云端服务与微信 VoIP
```

移植时必须保持以下所有权：

- `demo_platform` 拥有网络、设备身份、正式 MQTT token 和服务地址。
- `wechat_voip` 拥有一次呼叫的方向、状态、`generation`、房间参数和挂断收敛。
- `rtc_adapter` 拥有 TiRTC 生命周期和异步 WHIP 入参，业务层不能直接释放其输入缓冲。
- 媒体回调只处理当前连接；旧代次、旧句柄和迟到事件必须忽略或释放。
- 联系人来自服务端授权列表，只保存在运行时内存，不写入 NVS，不在设备侧自行增加或删除。

## 9. 移植前置条件

### 9.1 固定版本与 ABI

开始修改前先锁定以下组合：

| 项目 | 本示例基线 | 验证方法 |
|---|---|---|
| 芯片 | ESP32-C3，RISC-V | `CONFIG_IDF_TARGET="esp32c3"` |
| ESP-IDF | 5.5.5 | `idf.py --version` |
| TiRTC | 2.5.0 | 启动日志中的 `TiRtcGetVersion()` |
| Flash | 2 MB | `CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y` |
| 分区 | factory `0x10000/0x1f0000` | `partitions.csv` 和构建输出 |
| 音频 | Opus、16 kHz、单声道、20 ms/包 | 首包日志与离线解码 |
| 小程序 | 正式版，`wx_version_type=0` | `DIALING` 日志 |

`components/tirtc_sdk/release-lock.json` 会校验静态库 SHA256。链接库、头文件和 `sdkconfig` 任一发生变化，都要清理构建目录后重新完成全部验收，不能只看增量编译是否通过。

### 9.2 目标平台必须提供的能力

- 可用的 TCP/IP、DNS、TLS、HTTP、MQTTS 和可信根证书。
- 准确的 Unix 时间；HMAC 请求要求设备时间与服务器偏差不超过 300 秒。
- 可持久化保存 Wi-Fi、`device_id` 和 `device_key` 的安全存储。
- 至少一个非阻塞控制任务、事件队列和毫秒级单调时钟。
- 能取得稳定、唯一的 `client_id`。本示例使用 `c3-<Wi-Fi MAC>`，更换网络不能改变物理设备身份。
- 足够的内部 RAM、最大连续块和任务栈余量；总空闲内存不能代替最大连续块检查。

## 10. 分步移植

### 10.1 集成 SDK 组件

本示例已经包含可直接用于 ESP32-C3 / ESP-IDF 5.5.5 的 TiRTC 2.5.0 预编译组件。

1. 原样复制 `components/tirtc_sdk`，不要单独替换其中的头文件或静态库。
2. 复制 `components/rtc_adapter`、`components/wechat_voip` 和所需的平台层文件。
3. 在目标组件的 CMake 依赖中加入 `tirtc_sdk`、`lwip`、`mbedtls`、`esp_timer`、`json`、`mqtt` 和网络组件。
4. 保留 `tirtc_sdk/CMakeLists.txt` 中的库哈希检查，确保工程实际链接随包 SDK。
5. 先使用本示例配置完成干净构建；每次只改变一类产品配置并重新执行对应验收。

以下配置属于本基线的重要组成部分：

- `CONFIG_FREERTOS_HZ=1000`
- `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y`
- 关闭 TLS 1.3 和 `MBEDTLS_SSL_KEEP_PEER_CERTIFICATE`
- `CONFIG_COMPILER_OPTIMIZATION_SIZE=y`
- 自定义 2 MB 分区表

更换芯片或 ESP-IDF 主版本时，应使用对应平台正式提供的 TiRTC SDK，不能复用本包静态库。文件名相同不能证明 ABI 相同。

### 10.2 接入网络、校时和服务发现

目标顺序：

```text
Wi-Fi 获得 IP
-> SNTP 校时
-> GET https://ep-open.tangeopen.com/services
-> 保存本次返回的 device-srv / voip-srv / mqtt-srv / tirtc-srv
```

本示例拒绝服务发现结果中的明文地址：设备、VoIP 和 TiRTC 地址必须是 `https://`，MQTT 必须是 `mqtts://`。不要把解析出的业务服务器地址写死到固件；私有部署只替换 `APP_DISCOVERY_URL`。

网络层迁移检查：

- Wi-Fi 断开回调不能执行 HTTP、NVS 或 TiRTC Stop，只更新网络状态并通知控制任务。
- 校时失败时不得继续生成 HMAC 请求。
- HTTP 超时为 10 秒，禁止自动重定向，响应 JSON 上限为 4096 B。
- HTTP 响应必须同时检查传输结果、HTTP 状态码和业务 `code`。
- device-server 成功业务码为 `200`；voip-server 成功业务码为 `0`，不要混为同一个判断。
- TLS 证书校验必须保留，不能通过改成 HTTP 或关闭校验来绕过问题。

### 10.3 完成六位验证码绑定

首次启动或设备已解绑时执行：

```text
POST /v1/device/report
-> 取得 code / temp_client_id / temp_token
-> 使用临时身份连接 MQTTS
-> 订阅 device/<temp_client_id>/cmd
-> 打印六位 BIND CODE
-> 收到 auth_grant
-> 在平台任务中把完整 device_id + device_key 提交到 NVS
-> 发布 ACK 并等待 PUBACK
-> 关闭临时 MQTT
```

NVS 命名空间为 `voip_demo`，键为 `wifi` 和 `identity`。迁移时应保留“完整身份一次提交、提交成功后再 ACK”的顺序。不要在 MQTT 回调里写 Flash；掉电发生在保存或 ACK 中间时，设备必须能够重新进入绑定流程，而不是留下半份身份。

已绑定设备通过 `POST /v1/device/token` 获取正式 `mqtt_token`。签名格式为：

```text
Base64(HMAC-SHA256(device_key, device_id + timestamp + nonce))
```

请求头包括 `X-Device-Id`、`X-Timestamp`、`X-Nonce`、`X-Signature`；token 请求额外携带真实 `X-Mac`。`device_key`、HMAC 原文、签名和 token 都不能输出到日志。

### 10.4 接入正式 MQTT

正式连接参数：

| 项目 | 值 |
|---|---|
| Client ID | `sn_<device_id>` |
| Username | `device_id` |
| Password | `/v1/device/token` 返回的 `mqtt_token` |
| 上行心跳 | `device/<client_id>/up` |
| 下行命令 | `device/<client_id>/cmd`，QoS 1 |
| 下行通知 | `device/<client_id>/notify`，QoS 1 |
| 命令 ACK | `device/<client_id>/ack`，QoS 1 |

MQTT 分片必须按 `current_data_offset` 顺序重组，确认总长度小于 4096 B，再交给业务任务。只有业务事件成功进入有界队列后才 ACK；队列满、消息过大、JSON 无效或内存申请失败时不 ACK，使上游能够重发。

正式 MQTT 就绪后才能执行 profile、联系人和呼叫请求。token 过期前 60 秒重新登录；断网、鉴权失败和解绑属于不同状态，不能统一清空 NVS。

### 10.5 接入 TiRTC 生命周期

本工程集中在 [`rtc_adapter.c`](components/rtc_adapter/rtc_adapter.c) 调用 TiRTC。启动顺序必须保持：

```text
TiRtcSetOption(TIRTC_OPT_MAX_SEND_BUFFER, 16 KiB)   // 必须在 Init 前
TiRtcSetOption(TIRTC_OPT_MAX_CONNECTIONS, 1)
TiRtcInit()
TiRtcLogSetLevel(...)
TiRtcSetOption(TIRTC_OPT_DEVICE_SECRET_KEY, device_key)
TiRtcSetOption(TIRTC_OPT_CLIENT_ID, stable_client_id)
TiRtcSetOption(TIRTC_OPT_SERVICE_ENDPOINT, discovered_tirtc_url)
TiRtcStart(device_id, &callbacks)
-> 等待 TIRTC_EVENT_SYS_STARTED
```

`TiRtcStart()` 返回 `0` 仅表示请求已提交。只有收到 `TIRTC_EVENT_SYS_STARTED`，RTC 才能进入可用状态。回调结构体必须是全生命周期静态对象，不能放在启动函数的栈上。

停止顺序：

```text
禁止新呼叫
-> 停止媒体输入
-> TiRtcDisconnect(current_hconn)
-> 等待 on_disconnected / RTC_CLOSED
-> TiRtcStop()
-> 等待 TIRTC_EVENT_SYS_STOPPED
-> TiRtcUninit()
```

普通通话结束只断开当前 `hconn`，不要每次通话都 Stop/Uninit。解绑、整体网络身份切换或应用真正退出时才停止 SDK。

### 10.6 上报能力并同步联系人

正式 MQTT 与 TiRTC 均就绪后：

1. `POST /v1/device/profile` 上报 VoIP 能力。
2. `GET /v1/voip/device/contacts` 拉取已授权联系人。
3. 收到 `callers_update` 时废弃旧列表并重新拉取。

本示例上报：16 kHz、单声道、下行 Opus、无视频、30 秒响铃窗口。最多缓存 8 个联系人，字段为 `wx_open_id`、`wx_app_id` 和 `remark`。目标产品需要更多联系人时，可以调整上限，但必须重新评估静态 RAM 和响应 JSON 上限。

### 10.7 设备呼叫微信

```text
空闲
-> POST /v1/voip/device/call
-> 状态进入 dialing，并先固定本次 generation、openid、appid
-> 收到与本次目标匹配的 call_incoming
-> 立即 TiRtcWhipConnect(peer_id, token, ...)
-> WHIP 成功后进入 waiting-peer
-> 收到 0x2000
-> 进入 active，启动媒体和通话计时
```

`call_incoming` 可能早于 HTTP 响应到达，因此必须在提交 HTTP 前先占用本次呼叫代次和方向。设备呼叫微信时，WHIP 不等待小程序接听；收到房间参数后立即建连，微信接听结果由 `0x2000`/`0x2001` 决定。

### 10.8 微信呼叫设备

```text
MQTT 收到 call_incoming
-> 校验 room、peer_id、token、房间类型和当前状态
-> 状态进入 ringing，等待本地 answer/reject
-> answer：TiRtcWhipConnect(peer_id, token, ...)
-> WHIP 成功并收到 0x2000
-> 进入 active，启动媒体和通话计时
```

来电 JSON 是借用数据，离开 MQTT 回调前必须复制；`peer_id`、token 和回调 payload 不能保存为指向 MQTT 缓冲区的裸指针。重复投递的同一房间要幂等处理，已关闭房间或旧 `generation` 的迟到事件不能重新激活通话。

### 10.9 拒接、取消和挂断

| 场景 | 正确动作 | 完成条件 |
|---|---|---|
| 来电未接时本地拒接 | 使用来电字段调用 `/v1/wxvoip/reject` | 服务请求完成，状态回到 `idle` |
| 已有 WHIP 时本地挂断 | `TiRtcSendCommand(hconn, 0x2001, {"reason":n})`，短暂让命令出队后 `TiRtcDisconnect()` | 收到 `RTC_CLOSED`，释放当前句柄 |
| 收到远端 `0x2001` | 校验 JSON 中的 `reason`，停止媒体并断开 | 不再发送媒体，最终回到 `idle` |
| 微信来电随后取消 | 匹配当前 `wx_room_id` 后结束 | 其他房间的取消消息被忽略 |
| WHIP 失败或超时 | 保留第一错误码并结束本代次 | 不自动重拨，不复用 token |

关闭流程必须幂等。应用一旦提交 `TiRtcDisconnect()`，立即停止使用该句柄；`on_disconnected` 只作为该句柄已经释放的完成证据。

### 10.10 替换真实音频

当前 [`sample_audio.c`](components/wechat_voip/sample_audio.c) 是媒体边界示例：上行从 Flash 读取完整 Opus 包，下行只验证并录制编码包。接入麦克风和扬声器时，保留 `wechat_voip` 状态机，只替换这一个媒体实现。

上行要求：

- 在 `CALL_ACTIVE` 后启动采集和编码，在离开 active 时立即停止。
- 使用 `TIRTC_AUDIO_OPUS`、`TIRTC_AUDIOSAMPLE_16K16B1C`、单声道 16 kHz。
- 每次发送一个完整 Opus 包，目标包时长 20 ms；不要把多个包拼接成一个 SDK frame。
- 使用订阅回调给出的 `stream_id`；收到取消订阅后立即停止该路发送。
- 发送前检查 `TiRtcGetSendBufferUsed()`，队列增长时丢弃过期实时音频，不能突发补发旧包。
- `TiRtcSendAudioStream()` 正数表示接受的字节数，`0` 或负数按失败统计。

下行要求：

- `on_audio` 中的 frame 和 payload 只在回调期间有效；异步解码前复制到有界队列。
- 回调中不解码、不播放、不写文件、不等待锁和网络。
- 解码器输出的 PCM 规格必须与音频驱动一致；变采样、声道转换和音量控制放在媒体驱动层。
- 队列满时按实时性策略丢最旧数据并记录计数，不能无限堆积。
- 真实双向设备应同步验收 AEC 参考、处理耗时和采集溢出；本示例没有实现 AEC。

### 10.11 线程、队列和内存所有权

本示例关键资源：

| 资源 | 当前配置 | 所有者 |
|---|---:|---|
| 平台控制任务 | 8192 B 静态栈 | `demo_platform` |
| VoIP 控制任务 | 16384 B 静态栈 | `wechat_voip` |
| MQTT 任务 | 6144 B 栈 | ESP-MQTT |
| 平台 HTTP 请求队列 | 2 项 | `demo_platform` |
| VoIP 用户/平台事件队列 | 8 项 | `wechat_voip` |
| RTC 事件队列 | 16 项 | `wechat_voip` |
| MQTT/HTTP JSON 上限 | 4096 B | `demo_platform` |
| WHIP peer/token 上限 | 2048/1024 B | `rtc_adapter` |

这些值是已经验证的基线，不是通用最小值。移植时不能为了省内存直接缩小栈、JSON 或 token 容量。先记录任务栈高水位、内部 RAM 最低水位和最大连续块，再据实调整。

SDK、MQTT 和网络回调只做校验、复制和投递；NVS、HTTP、服务请求、状态转换和断开动作全部放到控制任务。任何回调投递失败都必须形成明确错误，不能静默吞掉后继续运行旧状态。

### 10.12 应用层接入

产品 UI、按键或上位机只调用：

```c
wechat_voip_command(VOIP_CALL, contact_index);
wechat_voip_command(VOIP_ANSWER, 0);
wechat_voip_command(VOIP_REJECT, 0);
wechat_voip_command(VOIP_HANGUP, 0);
wechat_voip_command(VOIP_CONTACTS, 0);
```

接口是异步命令提交，`ESP_OK` 只表示进入队列。UI 状态应由业务事件或新增的只读状态通知驱动，不能因按钮调用返回成功就显示“已接通”。通话时长从收到 `0x2000`、状态进入 `active` 后开始计算。

## 11. 常见移植错误

| 现象 | 第一检查点 | 正确处理 |
|---|---|---|
| `TiRtcStart()` 返回 0 但不能呼叫 | 是否收到 `SYS_STARTED` | 把异步事件作为启动完成依据 |
| `40012` 或 WHIP 鉴权失败 | `peer_id/token` 是否来自同一次 `call_incoming` | 不裁剪、不拼接、不复用旧 token |
| 微信已接听但设备仍显示呼叫中 | 是否收到并处理 `0x2000` | WHIP 成功与对端接听是两个阶段 |
| 微信挂断后设备等到心跳超时 | `on_command` 是否收到 `0x2001` | 保留命令回调和房间代次，不用超时冒充挂断 |
| 第二次呼叫失败 | 上次 `hconn`、异步输入和状态是否真正释放 | 等 `RTC_CLOSED` 后再允许下一轮 |
| 收到来电后界面卡住 | MQTT/SDK 回调中是否做了 NVS、HTTP 或 UI | 回调只复制并投递到有界队列 |
| 总空闲很多但申请失败 | 最大连续内部块过小 | 同时记录 free、largest、minimum 和任务栈高水位 |
| 设备上行有包但微信无声 | Opus 规格、订阅 stream 和发送返回值 | 用首包日志、失败计数和实际听感三重确认 |
| 设备能收包但扬声器无声 | 示例下行只保存编码包 | 增加 Opus 解码和硬件播放，不能把“收到包”当成“已播放” |

## 12. 验收标准

下面是本工程建议的最低交付门槛。每项分别留存日志或产物；后一级通过不能替代前一级证据。

### 12.1 静态与构建验收

| 编号 | 验收项 | 通过标准 | 必留证据 |
|---|---|---|---|
| A1 | SDK 一致性 | 头文件、静态库、版本锁一致；库哈希检查通过 | SDK SHA256、`TiRtcGetVersion()` |
| A2 | API 顺序 | Max send buffer 在 Init 前；Start/Stop 均等待异步完成事件 | 代码检查记录或启动/停止日志 |
| A3 | 分层 | TiRTC API 只出现在适配层；UI/网络层不直接持有 `hconn` | 静态扫描结果 |
| A4 | 敏感信息 | 源码和日志中没有 Wi-Fi 密码、device_key、token | 扫描记录 |
| B1 | 干净构建 | 删除构建目录后 `idf.py build` 成功，无未解析符号 | 完整构建日志 |
| B2 | 目标与分区 | 构建目标为 ESP32-C3；应用镜像不越过 app 分区 | `idf.py size`、镜像大小、分区余量 |
| B3 | 默认配置复现 | 仅使用随包 `sdkconfig.defaults` 干净构建成功，SDK/ABI 门禁未被绕过 | 独立构建目录的完整日志 |

### 12.2 设备上线验收

| 编号 | 验收项 | 通过标准 | 必留证据 |
|---|---|---|---|
| C1 | 连续启动 | 冷启动 3 次均获得 IP，无 panic、看门狗或重启循环 | 三次启动日志 |
| C2 | 时间与 TLS | SNTP 成功；HTTPS/MQTTS 证书校验通过 | 校时、服务发现和 MQTT 日志 |
| C3 | 首次绑定 | 空 NVS 时能取得六位码、收到 `auth_grant`、保存身份并确认 PUBACK | 从 Report 到正式 MQTT 的完整时间线 |
| C4 | 重启复用 | 重启后直接使用已存身份换取正式 token，不重复要求绑定 | `id`、token 请求和正式 MQTT 日志 |
| C5 | 解绑恢复 | 服务端解绑后进入重新绑定，不清除 Wi-Fi，不继续使用旧身份上线 | 解绑、重新 Report 日志 |
| C6 | 平台就绪 | `status` 显示 `platform=1 rtc=1 profile=1`，联系人来自服务端 | status 和 contacts 输出 |

### 12.3 呼叫与媒体验收

| 编号 | 验收项 | 通过标准 | 必留证据 |
|---|---|---|---|
| D1 | 设备外呼 | `call` 成功、收到匹配房间、立即提交 WHIP；微信正式版响铃 | HTTP、call_incoming、WHIP 时间线 |
| D2 | 外呼接通 | 微信接听后设备收到 `0x2000`，状态进入 `active` | `SIGNAL 0x2000` 和 `FLOW -> active` |
| D3 | 微信呼入 | 设备进入 `ringing`，本地接听后 WHIP 和 `0x2000` 正常 | 来电、answer、WHIP、active 日志 |
| D4 | 本地拒接 | 响铃时拒接，微信结束呼叫，设备回到 `idle` | REJECT 返回和状态日志 |
| D5 | 双向媒体 | 设备上行 `TX FIRST`；设备下行 `RX FIRST`；微信实际可听到预录音频；下行录音可离线解码 | 双向首包、计数和听测记录 |
| D6 | 格式与队列 | `failed=0`、`bad_format=0`；正常网络下无持续 backlog drop | 通话 TOTAL 日志 |
| D7 | 通话计时 | 仅在 `0x2000` 后开始，响铃和 WHIP 建连时间不计入通话时长 | 时间线日志 |

### 12.4 结束路径与重复操作验收

每种路径至少连续执行 5 次：

| 编号 | 场景 | 通过标准 |
|---|---|---|
| E1 | 设备外呼，微信未接，设备取消 | 微信停止响铃；设备最终 `closing->idle` |
| E2 | 设备外呼，微信未接，微信侧取消 | 设备收到对应结束事件，不等待 RTC 心跳超时 |
| E3 | 通话中设备挂断 | 发出 `0x2001`，双方结束，收到 `RTC_CLOSED` |
| E4 | 通话中微信挂断 | 设备收到 `0x2001`，立即停止媒体并回到 `idle` |
| E5 | 微信呼入，设备拒接 | 拒接服务请求成功，双方结束 |
| E6 | 微信呼入，微信取消 | 仅结束匹配 room 的来电，旧/其他 room 不影响当前状态 |
| E7 | 断网发生在响铃、连接、通话中 | 当前代次收敛；恢复网络后可以用新 token 再呼叫 |

全部路径都必须满足：

- 最终出现 `call resources ready` 和 `FLOW ... -> idle`。
- 当前 `hconn`、pending HTTP、WHIP 输入、媒体任务和录制状态均已释放或复位。
- 下一次呼叫不依赖重启、清 NVS、延时补丁或看门狗恢复。
- 旧房间、旧 token 和旧代次的迟到回调不能改变新一轮状态。

### 12.5 稳定性与资源验收

| 编号 | 验收项 | 建议最低门槛 | 通过标准 |
|---|---|---:|---|
| F1 | 交替呼入/外呼 | 50 轮 | 无死机、无状态卡住、无必须重启的失败 |
| F2 | 在线待机 | 8 小时 | MQTT 和 TiRTC 保持或自动恢复，无重复绑定 |
| F3 | 单次通话 | 30 分钟 | 双向包持续、无持续积压、无看门狗 |
| F4 | 断网重连 | 30 轮 | 每轮恢复正式 MQTT、RTC 和 profile，允许再次呼叫 |
| F5 | 内存恢复 | 记录预热后基线 | 每轮结束 30 秒后 free/largest 无单调下降；差异超过 4 KiB 必须归因 |
| F6 | 任务栈 | 覆盖 WHIP、HTTP、媒体、挂断峰值 | 所有任务高水位保留明确安全余量，不出现栈溢出 |

`4 KiB` 是本示例用于发现持续泄漏的工程门槛，不是 SDK 的官方保证。目标产品应结合自身网络栈、编解码器和任务模型制定更严格的基线。

### 12.6 验收结论模板

```text
静态检查：通过/未通过（证据：...）
干净构建：通过/未通过（固件大小：...，分区余量：...）
烧录启动：通过/未通过（板卡、串口、固件 SHA256：...）
首次绑定：通过/未通过（六位码到正式 MQTT：...）
设备外呼：通过/未通过（0x2000、TX/RX 首包：...）
微信呼入：通过/未通过（接听、拒接：...）
挂断矩阵：通过/未通过（E1-E7：...）
稳定性：通过/未通过（轮次、时长、内存漂移：...）
未覆盖项：...
```

只有全部必选项通过，才能写“移植验收通过”。仅构建成功时只能写“构建通过”；仅出现 WHIP connected 时不能写“通话通过”。

## 13. 本示例当前验证范围

本次随包 TiRTC 静态库已完成 ESP-IDF 5.5.5 独立干净构建，并在 ESP32-C3 真机上通过 Wi-Fi、HTTPS、正式 MQTT、TiRTC `SYS_STARTED` 和联系人同步连通验证。

同一工程在前一组 SDK 构建上已完成正式版微信设备外呼、双向 Opus、BOOT 空闲呼叫、通话中挂断和资源回收验证；这些运行结果不能替代本次静态库的完整通话回归。

待继续验证：当前随包 TiRTC 静态库的完整呼入/外呼与双向音频矩阵、小时级长稳、弱网和频繁重连、当前版本首次绑定全流程、微信呼入的 BOOT 接听与拒接、实时麦克风/扬声器/AEC。

上述“已验证”描述的是本示例现有证据，不自动覆盖移植后的目标硬件。目标工程修改 SDK、网络、存储、配置或媒体驱动后，需要重新执行第 12 节对应项目。

## 14. 参考资料

- [TiRTC C SDK 接入](https://docs.tange.ai/products/tirtc/guides/sdk-integration/c.html)
- [TiRTC C API Reference](https://docs.tange.ai/products/tirtc/en/api-reference/c.html)
- [微信 VoIP 通话流程](https://docs.tange.ai/products/wxvoip/guides/voip-call-flows.html)
- [ThingConnect C 参考实现](https://github.com/tangeai/tirtc-server-example/tree/main/thing-connect/device-sim/device-sim-c)
- [ThingConnect API Reference](https://github.com/tangeai/tirtc-server-example/blob/main/thing-connect/api-reference.md)

## 15. 安全说明

- Wi-Fi 密码、设备密钥和一次性 token 不得写入源码或日志。
- 示例默认使用 NVS 保存 Wi-Fi 和设备身份，未启用 Flash/NVS 加密。
- 产品化时应按硬件安全要求配置 Secure Boot、Flash Encryption、NVS Encryption 和密钥轮换。
