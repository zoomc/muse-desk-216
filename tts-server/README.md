# tts-server — 局域网中文语音服务

接收设备发来的 UTF-8 文字回复，用 edge-tts 合成中文女声，通过 HTTP 流式返回 MP3。需要 Python 3.11+ 与互联网；不包含离线语音引擎。

相对上游 box3 版本的唯一改动：`/tts` 加了 `Authorization: Bearer <token>` 鉴权（box3 原版无鉴权，局域网内任何人可调用）。

## 生成 token

```sh
python3 -c "import secrets;print(secrets.token_urlsafe(32))"
```

把输出填进环境变量 `MUSE_TTS_TOKEN`（`.env.example` 有模板）。未配置 token 时 `/tts` 直接返回 503（fail closed），不会无鉴权对外服务。

## 本机启动

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
export MUSE_TTS_TOKEN='<刚才生成的 token>'
python -m uvicorn tts_server:app --host 0.0.0.0 --port 8765 --no-access-log
```

环境变量：

| 变量 | 必填 | 默认值 | 用途 |
|---|---|---|---|
| `MUSE_TTS_TOKEN` | 是 | 空（未设则 `/tts` 503） | `/tts` 的 Bearer token |
| `MUSE_TTS_VOICE` | 否 | `zh-CN-XiaoxiaoNeural` | 音色 |
| `MUSE_TTS_PROXY` | 否 | 空，不使用代理 | 可选 HTTP 代理地址 |

## 接口

- `GET /health`：免鉴权（给局域网监控用），只报进程状态，不保证语音上游可达。
- `POST /tts`：必须带 `Authorization: Bearer <token>`，否则 401；`Content-Type: text/plain; charset=utf-8`，最大 4095 字节，返回 `audio/mpeg` 流。格式不匹配、UTF-8 无效/空文本、超限和服务忙分别返回 415、400、413、503。
- 最多两个并发请求，上游每次读取超时 20 秒；取消和异常均释放并发槽位。
- 上游失败中止音频流，固件按阅读速度继续显示字幕（设备侧降级，绝不卡死）。
- 服务不缓存音频、不记录回复原文。

验证：

```sh
curl -fsS http://127.0.0.1:8765/health
curl -f -H 'Content-Type: text/plain; charset=utf-8' \
  -H "Authorization: Bearer $MUSE_TTS_TOKEN" \
  --data '你好，这是中文语音测试。' http://127.0.0.1:8765/tts -o test.mp3
```

## 设备侧配置

固件 `menuconfig` 里选 TTS 后端 edge（默认），填 `CONFIG_MUSE_LOCAL_TTS_URL=http://你的服务器IP:8765/tts`，
再把同一 token 填进 `CONFIG_MUSE_LOCAL_TTS_TOKEN`（与 URL 一样经 `menuconfig` / 生成的 `sdkconfig` 设置，
不要进 git），重新构建。设备每次请求都会带 `Authorization: Bearer` 头；token 为空则服务端回 401，
该条回复降级为字幕。
