# SECURITY — 安全须知

## 密钥进固件：SDK token / TTS key 绝不进 git

- `muse_sdk_token`（`mgst_…`）、`elevenlabs_api_key`、`elevenlabs_voice_id`
  经 `secrets/` + `esp32/tools/muse/secrets.py` 在构建时注入生成的
  `sdkconfig`，Kconfig 只留空默认值。密钥只存在于：你本机的 `secrets/`
  明文文件、构建目录 `esp32/build-*/sdkconfig`、刷进 Flash 的固件镜像。
- 因此：**绝不分享 `esp32/build-*/` 目录与刷机镜像**（`.bin` / `flash_args`
  整包），它们携带完整密钥。
- 泄漏后：去 [gadgets.muse.ai](https://gadgets.muse.ai/settings/sdk-tokens)
  吊销 SDK token（ElevenLabs key 去 elevenlabs.io 吊销），换新 key 后重新
  `board.sh build` + 刷机。旧固件/旧镜像仍含旧 key，一并销毁。
- 提交前自查：全文扫描 `mgst_` / `sk-` / `sk_` 真值；`git log --all -- secrets/`
  应只有 README 与 `*.example` 模板。

## 配对：只在可信网络做 community pairing

- 设备用 community pairing v5：需要手机 App 侧物理按键确认，但无厂商
  attestation，且防不住活跃中间人。只在可信局域网配对，配完确认设备名
  `MuseGadget-XXXXXX` 与屏幕显示一致。

## tts-server token 的能力边界

- `/tts` 的 `Authorization: Bearer` token 防的是**误触发**（局域网里别的
  设备/脚本随手调），不是防窃听：局域网走明文 HTTP，能抓包的人能看到
  token 和全部回复文本。
- 敏感环境请把 tts-server 放进 WireGuard / 尾网（Tailscale），或只绑
  `127.0.0.1` 跑本机。
- token 泄漏：重生成（`python3 -c "import secrets;print(secrets.token_urlsafe(32))"`），
  服务端与 `CONFIG_MUSE_LOCAL_TTS_TOKEN` 两边一起换，旧 token 即时失效。

## relay

- relay 跑在 Mac mini 上，持有配对凭据（可读写你的 Muse 聊天）。凭据目录
  权限 0700，不要多进程共用；`unpair` 只删本地文件，不解绑服务端，
  解绑去手机 App Settings > Devices。
- `RELAY_WEBHOOK_URL` 只填本机/可信内网地址：转发的每条助手消息原文都会
  POST 过去。
