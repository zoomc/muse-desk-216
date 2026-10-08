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
