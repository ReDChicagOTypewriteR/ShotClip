# ShotClip 架构与路线

## 产品定位

ShotClip 是一个面向长篇演讲、访谈和“现身说法”素材的专业离线剪辑工具。目标用户通常
拥有数十分钟到数小时原片，希望得到约 10 分钟、保持原意且可人工审核的成片。产品不是
“一句话后黑盒导出”，而是 Shotcut 专业时间线与可控 Agent 的一体化工作台。

稳定性优先级为：稳定性 > 编辑正确性 > 性能 > 功能数量 > 视觉效果。

## 用户完整工作流

创建/打开项目 → 导入视频、图片和音频 → 素材预览与整理 → 拖入多轨时间线 → 手动剪辑或
让 Agent 生成粗剪方案 → 逐项试听、接受或拒绝 → 添加文字、字幕、音乐、转场和画面/声音
调整 → 全片预览 → MLT/FFmpeg 本地渲染 → 导出 MP4。

## 界面布局

- 顶部：项目、导入、保存、撤销/重做、导出和工作区入口。
- 左侧：播放列表/素材、滤镜、转场、文字与字幕等工具 Dock。
- 中间：源/项目播放器，播放头与时间线同步。
- 右侧：属性面板与 ShotClip Agent 以 Dock/标签方式切换。
- 底部：Shotcut 原生多轨时间线，支持视频、音频、字幕、播放头、缩放和滚动。

Agent 不覆盖专业工具：选中普通工具时显示参数；切到 Agent 时显示上下文、目标、建议和
日志；接受建议后仍然落到同一条可手工修改的时间线上。

## 功能优先级

### P0：可用编辑器 + 可控粗剪

- 项目新建、打开、保存、自动恢复。
- 视频/音频/图片素材管理与预览。
- 多轨时间线、移动、裁剪、分割、删除、复制、撤销/重做。
- 画面变换、音量、文字、手动字幕、MP4 导出与进度。
- Agent 建议协议、严格校验、人工确认、原子应用和整体撤销。
- 离线规则模式与本地运行日志。

### P1：真正适合长演讲

- whisper.cpp 离线转录、时间戳、说话人/段落检测和 SRT。
- 静音、重复句、口头语、低信息密度片段候选检测。
- llama.cpp 本地 GGUF 内容规划，支持“忠实/标准/精简”和目标时长预算（0.3.0 已接入
  基础结构化方案；长文分块与全片质量评测仍待完成）。
- 代理媒体、转录缓存、增量分析、任务暂停/恢复。
- 跨轨道链接组、批量 Agent 修改事务与 A/V 同步安全检查。

### P2：专业 AI 工作流

- 多步骤 Agent：分析、粗剪、字幕、包装、质检、导出检查。
- 场景/镜头/音频波形等多模态索引。
- 可插拔本地模型 Provider、工具协议和插件权限。
- 本地 AI 降噪、抠图、智能卡点、防抖、绿幕、关键帧建议。

## 数据模型

继续以 MLT XML 作为可播放、可导出的时间线事实来源。ShotClip 扩展数据应放入独立的
项目旁车 JSON，或写入明确命名的 MLT property，避免复制原始素材。

```text
Project
  id, name, projectPath, profile, fps, canvas
  assets[]              -> Asset
  timeline              -> MLT project + stable ShotClip IDs
  agentSessions[]       -> AgentSession
  transcriptRefs[]      -> local cache references
  exportPreset

Asset
  id, originalPath, kind, duration, width, height, fps, audioInfo
  fingerprint, missing, proxyPath?, transcriptPath?

Track
  id, kind(video|audio|subtitle), order, locked, hidden, muted, clips[]

Clip
  id, assetId, trackId, sourceIn, sourceOut, timelineStart, speed
  transform, crop, color, audio, transitions, linkGroupId?

AgentEditOperation
  id, type, targetIds[], range, parameters, reason, confidence, risk
  preconditions[], accepted, executable
```

所有时间使用整数帧或明确 timebase 的整数时间戳，不用浮点数作为持久化真值。

## Agent 分层

```text
用户指令
  → Context Builder（只提供素材 ID、转录、时间线结构，不提供本地绝对路径）
  → Local Provider（规则 / whisper.cpp / llama.cpp）
  → Plan JSON Schema
  → Validator（边界、ID、重叠、轨道锁、A/V 链接、持续时间）
  → Review UI（试听、解释、接受/拒绝）
  → Command Compiler（转换为 MLT 时间线命令）
  → Undo Transaction（一次应用、一次撤销）
```

模型永远不能直接调用 MLT 或写项目文件。每个工具有白名单、参数 Schema、只读预览阶段和
显式提交阶段。失败时回滚整批事务并保留诊断日志。

## 本地模型建议

适合目标主机（i9-14900KF、64 GB、RTX 4070 Ti）的首选组合：

- 转录：whisper.cpp + Whisper large-v3-turbo；可量化模型降低显存/内存占用。
- 内容规划：llama.cpp + Qwen 系列 7B/8B Instruct 的 Q4/Q5 GGUF。
- 可选视觉理解：先使用镜头切分和抽帧索引，确有必要时再接入 7B 级视觉语言模型。

模型由用户从本地路径注册，ShotClip 不打包 Ollama，也不复制模型进项目。Provider 接口
负责能力探测、加载/卸载、进度、取消和结构化输出。正式版必须为模型文件提供哈希、版本、
许可证和兼容性记录。

## 预览、导出与性能

- 编辑阶段只修改参数与 MLT 时间线，不重复全片编码。
- 播放器从时间线图实时合成；低性能时降低预览分辨率。
- 4K/长视频生成 720p 代理，导出时重新链接原始文件。
- 转录、缩略图、波形和代理按素材指纹缓存，可取消、可恢复、可清理。
- 导出统一走 MLT consumer/FFmpeg，本地 H.264 + AAC MP4 为兼容基线；硬件编码必须回退到
  CPU，并用短片段验证输出时长、音视频同步和可解码性。

## Undo、自动保存与恢复

- 手动操作和 Agent 操作统一进入 Qt `QUndoStack`。
- Agent 的多条命令组成一个宏事务，写入前记录时间线版本与前置条件。
- 编辑发生后使用防抖自动保存；崩溃恢复文件与显式项目文件分离。
- 启动时检测未正常关闭标记，展示恢复时间、原项目和只读预览，不静默覆盖。
- 素材丢失时按 fingerprint、文件名、大小、时长辅助重新定位。

## 目录演进

```text
src/agent/             计划协议、规则引擎、Provider、校验器、命令编译器
src/docks/             Agent 与任务/日志 Dock
src/models/            时间线和字幕模型（上游）
src/commands/          统一 Undo 命令（上游 + ShotClip 扩展）
src/jobs/              转码、代理、转录等后台任务
src/resources/         ShotClip 品牌资源
tests/                 规则、Schema、事务和导出测试
docs/                  产品、架构、离线与发布说明
```

## MVP 开发顺序

1. 0.0.9：品牌、Agent Dock、离线规则、建议确认、时间线事务、日志（已完成）。
2. 0.1.x：稳定 Clip ID、跨轨道同步编辑、自动保存与恢复（已完成）。
3. 0.2.x：whisper.cpp 转录、字幕和文本定位（基础接入已完成；分章和缓存待完善）。
4. 0.3.x：llama.cpp 规划与结构化工具（基础接入已完成）；下一步完成长演讲分块、十分钟
   粗剪和评测集。
5. 0.4.x：代理媒体、后台任务恢复、导出质检和 Windows 真机验证。
6. 1.0：发布级签名、安装、许可证清单、模型管理和插件权限系统。
