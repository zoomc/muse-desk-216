# UPSTREAM — 上游基线与同步方法

本仓库 fork 自官方 `facebookincubator/muse-gadget-sdk`，保留上游历史。

- 上游基线：`7e7123e2815d3e7e3c0f2ca330f576290ae6a6a9`（Initial commit，
  已用 `git log --all` 在本仓库核实存在，且为当前 HEAD 的祖先）。
- 移植来源（非上游，供追溯；详见 `docs/PORTING.md`）：
  - wupsbr fork: <https://github.com/wupsbr/waveshare-muse-gadget-sdk.git> @ `2c648812feb606a85043e368e711e0ca82d61ab9`
  - box3 中文 TTS: <https://github.com/isamu2025/muse-box3-chinese-tts.git> @ `c9f03e7d9ceec6f405b2f6fd0171f9aa041d9034`
  - muse-client（relay）: <https://github.com/wong2/muse-client.git> @ `89a3feaeac8d18f912c33f621e5f59c300b87d36`

## 同步上游（二选一）

首次先加 remote（只需一次）：

```sh
git remote add upstream https://github.com/facebookincubator/muse-gadget-sdk.git
git fetch upstream main
```

选一种合入本地分支：

```sh
# A. merge（保留分叉历史，推荐日常同步）
git checkout feature/desk-216
git merge upstream/main

# B. rebase（线性历史，推送前整理时用；已推分支慎用）
git checkout feature/desk-216
git rebase upstream/main
```

合入后按 `docs/PORTING.md` 的移植点逐项确认无回归，再跑 `esp32/tools/muse/board.sh build s3-216`。
绝不 `git push`（见任务铁律；推送由用户在仓库外显式操作）。
