# Linux Device TiRTC Nano Uplink Demo

This demo is a Linux x86_64 device-side reference for one narrow flow only:

`start -> wait for client -> stream fixed assets -> keep waiting after disconnect`

It is intentionally not a product shell. The only required inputs are:

- `endpoint`
- `device_id`
- `device_secret_key`

The only media inputs are kept under `assets/` and are expected to be shipped in the release zip, not from source control.



- `assets/audio.g711a`
- `assets/video.h264`

The demo keeps stream IDs fixed so the existing Flutter example can play it directly:

- audio stream ID: `10`
- video stream ID: `11`

## What It Supports

- Linux x86_64 only
- C source only
- one active client connection at a time
- fixed looping G711A + H264 uplink
- keep running after disconnect and wait for the next client

## What It Does Not Support

- macOS / Windows / Android
- dynamic assets or stream IDs
- command channel logic
- stream messages
- multi-client fan-out
- real camera or microphone capture

## Build

```sh
./script/build.sh
```

`build.sh` fails fast when these files are missing:

- `3rd/include/tiRTC.h`
- `3rd/include/basedef.h`
- `3rd/lib/libtirtc.a`

The bundled `libtirtc.a` is expected to be built from `.refers/tirtc-nano/TiRTC` before delivery, then copied into `3rd/lib/` for the final demo bundle.

The binary is written to:

```sh
build/linux_device_uplink_demo
```

## Run

```sh
./script/run_demo.sh \
  --endpoint https://your-endpoint.example.com \
  --device-id your_device_id \
  --device-secret-key your_device_secret_key
```

`run_demo.sh` fails fast when any required argument is missing, or when either of these files is missing:

- `assets/audio.g711a`
- `assets/video.h264`

## Expected Runtime Flow

After startup, the demo should log the same high-level lifecycle every time:

1. validate startup inputs
2. start TiRTC device service
3. wait for client connections
4. accept one client connection
5. send an IDR frame immediately after the client connection is accepted, then continue normal audio/video pacing
6. rewind assets and continue with continuous timestamps
7. stop that connection after disconnect
8. return to waiting for the next client

Hot-path packet logs are intentionally omitted. You should only see lifecycle logs, loop rewind logs, and low-frequency streaming heartbeat logs.

## Flutter Playback Smoke

Use the existing Flutter example in Matrix:

`products/sdk/flutter/tirtc_av_kit/example`

The example already defaults to:

- `audio_stream_id = 10`
- `video_stream_id = 11`

On the Flutter configure page, fill:

- `endpoint`: the same endpoint passed to this demo
- `remote_id`: the `device_id` passed to this demo
- `token`: a valid connect token for that device
- keep the default stream IDs `10` and `11`

Then connect. Expected behavior:

- video sends an immediate IDR frame after connect so the client can render quickly, then audio/video continue in normal pacing
- playback continues while the fixed assets loop
- if the Flutter side disconnects, the Linux demo stays alive and waits for the next client
