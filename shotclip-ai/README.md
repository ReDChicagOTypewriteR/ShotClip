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

macOS 不能可靠地验证最终 Windows 安装行为。建议由 GitHub Actions 或 Windows x64 主机运行
`npm run pack:win`，然后在真实 Premiere Pro 中导入 XML，检查中文/空格路径、帧率、音视频
链接和字幕时间码。

## 本地运行引擎

“模型”窗口需要配置 FFmpeg、FFprobe、`whisper-cli`、Whisper `.bin` 模型、`llama-cli`
和指令模型 `.gguf`。模型文件不进入安装包。主进程使用参数数组启动白名单程序，不经过 shell；
渲染进程启用 context isolation、关闭 node integration，并且只有有限 IPC 接口。

工程和模型路径保存在 Electron `userData`。转录时的 WAV/SRT 和 LLM 提示词只写入系统临时
目录，并在成功、失败或取消后清理。控制台可能包含本地运行引擎的错误输出，不会主动联网。
