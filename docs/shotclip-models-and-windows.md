# ShotClip 模型下载与 Windows x64 打包

## 最短使用流程

1. 双击 `ShotClip-0.3.0-Windows-x64-Setup.exe` 完成安装。
2. 打开 ShotClip，按 `Ctrl+Shift+A` 唤出 Agent。
3. 点击“导入本地模型”，选择已下载的 `.gguf` 文件并保存。
4. 导入素材并输入剪辑要求，逐项确认建议后再应用到时间线。

安装包已经包含 `llama-cli.exe`、`whisper-cli.exe`、`ffmpeg.exe` 和 `ffprobe.exe`。
正常使用不需要单独安装 Ollama、Python、FFmpeg 或 llama.cpp，也不需要联网登录。
需要语音转字幕时，再在同一窗口选择 Whisper GGML 模型。

Agent 默认收起，避免压缩预览区和时间线；可通过面板右上角按钮在右侧停靠与独立窗口之间切换。

## 推荐模型

### Agent 规划模型（必选一个）

首选是 Qwen 官方的 `Qwen3-8B-Q5_K_M.gguf`，约 5.85 GB，Apache-2.0。它适合中文
长文本、结构化工具调用，并能在 RTX 4070 Ti 12 GB 上保留合理的 KV Cache 空间。

- 模型页：<https://huggingface.co/Qwen/Qwen3-8B-GGUF>
- Q5_K_M 文件页：<https://huggingface.co/Qwen/Qwen3-8B-GGUF/blob/main/Qwen3-8B-Q5_K_M.gguf>
- 直接下载：<https://huggingface.co/Qwen/Qwen3-8B-GGUF/resolve/main/Qwen3-8B-Q5_K_M.gguf?download=true>
- SHA-256：`068bae163faa96ad48032daf4e071a6a28fe67d8dcc95367609c2ff165e52738`

如果优先考虑结构化 JSON 的稳定性，可改用 `Qwen2.5-7B-Instruct-Q5_K_M`。官方仓库中的
Q5_K_M 是两个分片，必须同时下载；ShotClip 当前模型选择器只接收一个 GGUF 路径，因此
优先使用上面的单文件 Qwen3，或先用 `llama-gguf-split --merge` 合并 Qwen2.5 分片。

- Qwen2.5 官方 GGUF：<https://huggingface.co/Qwen/Qwen2.5-7B-Instruct-GGUF/tree/main>

### 离线转录模型

首选 `ggml-large-v3-turbo-q5_0.bin`，约 547 MiB，在长演讲中文转录的速度、准确度和资源
占用之间较均衡。

- 模型仓库：<https://huggingface.co/ggerganov/whisper.cpp>
- 文件页：<https://huggingface.co/ggerganov/whisper.cpp/blob/main/ggml-large-v3-turbo-q5_0.bin>
- 直接下载：<https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin?download=true>
- SHA-256：`394221709cd5ad1f40c46e6031ca61bce88931e6e088c188294c6d5a55ffa7e2`

模型由用户放在本机任意目录，再在 ShotClip Agent 中选择路径。模型不会被复制进项目，也
不会自动访问网络。Windows 安装包已包含推理程序；可执行文件选择仅保留在高级设置中，供
开发版本和自定义运行时使用。

## Windows 产物

Windows 构建必须在 Windows x64/MSYS2 环境执行。当前工作流：

`Actions → build-shotclip-windows → Run workflow`

完成后会生成一个名为 `ShotClip-0.3.0-Windows-x64` 的构建产物，包含：

- `ShotClip-0.3.0-Windows-x64-Setup.exe`：Inno Setup 安装器。

安装器配置包括 ShotClip 名称和版本、Windows x64、可选择安装目录、开始菜单入口、可选
桌面快捷方式、MLT 文件关联、GPLv3 许可页和黑白 ShotClip 图标。

构建会把 Shotcut/MLT 运行时、FFmpeg、FFprobe、whisper-cli 和 `llama.cpp b10516` Vulkan
x64 运行时放入应用目录。llama.cpp 官方归档使用固定 SHA-256 校验，许可证一并写入安装目录。
构建不打包任何 GGUF/GGML 模型，因此安装包不会因为模型膨胀数 GB。

## 真实性边界

本项目当前所在机器是 Apple Silicon macOS，不能运行 Windows 二进制，也没有 MinGW/Qt/MLT
Windows 交叉工具链或 Windows 容器。因此可在本机验证 CMake、资源、安装脚本和 macOS
构建，但最终 EXE 必须由 Windows runner 生成，并在真实 Windows 10/11 x64 上完成启动、
中文路径、转录、CUDA/Vulkan、导出和卸载验证后，才能称为可发布版本。
