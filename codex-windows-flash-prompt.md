# Codex 刷机 Prompt（Windows PC / ESP32-S3 2.16" 板）

> 用法：板子到货、插上 Windows PC 后，把下面整段贴给 Codex。用户需配合做的三件事标了。

---

你现在要帮我把一块 Waveshare ESP32-S3 2.16 英寸触摸板刷成 Muse 桌面终端。仓库：https://github.com/zoomc/muse-desk-216 （公开，直接 git clone）。

## 0. 环境（版本锁死，别自作主张升级）

- 安装 ESP-IDF **v6.0.1**，必须是这个版本（board.sh 里写死的查找路径就是 esp-idf-v6.0.1）。
- 用乐鑫官方 Windows 一键安装器装，装完验证 `idf.py --version` 显示 v6.0.1。
- Python 用安装器自带的那个，不要混系统 Python。

## 1. 密钥（先去申请 token）

1. 去 https://gadgets.muse.ai/settings/sdk-tokens 申请一个 SDK token，把 token 字符串给你。
2. 在仓库根目录执行（Git Bash 或照着手动拷）：
```
cp secrets/muse_sdk_token.example secrets/muse_sdk_token
cp secrets/elevenlabs_api_key.example secrets/elevenlabs_api_key
cp secrets/elevenlabs_voice_id.example secrets/elevenlabs_voice_id
```
3. 把 token 填进 `secrets/muse_sdk_token`。ElevenLabs 那两个不填也行（默认走免费的局域网 edge-tts）。
4. **红线**：`secrets/` 下真实文件永远不要 `git add` / `git commit` / `git push`。仓库里只能有 `.example` 模板——现在远端 main 上就是干净的，保持住。

## 2. 编译

优先跑仓库自带脚本（Git Bash 里）：
```
esp32/tools/muse/board.sh build s3-216
```
- 脚本会自动找 ESP-IDF v6.0.1（`$HOME/.espressif/esp-idf-v6.0.1` 或 `IDF_EXPORT` 环境变量）；找不到就先 source 官方安装器的 `export.ps1`/`export.sh` 再跑。
- 没有 Git Bash 的话，在 ESP-IDF PowerShell 里进 `esp32/` 目录手动跑：`idf.py set-target esp32s3` 然后 `idf.py build`。注意 sdkconfig defaults 链（board.sh 里定的）：`sdkconfig.defaults;devices/sdkconfig.muse;devices/sdkconfig.muse-waveshare-s3-216`，别漏。
- 编译必须零 error。有 warning 先记下来继续，error 停下来贴给我看。

## 3. 刷机

刷机命令（Git Bash）：
```
esp32/tools/muse/board.sh flash s3-216
```
- 板子通过 USB 刷机：插上后 Windows 设备管理器应出现一个新的 COM 口（ESP32-S3 原生 USB 显示为 "USB JTAG/serial debug unit"）。如果没出现，换根数据线/换个 USB 口再试——很多"刷不进"其实是拿了根充电线。
- 脚本会按 USB 设备自动找端口（`esp32/tools/muse/ports.py`）；如果电脑上插了多块同类板子，第三个参数传 USB 序列号或端口名。
- 你先在设备管理器里确认板子是哪个 COM 口，告诉 Codex。
- 刷不进去（`A fatal error occurred` / 一直 `Connecting...`）：**按住 BOOT 键不放，按一下 RST（复位），再松开 BOOT**，进下载模式后重刷。这是 ESP32 的标准救砖流程，板子上有 BOOT 和 RESET 两个按键。
- ESP-IDF PowerShell 下手动刷：`idf.py -p COMx flash`（COMx 换成实际端口）。

刷完跑 `idf.py -p COMx monitor` 看启动日志：屏幕应亮起进待机界面，日志里不应有反复重启（boot loop）或 PSRAM 初始化失败。

## 4. 配对（手机上做，Codex 碰不到）

刷完后用户自己在手机上操作：Muse App → 设置 → Devices → 打开开发者模式 → 找到 `MuseGadget-xxxx` 配对。配上后在 App 里发句话，板子应唤醒屏幕+显示+语音播报（All messages 在设置里默认关闭，想让它主动播报记得打开）。

## 5. 可选：TTS 服务端（不跑也行）

仓库 `tts-server/` 是局域网免费 edge-tts 中文语音服务。Windows 上 Python 跑起来即可，README 里有步骤。不跑的话固件播失败会自动降级成纯字幕显示，不影响使用。

## 不要做的事

- 不要把已删除的 `relay/` 加回来（v1 不做中继，用户亲定的）。
- 不要升级 ESP-IDF 版本，不要改 sdkconfig 里 PSRAM/Flash 的配置除非编译不过。
- 不要提交任何含真实密钥的文件。
