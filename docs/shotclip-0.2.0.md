# ShotClip 0.2.0 Demo

## 本版本范围

0.2.0 合并了两个连续里程碑：0.1.0 的编辑安全基础，以及 0.2.0 的完全离线转录入口。
稳定性和编辑正确性优先，未加入在线模型服务，也未生成 Windows 安装包。

### 0.1.0：跨轨安全、稳定 ID、自动保存与恢复

- Agent 的时间范围删除改为跨全部视频/音频轨道的涟漪删除。
- 修改前检查所有轨道；只要存在锁定轨道就拒绝整次操作，避免音画不同步。
- 同一次编辑同步处理时间线标记和全部字幕轨。
- 视频、音频、字幕与标记合并为一个撤销事务，可用一次 `Ctrl+Z` 完整还原。
- 项目、轨道、片段和链接组获得持久 UUID；分割产生重复 ID 时自动重新分配。
- 手动保存和自动恢复文件都会写入这些 ID，不复制媒体文件。
- 复用 Shotcut 的 60 秒自动保存与异常恢复机制，并在 Agent 面板提供“立即创建恢复点”。

### 0.2.0：whisper.cpp、字幕与文本定位

- 复用 Shotcut 的本地转录作业：先从所选时间线音轨提取 16 kHz 单声道 WAV，再调用
  用户指定的 `whisper.cpp` 可执行文件和 GGML `.bin` 模型，最后导入 SRT 为字幕轨。
- Agent 面板新增“转录时间线”和“字幕与文稿”入口。
- 字幕面板支持手工编辑、导入/导出 SRT、搜索文本，以及双击字幕定位播放头。
- GPU 转录可选；GPU 失败时沿用上游作业的 CPU 回退逻辑。
- `SHOTCLIP_OFFLINE` 构建关闭模型列表刷新、自动下载、点击下载和模型 URL 入口。
- 没有配置 whisper.cpp 或模型时，确认按钮禁用，时间线和手工字幕功能仍可使用。

## 使用本机模型

ShotClip 不附带模型。先自行准备与你的平台匹配的 `whisper.cpp` 命令行程序，以及
whisper.cpp GGML 格式模型（建议先试 `ggml-large-v3-turbo.bin`）。然后：

1. 打开右侧 `ShotClip Agent`，点击右上角设置。
2. 选择本机 whisper.cpp 可执行文件和 GGML `.bin` 模型，可选择 GPU。
3. 把长视频放入时间线，点击“转录时间线”。
4. 选择包含讲话的音轨、语言、字幕行长后确认。
5. 在“任务”面板观察音频提取和转录进度；完成后字幕自动进入新字幕轨。
6. 打开“字幕与文稿”，搜索关键词或双击字幕即可定位原视频位置。

整个过程只读取本机项目、素材、程序和模型。模型路径保存在本机设置中，不写入项目。

## 本机开发运行（macOS）

```bash
cmake -S . -B build/shotclip-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DSHOTCUT_VERSION=0.2.0 \
  -DSHOTCLIP_VERSION=0.2.0 \
  -DSHOTCUT_BUILD_TESTS=ON \
  -DCLANG_FORMAT=OFF
cmake --build build/shotclip-debug --target qm shotcut -j 8
open build/shotclip-debug/src/ShotClip.app
```

运行测试：

```bash
ctest --test-dir build/shotclip-debug --output-on-failure
```

## 已知限制

- 本机当前没有配置 whisper.cpp 可执行文件和 GGML 模型，因此只能验证无模型状态、配置
  入口、作业代码、编译和字幕逻辑；真实中文长视频转录需要用户提供模型后验证。
- 本版没有语义 LLM，不会根据演讲内容自动挑选十分钟成片；这一层将消费转录文本并输出
  可审查的稳定 Clip ID 编辑计划。
- 跨轨 Agent 删除采取严格策略：任何轨道锁定都会取消操作。后续可增加“锁轨保持位置”
  与“解锁后同步”两种显式策略，但不能静默造成错位。
- 自动保存负责项目恢复，不替代用户的正式保存和版本备份。
- 本阶段不生成 EXE；Windows、CUDA/Vulkan 加速和长视频吞吐必须在 Windows 真机验证。
- Shotcut 上游仍包含用户主动触发的帮助链接等网络入口；模型转录路径已离线封闭，但正式
  发布前仍需完成整应用出站网络审计。

## 许可证

ShotClip 基于 GPLv3 的 Shotcut/MLT。whisper.cpp 程序与模型权重由用户自行提供；发布时
需要分别核查程序、模型和训练数据衍生许可。本版本不包含模型、Ollama 或下载器资源。
