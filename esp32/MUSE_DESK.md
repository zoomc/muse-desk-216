# 微雪 2.16 Muse 精简版

基于原 Muse SDK，只增加高清动漫人偶、大连天气、番茄专注，并保留直接 MiMo TTS 和网关/DNS 覆盖。左右滑动依次是原人偶、原设置、番茄专注、大连天气。

- 人偶：320×320 原生 RGB565 调色板渲染，状态图片和空闲动作共 18 个姿态。素材由本项目生成，位于 assets/muse-desk/；不是长段逐帧动画。
- 天气：设备直接请求 Open-Meteo 的大连坐标，约每 30 分钟更新，开机约 30 秒后首次获取，失败保留缓存。
- 番茄：25 分钟专注、5 分钟休息，每 4 个番茄提供 15 分钟长休息；开始、暂停、重置，到期提示。下一阶段需要点击开始，重启清零。
- MiMo：设备直接请求 mimo-v2.5-tts，茉莉声音、温柔中文提示，无电脑中间服务。
- 网络：DHCP 分配 IP；网关 192.168.5.2、DNS 198.18.0.2。连接其他网段时保留 DHCP 路由。

## 密钥

真实密钥必须只放入被 Git 忽略的 secrets/muse_sdk_token 和 secrets/mimo_api_key，或者环境变量 MUSE_SDK_TOKEN、MIMO_API_KEY。只提交 *.example 模板。

编译后的 sdkconfig、应用固件和设备 NVS 包含凭据，不应发布到仓库。提交的 muse_art.bin 仅包含图片调色板与像素，不含凭据。

## 构建

使用 ESP-IDF v6.0.1，先激活对应的环境。在 esp32 目录执行（PowerShell）：

```powershell
python tools/muse/secrets.py build-muse-waveshare-s3-216/sdkconfig
idf.py -B build-muse-waveshare-s3-216 -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-muse-waveshare-s3-216/sdkconfig '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;devices/sdkconfig.muse;devices/sdkconfig.muse-waveshare-s3-216;devices/sdkconfig.muse-desk' build
```

新建构建目录可避免旧 sdkconfig 覆盖默认值；已有目录应确认 MUSE_DESK_FEATURES、MUSE_TTS_BACKEND_MIMO、HOMEHUB_WIFI_CUSTOM_ROUTE 已启用，分区表使用 partitions_muse_desk.csv。

## 刷入

在 build-muse-waveshare-s3-216 目录执行，端口按实际设备修改：

```powershell
python -m esptool --chip esp32s3 --port COM5 --baud 460800 --before default-reset --after hard-reset write-flash '@flash_args' 0x830000 ../assets/muse-desk/muse_art.bin
```

人物使用独立 4 MB muse_art 分区。原 NVS 和两个 4 MB OTA 应用槽地址不变。不要擦除整块闪存，否则需重新设置和配对。仅更新应用不会替换人物资源。

重新打包图片需要 Pillow，在 esp32 目录执行：

```powershell
python tools/muse/pack_anime.py assets/muse-desk/muse_art.bin assets/muse-desk/anime_sheet.png assets/muse-desk/anime_actions.png
```

## 已验证

微雪 ESP32-S3-Touch-AMOLED-2.16、480×480、16 MB Flash、8 MB PSRAM：编译及刷写校验成功，18 姿态资源加载成功，大连天气获取成功，番茄到期切换到休息成功，MiMo HTTP 200 并输出约 5 秒语音到扬声器路径，Muse 配对并在线。74 项契约测试及计时器、天气解析、MiMo PCM 测试通过；未宣称 Windows 上 POSIX 依赖的完整测试套件全部通过。
