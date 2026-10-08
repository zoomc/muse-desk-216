<!--
Copyright (c) Meta Platforms, Inc. and affiliates.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# muse-desk-216（中文说明）

2.16 寸 Waveshare 桌面终端：基于 ESP32-S3（480×480 AMOLED + 触摸 + 音频），
fork 官方 `muse-gadget-sdk`，以 `s3-216` profile 为基座移植。屏幕/音频驱动未动，只做加法。

## 功能清单（P0/P1，均已实现）

P0：

- 双向语音对话（基座保留：按住说话 → Muse 回复字幕）。
- All messages 主动通知（设置页顶层开关，默认**关**，省电；开关开 + 设备空闲时播报助手新消息）。
- TTS 语音播报（双后端 Kconfig 选择：局域网 edge-tts 默认零 key 成本 / ElevenLabs 备选 / 关闭；失败降级为按阅读速度显示字幕，绝不卡死）。
- 中文界面（GB2312 级 CJK 字库，字幕中文正常；设置菜单保持英文）。

P1：头像 tile 纵向拖拽调音量、电池剩余时间拟合、六态 UI 状态机（idle/listening/thinking/speaking/sleepy/error）、
`relay/`（Mac mini 通知中继）、`tts-server/`（带鉴权的局域网中文语音服务）、`secrets/` 密钥机制。

移植细节见 `docs/PORTING.md`，上游基线见 `UPSTREAM.md`。

## 构建 / 配对 / 刷机

```sh
cp secrets/muse_sdk_token.example secrets/muse_sdk_token   # 填入 mgst_…（绝不进 git）
esp32/tools/muse/board.sh build s3-216     # 构建（自动经 secrets.py 注入密钥）
esp32/tools/muse/board.sh flash s3-216     # 刷机
```

配对：在 Muse 手机 App（先开 Developer mode）Settings > Devices 里找 `MuseGadget-XXXXXX`，
只在可信网络做 community pairing。ESP-IDF **v6.0.1 锁死**。安全事项见下 §Security 与 `docs/SECURITY.md`。

# Muse Gadgets

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset=".github/images/muse-gadgets-dark.png">
    <img src=".github/images/muse-gadgets-light.png" width="900" alt="Muse gadgets: a Waveshare round AMOLED, an M5Stack StickS3, Muse Home Link, a Raspberry Pi and a Seeed reTerminal e-ink display">
  </picture>
</p>

Muse gadgets are open source devices you build yourself. Program an
off-the-shelf ESP32 board or set up a Raspberry Pi with our device SDKs, then
connect Muse to your displays, buttons, sensors, actuators, and whatever else
you've got lying on your workbench.

We open sourced the SDKs and firmware here. It's built by hackers, for hackers,
just for fun. Side effects of tinkering may include bricked boards, voided
warranties, brownouts, or bankruptcies. Proceed at your own risk!

| | |
|---|---|
| [**ESP32 Device SDK**](esp32) | Connect your ESP32 board to Muse through our open source SDK. Throw in a screen to show images, add audio in and out, or wire up other sensors. |
| [**Linux Device SDK**](linux) | Turn that spare Raspberry Pi or Linux box into a Muse gadget. Hack in your own commands to let Muse handle sysadmin chores or your Home Assistant setup. |

Before you flash or pair a gadget, get an
[SDK token](https://gadgets.muse.ai/settings/sdk-tokens) and review the
[Gadget SDK Terms](https://gadgets.muse.ai/sdk-terms). Every gadget needs a
token to pair.

ESP32 and Linux gadgets pair with the Muse app on iOS and Android, via
Settings > Devices. Turn on Developer mode there first, then look for devices
prefixed with "MuseGadget".
Each directory has a `README.md` to get started and an `AGENTS.md` for coding
agents like [Muse Code](https://developer.meta.com/ai/lp/muse-code/).

## Community

Meet other hackers who are building and customizing Muse gadgets in our
community [Discord](https://discord.gg/3bhjCkZdd6). Get inspired, support each
other, and share what you make.

## Security（安全警告）

- SDK token（`mgst_…`）与 TTS key 编译进固件：**不分享 `esp32/build-*/` 与刷机镜像**；
  泄漏后去 gadgets.muse.ai 吊销并重编重刷（详见 `docs/SECURITY.md`、`secrets/README.md`）。
- community pairing 只在可信网络做（无厂商 attestation，防不住活跃中间人）。
- `tts-server` 的 Bearer token 防误触发不防窃听（局域网明文 HTTP），敏感环境请走 WireGuard/尾网。
- Jollybot 头像素材不在 Apache-2.0 内：公开发布前请替换头像素材（或先用纯色/LVGL 自绘占位）。

## License

Muse Gadgets is licensed under the Apache License, Version 2.0, found in
[`LICENSE`](LICENSE), except for these third-party files, which keep their
upstream licenses:

| Path | Upstream | License |
|---|---|---|
| [`esp32/components/minimp3/include/minimp3.h`](esp32/components/minimp3) | [lieff/minimp3](https://github.com/lieff/minimp3) | CC0-1.0, see [`LICENSE`](esp32/components/minimp3/LICENSE) |
| [`esp32/main/pixel_font.c`](esp32/main/pixel_font.c) | Adafruit GFX `glcdfont.c` | BSD-2-Clause, in the file header |

Dependencies fetched at build time are under their own licenses: ESP-IDF
components (into `esp32/managed_components/`), and the simulator's LVGL and
SDL (listed in [`esp32/simulator/THIRD_PARTY.md`](esp32/simulator/THIRD_PARTY.md)).

The Apache License does not cover the [Jollybot avatar](esp32/avatar).
