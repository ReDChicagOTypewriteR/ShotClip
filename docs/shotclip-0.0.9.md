# ShotClip 0.0.9 Demo

## 这一版解决什么

0.0.9 不再从零制造一个不可靠的视频编辑器，而是在 Shotcut + MLT 已有能力上建立
ShotClip 的离线 Agent 层。核心原则是：稳定剪辑内核负责确定性操作，模型只负责理解、
规划和解释；任何模型建议都必须经过校验和用户确认。

这一版已经具备：

- ShotClip 应用名、0.0.9 版本号、矢量标志和 About 信息。
- 右侧原生 `ShotClip Agent` Dock，可与播放预览、素材、滤镜、时间线无缝切换。
- 完全离线规则模式，不启动 Ollama，不访问互联网。
- 自然语言规则：`删除开头 5 秒`、`删除结尾 3 秒`、`保留 10 秒到 30 秒`、
  `撤销上一次修改`。
- 时间范围严格校验；无效方案不会修改时间线。
- 建议列表支持定位试听、接受、拒绝和统一应用。
- 应用方案时自动分割、删除并闭合当前轨道空隙，整批操作可以一次 `Ctrl+Z` 撤销。
- “剪成 10 分钟”等语义任务会明确显示 Whisper 转录、内容规划、人工确认三个步骤；
  0.0.9 没有本地模型时不会假装已经理解视频。
- 本地模型设置可登记 Whisper `.bin` 和规划模型 `.gguf` 路径，但 0.0.9 暂不加载。
- 可折叠运行日志和磁盘日志，便于定位问题。
- Shotcut 原有的素材导入、多轨时间线、裁剪、分割、转场、滤镜、文字/字幕、音频处理、
  代理媒体、项目保存和导出均继续由原生内核提供。

## 推荐体验流程

1. 打开 ShotClip，新建项目并设置视频模式。
2. 使用“打开文件”载入视频，再拖到时间线；或直接把本地素材拖入播放列表/时间线。
3. 先用 Shotcut 原生时间线完成移动、裁剪、分割、多轨、文字、字幕、转场和声音调整。
4. 打开右侧“AI 助理”（快捷键 `Ctrl+Shift+A`）。
5. 输入 `删除开头 5 秒`，点击“分析并生成方案”。
6. 先选择建议并点击“定位试听”，确认后勾选/接受，再点击“应用已接受建议”。
7. 用 `Ctrl+Z` 整体撤销这次 Agent 修改，或继续编辑并从“导出”面板输出视频。
8. 遇到问题时展开 Agent 的“运行日志”。完整日志位于应用数据目录中的
   `shotclip-log.txt`。

## 本机开发运行（macOS）

依赖：CMake、Ninja、Qt 6、MLT 7、FFTW。本机已使用 Homebrew 依赖完成验证。

```bash
cmake -S . -B build/shotclip-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DSHOTCUT_VERSION=26.8.23 \
  -DSHOTCLIP_VERSION=0.0.9 \
  -DSHOTCUT_BUILD_TESTS=ON \
  -DCLANG_FORMAT=OFF
cmake --build build/shotclip-debug --target qm shotcut -j 8
open build/shotclip-debug/src/ShotClip.app
```

规则解析单元测试：

```bash
cmake --build build/shotclip-debug --target test_agent_rule_engine -j 8
ctest --test-dir build/shotclip-debug -R agent_rule_engine --output-on-failure
```

## 当前限制

- 规则执行器目前只操作“当前轨道”，不会自动修改其他音视频轨道。应用后必须检查同步；
  正式版将使用稳定 Clip ID、链接组和跨轨道编辑事务。
- 本地模型路径仅保存到本机设置，不写入项目，也不会打包模型；模型运行时尚未接入。
- “演讲剪成 10 分钟”“保留现身说法的事件、后果和警示”等语义任务需要后续的
  Whisper 转录、分章和本地 LLM 规划，当前只生成可审查的任务草稿。
- macOS 当前使用上游 `shotcut.icns` 作为应用图标占位，界面内已经使用 ShotClip SVG 标志。
- 这次按要求不生成 Windows EXE。Windows x64、硬件编码、安装路径、资源许可和断网审计
  必须在后续里程碑中单独验证，未经真机验证不得宣称可发布。
- Agent 不会联网；上游 Shotcut 某些由用户主动打开的外部链接/资源入口尚未做完整移除。
  正式离线发布前必须做出站网络审计并禁用不需要的入口。

## 许可证与分发

ShotClip 基于 GPLv3 的 Shotcut，使用 MLT、Qt 和 FFmpeg 等项目。分发修改版时必须遵守
GPLv3，提供完整对应源代码、保留版权/许可证声明，并复核各编解码器、模型和模型权重的
许可证。0.0.9 不包含任何模型权重，也没有把 Ollama 打进应用。
