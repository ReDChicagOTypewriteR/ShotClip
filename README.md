# ShotClip 0.3.0 Demo

ShotClip 是基于 Shotcut/MLT 的完全离线剪辑实验分支。0.3.0 保留 Shotcut 已验证的
专业时间线、字幕、滤镜、音频与导出能力，并新增一个原生的 **ShotClip Agent** 工作区，
用于先生成结构化剪辑建议、由用户确认，再写入时间线。

Windows 安装版采用“一次安装、本地模型即插即用”的方式：安装包内置 FFmpeg、FFprobe、
whisper.cpp 与 llama.cpp 运行引擎，不内置数 GB 的模型文件。安装后按 `Ctrl+Shift+A`
打开 Agent，点击“导入本地模型”，选择 GGUF 文件即可开始使用；Whisper 转录模型为可选项。
Agent 默认收起以保留时间线空间，也可以停靠在主界面右侧或弹出为独立窗口。

- [0.3.0 llama.cpp 模型导入与结构化 Agent 工具](docs/shotclip-0.3.0.md)
- [推荐模型下载与 Windows x64 打包说明](docs/shotclip-models-and-windows.md)
- [0.2.0 功能、使用方式与已知限制](docs/shotclip-0.2.0.md)
- [0.0.9 初始 Demo 记录](docs/shotclip-0.0.9.md)
- [长期架构与版本路线](docs/shotclip-architecture.md)

> 0.3.0 从本机导入 GGUF 模型，用严格 JSON Schema 生成白名单工具方案；方案经过边界、
> 重叠与时间线版本校验，并且必须由用户勾选确认。Windows 安装包由 GitHub Actions 构建，
> 内置固定版本并经过 SHA-256 校验的推理运行时，但模型仍由用户自行下载和选择。

---

[![build-shotcut-linux](https://github.com/mltframework/shotcut/workflows/build-shotcut-linux/badge.svg)](https://github.com/mltframework/shotcut/actions?query=workflow%3Abuild-shotcut-linux+is%3Acompleted+branch%3Amaster)
[![build-shotcut-macos](https://github.com/mltframework/shotcut/workflows/build-shotcut-macos/badge.svg)](https://github.com/mltframework/shotcut/actions?query=workflow%3Abuild-shotcut-macos+is%3Acompleted+branch%3Amaster)
[![build-shotcut-windows](https://github.com/mltframework/shotcut/workflows/build-shotcut-windows/badge.svg)](https://github.com/mltframework/shotcut/actions?query=workflow%3Abuild-shotcut-windows+is%3Acompleted+branch%3Amaster)


# Shotcut - a free, open source, cross-platform **video editor**

<div align="center">

<img src="https://www.shotcut.org/assets/img/screenshots/Shotcut-18.11.18.png" alt="screenshot" />

</div>

- Features: https://www.shotcut.org/features/
- Roadmap: https://www.shotcut.org/roadmap/

## Install

Binaries are regularly built and are available at https://www.shotcut.org/download/.

## Contributors

- Dan Dennedy <<http://www.dennedy.org>> : main author
- Brian Matherly <<code@brianmatherly.com>> : contributor

## Dependencies

Shotcut's direct (linked or hard runtime) dependencies are:

- [MLT](https://www.mltframework.org/): multimedia authoring framework
- [Qt 6 (6.4 minimum)](https://www.qt.io/): application and UI framework
- [FFTW](https://fftw.org/)
- [FFmpeg](https://www.ffmpeg.org/): multimedia format and codec libraries
- [Frei0r](https://www.dyne.org/software/frei0r/): video plugins
- [SDL](http://www.libsdl.org/): cross-platform audio playback

See https://shotcut.org/credits/ for a more complete list including indirect
and bundled dependencies.

## License

GPLv3. See [COPYING](COPYING).

## How to build

**Warning**: building Shotcut should only be reserved to beta testers or contributors who know what they are doing.

### Qt Creator

The fastest way to build and try Shotcut development version is through [Qt Creator](https://www.qt.io/download#qt-creator).

### From command line

First, check dependencies are satisfied and various paths are correctly set to find different libraries and include files (Qt, MLT, frei0r and so forth).

#### Configure

In a new directory in which to make the build (separate from the source):

```
cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ /path/to/shotcut
```

We recommend using the Ninja generator by adding `-GNinja` to the above command line.

#### Build

```
cmake --build .
```

#### Install

If you do not install, Shotcut may fail when you run it because it cannot locate its QML
files that it reads at run-time.

```
cmake --install .
```

## Translation

If you want to translate Shotcut to another language, please use [Transifex](https://explore.transifex.com/ddennedy/shotcut/).
