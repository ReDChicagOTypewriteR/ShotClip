# ShotClip 0.3.0 Demo

## 本版本范围

0.3.0 在 0.2.0 的稳定 ID、跨轨编辑、恢复和离线转录之上，加入可替换的本地
`llama.cpp` 规划层。模型只负责提出结构化方案，不能执行命令、读写项目或直接操作 MLT。
所有时间线变更仍由 ShotClip 白名单工具校验、用户确认和原子撤销事务完成。

本版本不下载、不复制、不打包 `llama.cpp` 或模型，不使用 Ollama，不访问云端，也不生成
Windows EXE。

## 配置本地规划模型

准备近期版本的 `llama-cli` 和一个本地 GGUF 指令模型。对于 64 GB 内存与 RTX 4070 Ti，
可以先从 7B/8B Instruct 的 Q4/Q5 GGUF 开始，再根据速度和方案质量调整。ShotClip 不校验
具体模型许可，分发前必须单独核查推理程序、模型权重和训练数据许可。

1. 打开右侧“ShotClip Agent”，点击设置。
2. 选择本机 `llama-cli` 可执行文件和 `.gguf` 模型文件。
3. 设置上下文长度、CPU 线程和 GPU 层数。默认上下文 8192，GPU 层 999 表示尽量卸载到
   GPU；如果显存不足可逐步降低。
4. 建议先使用 0.2.0 的“转录时间线”生成字幕，再输入剪辑目标。
5. 点击“分析并生成方案”。执行中可停止，失败不会修改时间线。
6. 检查每项时间范围和原因，逐项勾选后点击“应用已接受方案”。

设置只保存本机路径和数值，不把绝对素材路径或提示词写入项目。提示词通过系统临时文件传给
本地进程，以兼容长上下文以及包含空格、中文的路径；临时文件在任务结束后删除。

## 结构化 Agent 协议

模型响应必须符合版本化 JSON Schema：

```json
{
  "version": "1.0",
  "context_id": "当前时间线上下文 ID",
  "summary": "方案摘要",
  "tool_calls": [
    {
      "id": "call-1",
      "name": "timeline.ripple_delete_range",
      "arguments": { "start_seconds": 0, "end_seconds": 5 },
      "reason": "删除无内容的开场"
    }
  ]
}
```

当前可写工具只有：

- `timeline.ripple_delete_range`：跨全部未锁定轨道安全删除时间范围并闭合空隙。
- `timeline.keep_range`：只保留一个有效时间范围。

模型获得的只读上下文包含项目/轨道/片段稳定 ID、素材与时间线范围、字幕片段和播放头，不
包含媒体绝对路径。上下文声明的只读能力为 `project.get_metadata`、
`timeline.list_tracks`、`timeline.list_clips` 和 `subtitles.list_segments`，它们不是模型能
直接调用的进程接口。

## 安全与编辑正确性

- JSON 以全有或全无方式解析；未知字段、未知工具、非法 ID、非数值参数都会拒绝整份方案。
- 校验负数、超出时间线、入点不小于出点、重复调用、删除范围重叠等错误。
- `keep_range` 不允许与其他修改混用，避免语义不确定。
- 上下文生成 SHA-256 版本 ID；模型分析后若时间线内容变化，旧方案立即失效。
- 模型方案默认不勾选，必须由用户显式确认；应用时再次比较上下文 ID。
- 执行沿用 0.1.0 的跨轨事务与锁轨检查，可通过一次撤销完整还原。
- 本地进程有十分钟超时、取消、退出码检查和 4 MB 输出上限；日志不记录完整提示词或模型
  原始响应。

## 开发与验证

```bash
cmake -S . -B build/shotclip-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DSHOTCUT_VERSION=0.3.0 \
  -DSHOTCLIP_VERSION=0.3.0 \
  -DSHOTCUT_BUILD_TESTS=ON \
  -DCLANG_FORMAT=OFF
cmake --build build/shotclip-debug --target qm shotcut -j 8
ctest --test-dir build/shotclip-debug --output-on-failure
open build/shotclip-debug/src/ShotClip.app
```

测试覆盖结构化方案成功解析、未知工具、过期上下文、重叠范围、工具冲突，以及模拟本地
`llama-cli` 子进程到方案解析的异步闭环。

## 已知限制与下一步

- 当前开发机没有真实 `llama-cli` 和 GGUF 文件，因此本版本只完成编译、无模型状态、严格
  解析器和模拟本地进程集成测试；真实中文长素材的模型质量和吞吐尚未验证。
- 为控制上下文，当前最多带入 600 条字幕且字幕文本总量约 24,000 字符。数小时演讲需要
  后续加入按章节分块、摘要合并和局部证据回查，不能依赖单次长上下文。
- 结构化写工具目前只有“删除范围”和“保留范围”。重排、按稳定 Clip ID 精修、字幕清理与
  包装工具需要逐项增加 Schema、预演和回归测试，不能允许模型自由生成命令。
- 当前不内置模型文件哈希、许可证清单和兼容性数据库。
- Windows CUDA/Vulkan、中文路径、长时间运行、取消和真实模型显存占用需要在目标 Windows
  机器验证。本阶段明确不生成 EXE。
