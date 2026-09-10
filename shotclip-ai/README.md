# ShotClip AI 0.1.0

完全离线的 AI 剪辑工程生成工作台。它不是另一个完整剪辑软件，而是把大量长视频/音频
转换为带证据的粗剪方案，并输出 Premiere Pro 可导入的 Final Cut Pro 7 XML、SRT、
剪辑报告和证据 JSON。所有模型与媒体处理均在本机进行。

## 当前闭环

1. 批量导入 MP4、MOV、MKV、AVI、WebM、MP3、WAV、AAC、FLAC、M4A、JPG、PNG、WebP。
2. FFprobe 读取媒体信息，Whisper.cpp 每五分钟分段转录并保留原素材时间码。
3. 文稿中心支持关键词检索；配置 `llama-embedding` 与 GGUF 后，会对关键词召回的候选证据进行本地语义重排。
4. 用户描述主题、时长、结构与禁止事项，本地 llama.cpp 生成结构化粗剪方案。
5. 校验器拒绝不存在的素材/证据、越界片段、证据不相交和时间线重叠。
6. 用户审查后输出 XML、SRT、Markdown 报告、证据 JSON 和 ShotClip 工程 JSON。

没有配置 llama.cpp 时会进入“关键词演示模式”，只用于测试交互，不能视为 AI 语义剪辑。
0.1.0 尚未实现说话人识别、视觉理解、全库向量索引、代理预览和 XML 的
Premiere 真机验收。XML 只写入剪辑入出点、轨道、媒体链接和基础序列参数，不承诺传递复杂
效果、字幕样式或第三方插件。

## 开发运行

```sh
npm install
npm run dev
```

## 验证和构建

```sh
npm run typecheck
npm test
npm run build
npm run pack:win
```

`npm run pack:win` 已在 Apple Silicon macOS 上完成交叉打包。首次本地打包前，需要把固定版本的
Windows 运行时准备到 `runtime/win32-x64/ffmpeg`、`runtime/win32-x64/whisper` 和
`runtime/win32-x64/llama`；这些大文件不会提交到 Git。准备一次后，后续 UI 和工作流改动都可直接
在 Mac 本地重复生成 NSIS EXE。GitHub Actions 仅用于干净 Windows 环境复现和正式发布。

macOS 无法运行生成的 EXE，也不能代替 Windows 10/11 与 Premiere Pro 真机验收。发布前仍需检查
安装/卸载、中文及空格路径、NVIDIA Vulkan 加速、长素材转录、XML 媒体重连和字幕时间码。

## 本地运行引擎

“模型”窗口需要配置 FFmpeg、FFprobe、`whisper-cli`、Whisper `.bin` 模型、`llama-cli`
和指令模型 `.gguf`。模型文件不进入安装包。主进程使用参数数组启动白名单程序，不经过 shell；
渲染进程启用 context isolation、关闭 node integration，并且只有有限 IPC 接口。

安装包内的 FFmpeg、Whisper 和 llama.cpp 运行时位于彼此独立的子目录，避免不同项目附带的
同名 `ggml*.dll` 互相覆盖。用户只需在首次启动时选择三个模型文件，无需另外安装运行引擎。

工程和模型路径保存在 Electron `userData`。转录时的 WAV/SRT 和 LLM 提示词只写入系统临时
目录，并在成功、失败或取消后清理。控制台可能包含本地运行引擎的错误输出，不会主动联网。
