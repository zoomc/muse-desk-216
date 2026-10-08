# relay — Mac mini 通知中继

订阅 Muse 聊天，把助手的成品消息（`message.assistant`）转到 stdout 或本地 webhook，供桌面提醒用。

## 前置

- Node.js >= 22.18。
- 已在本机配对（muse-client 的凭据目录，macOS 默认 `~/Library/Application Support/MuseGadgetPair`；配对需要 macOS + Muse 手机 App，见 muse-client 文档）。

## 安装（二选一）

默认：`muse-client` 是外部依赖，`package.json` 已按 commit pin 好上游：

```sh
cd relay
npm install
```

本地联调上游源码时可用 `npm link` 代替 registry/git 安装：

```sh
cd ~/workspace/research/muse-gadget/muse-client
npm link
cd - # 回到 relay/
npm link muse-client
```

## 运行

```sh
npm start
```

环境变量：

| 变量 | 默认 | 用途 |
|---|---|---|
| `RELAY_WEBHOOK_URL` | 空（打印到 stdout） | 设为本地 webhook URL 则 POST JSON `{message_id, text}`，否则打印 `Muse: <text>` |
| `RELAY_SESSION_ID` | 空（主聊天） | 已存在的 side chat 才需要；新 side chat 必须先发一条消息才能订阅 |
| `RELAY_VM_ID` | 空（默认 VM） | 选定 Muse VM |
| `RELAY_CREDENTIALS_DIR` | muse-client 默认目录 | 配对凭据目录 |

配对凭据刷新后会自动写回（`onCredentials` → `saveCredentials`）；不要多进程共用同一份凭据。

## 诚实边界

- **无历史补拉**：`/chat/subscribe` 是实时流。断线窗口内发出的消息永久丢失，重连不会补回。
- 只取 `message.assistant`（成品消息），`display_text_ready === false` 的占位事件会被跳过；按 `message_id` 去重（上限 1000 个，防 `delta.message_done` 与 `message.assistant` 对同一回复的重复投递）。
- 中继只订阅不发送，不会产生自己的回声。
- muse-client 自身无重连：此处自己实现断线重连 + 指数退避（初始 1s、翻倍、上限 60s、等量 jitter）。
