# PORTING — P0 移植记录

每个移植点一行：来源仓库 URL、来源 commit、源文件及行号范围、本仓库目标文件。
行号指移植时来源文件的行号（`feature/desk-216` 基线 `86cf33f` 时刻）。

来源仓库：

- wupsbr fork: <https://github.com/wupsbr/waveshare-muse-gadget-sdk.git> @ `2c648812feb606a85043e368e711e0ca82d61ab9`
- box3 中文 TTS: <https://github.com/isamu2025/muse-box3-chinese-tts.git> @ `c9f03e7d9ceec6f405b2f6fd0171f9aa041d9034`

## 1. All messages 主动通知（来源 wupsbr）

| 来源文件 | 行号范围 | 内容 | 本仓库目标文件 |
|---|---|---|---|
| `esp32/components/muse/muse_chat_session.cpp` | ~L1291–1356 | 注释 + `SHOWN_IDS=8` + `s_push_pending`/`s_shown_ids` + `remember_shown`/`was_shown` + `push_begin` + `push_maybe_begin` | `esp32/components/muse/muse_chat_session.cpp`（`turn_reset_streams` 前 + `msg_id` 后） |
| `esp32/components/muse/muse_chat_session.cpp` | ~L959–967 | `turn_finish` 内 `remember_shown` 全部已显示 id | 同上 `turn_finish` |
| `esp32/components/muse/muse_chat_session.cpp` | ~L1503–1506 | `on_event` 开头先取 `event`/`payload`、调 `push_maybe_begin`，再做 `P_WAIT_REPLY` 检查 | 同上 `on_event` |
| `esp32/components/muse/muse_chat_session.cpp` | ~L2234–2245 | 未连接时若 `pushes_on` 且退避到期则重连（All messages 开启后保持在线） | 同上 `hatch_task` 未连接分支 |
| `esp32/components/muse/muse_chat_session.cpp` | ~L2279–2283 | 空闲断开加 `!muse_settings_pushes_on()`（开时不断开，省电默认关） | 同上 `hatch_task` 空闲分支 |
| `esp32/components/muse/muse_chat_session.cpp` | ~L2387–2394 | `muse_hatch_push_take` / `muse_hatch_push_drop` | 同上 Public API + `esp32/components/muse/muse_chat.h` 声明 |
| `esp32/components/muse/muse_voice.c` | ~L351–439 | `hatch_reply` 重构为 `hatch_reply_as(delivered, waiting)` | `esp32/components/muse/muse_voice.c` |
| `esp32/components/muse/muse_voice.c` | ~L443–451 | `play_push()`：灭屏唤醒 + Wi-Fi 全功率 + 当普通回复播出 | 同上 |
| `esp32/components/muse/muse_voice.c` | ~L929–939 | 主循环空闲取 pending（播 push、中断恢复 idle）+ 按住说话 `muse_hatch_push_drop` 抢占 | 同上 `voice_task` 空闲分支 |
| `esp32/components/muse/muse_settings.c` | ~L37, L122–124, L143, L157, L214–219 | `pushes_on` 字段 + NVS 键 `pushes` + `muse_settings_pushes_on()` + setter + 日志 | `esp32/components/muse/muse_settings.c` / `.h`（+ `MUSE_SETTING_PUSHES`） |
| `esp32/components/muse/muse_settings_ui.c` | ~L76, L270–292, L1209–1212, L1220, L1250–1253 | `s_home_pushes` + `switch_row_icon` + 顶层 `LV_SYMBOL_BELL` "All messages" 开关（默认关）+ `tick_home` 同步 | `esp32/components/muse/muse_settings_ui.c` |

触发条件（照抄来源）：开关开 + 无等待 turn 的 assistant 消息（`delta.message_start`/`message.assistant` 且 `phase==P_IDLE`）+ 设备空闲（`MUSE_MODE_IDLE` 且无 pending）+ 未播过（8 个 id 去重）+ 按住说话抢占（`muse_hatch_push_drop`）。

s3-216 显示/音频驱动未动（`boards/board_waveshare_s3_216.c`、`muse_lcd_bands.c`、`muse_audio.c` 均未改）。

## 2. TTS 双后端（Kconfig `MUSE_TTS_BACKEND` choice：edge 默认 / elevenlabs / off）

| 来源文件 | 行号范围 | 内容 | 本仓库目标文件 |
|---|---|---|---|
| box3 `esp32/components/muse/muse_tts.cpp` | L1–168 全文 | worker 经 `esp_http_client` 向 `CONFIG_MUSE_LOCAL_TTS_URL` POST UTF-8 `text/plain`、`Accept: audio/mpeg`、流式 2048B 读；非 200/非 audio-mpeg/20s 无数据/截断判失败；`s_busy` 单 worker；`cancelled` 原子取消；PSRAM 分配见下 | `esp32/components/muse/muse_tts.cpp`（加 PSRAM 原因注释：Wi-Fi 连接瞬间内部 RAM 只剩约 6KB） |
| box3 `esp32/components/muse/muse_tts.h` | L1–25 全文 | `muse_tts_begin/read/finished/close` + `MUSE_TTS_TEXT_BYTES=4096` | `esp32/components/muse/muse_tts.h` |
| box3 `esp32/components/muse/muse_chat_session.cpp` | ~L1538–1560, L1588–1615, L2039–2044 | `start_tts` 内 local TTS 尝试 + `poll_local_tts`（失败→`"speech unavailable; continuing captions"` 静默按阅读速度继续，绝不卡死）+ `hatch_task` 调用 | `esp32/components/muse/muse_chat_session.cpp`（`MUSE_TTS_EDGE` 分支） |
| wupsbr `esp32/components/muse/muse_chat_session.cpp` | ~L1580–1700 | `tts_fetch()`：POST `https://api.elevenlabs.io/v1/text-to-speech/{voice_id}/stream?output_format=mp3_22050_32` + `xi-api-key` 头；`{text, model_id}`；首包计时；非 200 打日志；截断判失败 | 同上（`MUSE_TTS_ELEVEN` 分支，原样移植） |
| wupsbr `esp32/components/muse/muse_chat_session.cpp` | ~L1701–1728, L1818–1842 | `tts_start`/`tts_task`（8KB 栈 core 1）+ `tts_pump`（失败回退阅读速度字幕） | 同上 |
| wupsbr `esp32/components/muse/Kconfig` | ~L190–216 | `MUSE_ELEVENLABS_API_KEY`（空默认=关闭）、`VOICE_ID`（Sarah）、`MODEL` | `esp32/components/muse/Kconfig`（空默认值，无真实 key） |
| box3 `esp32/components/muse/Kconfig` | ~L135–151 | `MUSE_LOCAL_TTS` + `MUSE_LOCAL_TTS_URL` | 同上（URL 空默认；backend choice 新增，见文件） |
| box3 `esp32/components/muse/CMakeLists.txt` | ~L49–51 | `CONFIG_MUSE_LOCAL_TTS` 时编译 `muse_tts.cpp` | `esp32/components/muse/CMakeLists.txt`（条件改为 `CONFIG_MUSE_TTS_BACKEND_EDGE`） |

内存红线：ElevenLabs `TTS_RX_BYTES` 64KB 经 `xStreamBufferCreateWithCaps(..., MALLOC_CAP_SPIRAM)` 放 PSRAM；edge `QUEUE_BYTES` 经 `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` 放 PSRAM。Wi-Fi 连接瞬间内部 RAM 只剩约 6KB，内部 RAM 放不下。

## 3. 中文 LVGL 字库（来源 box3）

| 来源文件 | 行号范围 | 内容 | 本仓库目标文件 |
|---|---|---|---|
| box3 `esp32/components/muse/fonts/muse_font_cjk_16.c` | 全文（3.3MB，6990 字形，16px 2bpp Noto Sans CJK SC） | 覆盖本仓库同名 Unifont 位图文件 | `esp32/components/muse/fonts/muse_font_cjk_16.c` |
| box3 `esp32/components/muse/fonts/OFL.txt` | 全文 | SIL OFL 1.1 说明（Noto 衍生要求） | `esp32/components/muse/fonts/OFL.txt`（新增） |
| box3 `esp32/components/muse/muse_ui.c` | ~L47–48, L890–902 | `LV_FONT_DECLARE(muse_font_cjk_16)` + 字幕字体链接 | 本仓库已具备同等接线（`esp32/components/muse/muse_ui.c` `caption_font()` fallback + `LV_FONT_DECLARE`，`CONFIG_MUSE_CJK_FONT` 门控）；`CONFIG_MUSE_UI_CHINESE`（默认开）`select MUSE_CJK_FONT` 复用该链路 |
| box3 `esp32/components/muse/Kconfig` | ~L103–129 | `MUSE_CJK_CAPTIONS` + `MUSE_UI_CHINESE` | `esp32/components/muse/Kconfig`（`MUSE_UI_CHINESE` 默认 y；字库编译仍走既有 `CONFIG_MUSE_CJK_FONT`→`fonts/muse_font_cjk_16.c`，见 `CMakeLists.txt` L52） |

字幕分页按 UTF-8 码点：`esp32/components/muse/muse_chat_text.c` `next_line`（`muse_text_ascii`/`muse_text_cjk` 按码点计宽，`end += bytes` 按字节推进，从不在多字节字符中间断开；`muse_hatch_tail_words`/`escape_some` 同理跳过 continuation bytes）。此为基线已有能力，本次未改，仅复用。

已知边界：本字库为 GB2312 级（生僻字/繁体/注音多数缺字，emoji 按既有过滤丢弃）；box3 的 `muse_locale*.c` 整套中文菜单文案未移植（BOX-3 布局专用），s3-216 菜单保持英文、字幕中文正常。

## 4. P1：触摸音量 + 电池剩余时间 + UI 状态机

来源仓库（行号指移植时来源文件的行号）：

- wupsbr fork: <https://github.com/wupsbr/waveshare-muse-gadget-sdk.git> @ `2c648812feb606a85043e368e711e0ca82d61ab9`
- charm-mosaico: <https://github.com/samyeei/Muse-charm-mosaico.git> @ `8d94c427390e9aad56f21b027757241a5c141158`（`projects/muse_companion/main/` 下）

### 4.1 触摸音量（来源 wupsbr，只取音量部分）

| 来源文件 | 行号范围 | 内容 | 本仓库目标文件 |
|---|---|---|---|
| `esp32/components/muse/muse_ui.c` | L76–79 | `COLOR_VOL`/`COLOR_VOL_TRACK`、`VOL_STEP`（5%/步）、`VOL_SHOW_S`、`VOL_FADE_MS` | `esp32/components/muse/muse_ui.c`（`COLOR_LIT` 后） |
| 同上 | L155–162 | `DRAG_*` 四态 + `s_vol`/`s_vol_step_px`/`s_drag`/`s_drag_at`/`s_drag_vol`/`s_vol_hide_at` | 同上（`s_shown_speaker` 后；原样，未取 `s_rub`/`s_tap`） |
| 同上 | L1002–1030 | `build_volume()`：圆屏 bezel 内青色圆环（复用 `on_ring_draw` slab 优化）、方屏右侧竖条；初始隐藏、不可点击 | 同上（`on_ring_draw` 后） |
| 同上 | L1033–1079 | `set_vol_opa` + `vol_faded` + `fade_volume`（2s 后 300ms 淡出）+ `show_volume`（扬声器关时变暗，只记电平） | 同上 |
| 同上 | L1082–1147 | `drag_can_start()` + `volume_drag()`：头像 tile 纵向拖拽（`\|dy\|>2·\|dx\|`、步长屏高 5%）每步 ±5，上滑大、下滑小；拖动中 `muse_audio_set_volume` 实时试听，松开 `muse_settings_set_volume` 写 flash + chirp；横滑仍进设置页 | 同上（`frame_tick` 首行调用；`muse_ui_start` 内 `if (s_face) build_volume(s_face)`） |
| 同上 | L819 | `on_ring_draw` 取 `lv_event_get_target_obj(e)` 而非 `s_ring`（音量圆环复用同一绘制优化） | 同上 `on_ring_draw` 开头两行 |

刻意改动（相对来源）：`DRAG_WATCH` 判定去掉了 `s_rub.turns >= 2`（揉脸检测未移植，见下）；`muse_ui_start` 只接 `build_volume`，不包 `touch_read`/`on_touch`、不设 `s_rub_step`。

未移植：wupsbr 的揉脸/连击 tickle（`muse_ui.c` L1160–1280 `touch_read`/`on_touch`/`tickle_poll`/`within`/`tickle`/`rub_restart` + `muse_state` 的 tickle/dizzy/sleepy/waking API）。本仓库 `muse_state.h` 无这套 API，且属趣味功能，超出 P1 范围；接入需先给 `muse_state` 加 tickle 状态，留待后续。

s3-216 显示/音频驱动未动：只经 LVGL `lv_indev_*` 读触摸、经 `muse_audio_set_volume`/`muse_settings_set_volume` 改音量；`boards/board_waveshare_s3_216.c`、`muse_lcd_bands.c`、`muse_audio.c` 均未改。

### 4.2 电池剩余时间（来源 wupsbr；百分比/充电状态为既有能力）

先读结论：本仓库电池链路已完整——s3-216 板文件 `boards/board_waveshare_s3_216.c` L272 `.read_power = muse_pmu_read_power`（AXP2101 读 `battery_pct`/`battery_mv`/`charging`/`usb`，见 `muse_pmu.c` L151），`muse_input.c` L446 轮询后经 `muse_state_set_power` 分发，主屏 `update_power` 与设置页 `tick_battery`/`tick_home` 已显示百分比 + 充电状态。wupsbr 的电池页反而是简化版（只有 Battery + Time left 两行），照搬会丢掉本仓库上游风格的测量页（Used / A full charge / Screen off / Chip asleep / Wakes / CPU busy）。因此只补 wupsbr 独有的电压拟合剩余时间，保留现有详细页。

| 来源文件 | 行号范围 | 内容 | 本仓库目标文件 |
|---|---|---|---|
| `esp32/components/muse/muse_battery.c` | L373–449 | 注释 + `ETA_EVERY_S=10`/`ETA_SAMPLES=30`/`ETA_MIN_SAMPLES=6`/`ETA_MAX_MIN` + `s_eta` + `curve_tenths`（LiPo 电压曲线）+ `eta_reset`/`eta_note` + `muse_battery_eta`（最小二乘斜率；环形缓冲按存储序求和，斜率与顺序无关） | `esp32/components/muse/muse_battery.c`（`muse_battery_drain` 后；`note_power` 前加 `eta_reset`/`eta_note` 前向声明） |
| 同上 | L457, L462 | `note_power`：开始测量时 `eta_reset()`，测量中 `eta_note(p)` | 同上 `muse_battery_note_power`（`muse_battery_reset` 未加 `eta_reset`，与来源一致：下次开始测量时重置） |
| `esp32/components/muse/muse_battery.h` | L67–69 | `muse_battery_eta(int pct_now, int *mins)` 声明 + 文档 | `esp32/components/muse/muse_battery.h`（`muse_battery_drain` 后） |
| `esp32/components/muse/muse_settings_ui.c` | L107, L1127–1171 | `s_batt_left` "Time left" 行：充电→Charging、USB→On USB、有拟合→`~H h MM min`/`~M min`、否则 Estimating | `esp32/components/muse/muse_settings_ui.c`（既有详细页上新增 `s_batt_left` 一行，不删原有行） |

已知边界：`curve_tenths` 系 StickS3/Watcher 的 LiPo 曲线，s3-216 的 AXP2101 同为单节 LiPo 计，拟合可用但斜率未经真机标定；`battery_mv<=2500` 时回退到 `pct*10`（有电池但读不到电压的板）。

### 4.3 LVGL 原生六态机（借鉴 charm `companion_model`，未搬 ESP-GSP）

charm 的 UI 是 ESP-GSP/Mosaico 场景播放器（`companion_ui.c` L29–42 六姿态互斥显示、L266–299 `character_motion()` 程序化位移：待机呼吸/聆听轻摆/思考起伏+圆点/说话点头/困倦慢呼吸/错误摇晃；L197–211 每态字幕+hint），与本仓库 LVGL 完全不兼容，按 DESIGN.md §4 只能借鉴思路。charm 状态机本体（`companion_model.h` L6–7 `COMP_IDLE/LISTENING/THINKING/SPEAKING/SLEEPY/ERROR` + L14–28 快照；`companion_model.c` L34–46 PRESS→LISTENING/RELEASE→THINKING、L84–98 `tick` 空闲变暗→SLEEPY、L104–116 reply/done/error 切换）自己拥有输入，而本仓库输入分散在 voice/input 各任务经 `muse_state_set_mode()`/`muse_state_set_asleep()` 写入共享状态——因此本实现为解析式（resolve）而非驱动式：每帧从 `muse_state` 解析出呈现态，切换仍由原有事件驱动。

| 来源 | 行号范围 | 借鉴点 | 本仓库目标文件 |
|---|---|---|---|
| charm `companion_model.h` | L6–7, L34–46 | 六态枚举 + PRESS/RELEASE/CANCEL/MUTE/VOLUME…动作表（动作→效果位思想） | `esp32/components/muse/muse_ui.h`（`muse_ui_state_t` 公开枚举 + `muse_ui_state()`） |
| charm `companion_model.c` | L34–60, L84–98, L104–116 | PRESS→listening / RELEASE→thinking / reply→speaking / done→idle / error→error / 熄屏超时→sleepy 的切换表 | `esp32/components/muse/muse_ui.c`（`resolve_ui_state()` + 注释中的切换表；切换执行仍在 `muse_voice.c`/`muse_input.c` 原有 `set_mode`/`set_asleep` 处） |
| charm `companion_ui.c` | L197–211, L266–299 | 每态字幕/hint + 每态位移动画 | 每态屏幕表现（LVGL 原生，既有控件）：idle=READY/米色mic灭+表隐藏；listening=点亮mic+实时电平表+ring进度；thinking=spinner ring+回复布局让位；speaking=ring进度+回复页+TTS；sleepy=暗屏+触摸罩+亮度0（`update_sleep`）；error=ERROR+accent色+管线错误字幕。字幕仍归 voice 管线所有，状态机不写字幕，避免打架 |

`s_last_mode` 已由 `s_ui_state` 取代：`frame_tick` 解析→`ui_state_enter()`（切换日志 + listening 入场：清推送图、滑回头像页）→`ui_draw_mode()` 映射回 `muse_mode_t` 供头像/ring/表/字幕绘制（BOOT→idle 画 "WAKING UP"，OFF→sleepy；asleep 时 `update_sleep()` 本就跳过绘制）。

## 5. relay / tts-server / secrets（第三轮）

来源仓库（行号指本轮移植时来源文件的行号）：

- muse-client: <https://github.com/wong2/muse-client.git> @ `89a3feaeac8d18f912c33f621e5f59c300b87d36`
- box3 中文 TTS: <https://github.com/isamu2025/muse-box3-chinese-tts.git> @ `c9f03e7d9ceec6f405b2f6fd0171f9aa041d9034`
- wupsbr fork: <https://github.com/wupsbr/waveshare-muse-gadget-sdk.git> @ `2c648812feb606a85043e368e711e0ca82d61ab9`

### 5.1 relay（来源 muse-client，未拷源码，外部依赖）

| 来源文件 | 内容 | 本仓库用法 |
|---|---|---|
| `src/client.ts` ~L73–93 `MuseClient.subscribe()` | 打开 `/chat/subscribe` 长订阅流 | `relay/src/relay.ts` `runOnce()` 直接调用；只取 `event === 'message.assistant'` 的成品消息 |
| `src/events.ts` ~L13–43 `decodeChatEvents()` | NDJSON 解码 + 同流内 `seq` 去重 | 复用其语义：跨重连按 `message_id` 去重（1000 上限，`Set` 环形丢弃最老）；`display_text_ready === false` 占位跳过（判据抄 `examples/cli.ts` L41–42 `printEvent`） |
| `src/credentials.ts` `loadCredentials`/`saveCredentials` | 配对凭据读写 + token 轮换持久化 | `runOnce()` 连接时调用，`onCredentials` 写回；`RELAY_CREDENTIALS_DIR` 覆盖默认目录 |
| `src/connection.ts` `NoiseConnection` | 底层 multiplexed HTTP-over-Noise | 不直接用，经 `MuseClient` 间接使用 |

`relay/` 自实现：断线重连 + 指数退避（初始 1s、翻倍、上限 60s、等量 jitter，`backoffDelayMs`；muse-client 不自带重连）。
收到后只做一件事：`RELAY_WEBHOOK_URL` 置则 POST JSON `{message_id, text}`，否则打印 `Muse: <text>`。
`package.json`  pin `muse-client` git commit（`npm install` 即装），`npm link` 备选写于 `relay/README.md`。
诚实边界：无历史补拉，断线窗口消息永久丢失；中继只订阅不发送，无自身回声。

### 5.2 tts-server（来源 box3，全文照搬 + 鉴权）

| 来源文件 | 行号范围 | 改动点 | 本仓库目标文件 |
|---|---|---|---|
| box3 `tts-server/tts_server.py` | 全文（`/health`、`POST /tts` text/plain、上游失败抛异常→设备降级字幕、`slots=Semaphore(2)`、20s 读取超时） | 原样保留；**新增**：`MUSE_TTS_TOKEN` env + `check_bearer()`（`hmac.compare_digest` 常量时间比较；`/health` 免鉴权，`/tts` 缺/错 token 回 401，服务端未配 token 回 503 fail-closed） | `tts-server/tts_server.py` |
| box3 `tts-server/requirements.txt` | 全文（`edge-tts==7.2.8`、`fastapi==0.136.3`、`uvicorn==0.48.0`） | 无改动 | `tts-server/requirements.txt` |
| box3 `tts-server/muse-box3-tts.env.example` | `MUSE_TTS_VOICE` + `MUSE_TTS_PROXY` | 新增 `MUSE_TTS_TOKEN=REPLACE_WITH_GENERATED_TOKEN` 占位 | `tts-server/.env.example` |

设备侧配套（否则鉴权后的服务端对固件永远 401，edge 后端静默死亡）：`esp32/components/muse/muse_tts.cpp`
`worker()` 在 URL/Accept 头之外追加 `Authorization: Bearer <CONFIG_MUSE_LOCAL_TTS_TOKEN>`（token 为空则不发，
服务端 401 → 该条降级字幕）；`esp32/components/muse/Kconfig` 新增 `MUSE_LOCAL_TTS_TOKEN`
（空默认，`MUSE_HATCH && MUSE_TTS_BACKEND_EDGE` 门控；`muse_tts.cpp` 仅 EDGE 后端编译，宏恒存在）。
显示/音频驱动未动（`boards/board_waveshare_s3_216.c`、`muse_lcd_bands.c`、`muse_audio.c` 均未改）。

### 5.3 secrets（来源 wupsbr，原样照搬）

| 来源文件 | 内容 | 本仓库目标文件 |
|---|---|---|
| wupsbr `secrets/README.md` | 全文（三段式：gitignored 说明 + 文件表 + build 注入说明） | `secrets/README.md`（改动：`board.sh build <board>` 示例改为 `build s3-216` 等本仓库板名） |
| wupsbr `secrets/muse_sdk_token.example` | `mgst_` 占位符 | `secrets/muse_sdk_token.example`（原文） |
| wupsbr `secrets/elevenlabs_api_key.example` | `sk_` 占位符 | `secrets/elevenlabs_api_key.example`（原文） |
| wupsbr `secrets/elevenlabs_voice_id.example` | Sarah 默认 voice id | `secrets/elevenlabs_voice_id.example`（原文） |
| wupsbr `esp32/tools/muse/secrets.py` | 全文（`ROOT=parents[3]` 指向仓库根；三元组 `CONFIG_GADGET_SDK_TOKEN`/`CONFIG_MUSE_ELEVENLABS_API_KEY`/`CONFIG_MUSE_ELEVENLABS_VOICE_ID`） | `esp32/tools/muse/secrets.py`（逐字节相同；相对路径一致故 `parents[3]` 无需改层数，已验证指向仓库根） |
| wupsbr `esp32/tools/muse/board.sh` L77–78 | build 时 `python3 "$root/tools/muse/secrets.py" "$B/sdkconfig"` | `esp32/tools/muse/board.sh`（同位置插入，`$root`=esp32/ 语义与来源一致） |

Kconfig 名字核对：`CONFIG_GADGET_SDK_TOKEN`（`esp32/main/Kconfig.projbuild` L26）、
`CONFIG_MUSE_ELEVENLABS_API_KEY` / `CONFIG_MUSE_ELEVENLABS_VOICE_ID`
（`esp32/components/muse/Kconfig` L236/L247）与 `secrets.py` 三元组一致。
`.gitignore` 加 `secrets/*` + `!secrets/README.md` + `!secrets/*.example` 取反保留模板。
