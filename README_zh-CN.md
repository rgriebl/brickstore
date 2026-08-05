# 任务目标

<!-- hy-mt2-i18n:start -->
[English](./README.md) | **中文** | [日本語](./README_ja.md) | [Español](./README_es.md)
<!-- hy-mt2-i18n:end -->

将下方 Markdown 格式数据翻译为中文。

# 严格约束
1. **只输出译文**。不要添加任何额外的文字内容。
2. **结构锁定**：必须完全保留原有的 Markdown 数据结构、缩进、标题层级、表格、链接、URL、徽章、代码块和行内代码，不得有任何改动。
3. **选择性翻译**：仅对面向用户显示的可见自然语言内容（正文、标题、说明文字和表格文本）进行翻译。
4. **禁止修改**：**严禁**翻译或更改代码标签、键名、变量占位符（如 {{var}}、${var}、%s、%d 等）、命令示例、文件路径、项目名、API 名、包名、模型名、标识符和代码符号；除非原文已经给出对应译名。

# 数据输入
源文件：README.md

Markdown 内容：
<img src="https://raw.githubusercontent.com/rgriebl/brickstore/main/assets/brickstore.png" align="right"
     alt="BrickStore Logo" width="192" height="192">

[![构建状态徽章](https://img.shields.io/github/actions/workflow/status/rgriebl/brickstore/build_cmake.yml?branch=main&logo=github&label=Build%20matrix)](https://github.com/rgriebl/brickstore/actions)
[![下载量徽章](https://img.shields.io/github/downloads/rgriebl/brickstore/latest/total?label=Downloads%20for%20latest%20version)](https://github.com/rgriebl/brickstore/releases)
[![数据库版本徽章](https://img.shields.io/github/v/release/rgriebl/brickstore-database?display_name=release&label=Last%20database%20update%20(UTC))](https://github.com/rgriebl/brickstore-database)
[![LDraw 版本徽章](https://img.shields.io/github/v/release/rgriebl/brickstore-ldraw?display_name=release&label=Last%20LDraw%20update%20(UTC)&color=%23cc4444)](https://github.com/rgriebl/brickstore-ldraw)

> [!注意]
> # 如需了解更多关于下载及使用的信息，请访问 https://www.brickstore.dev。

## BrickStore

BrickStore 是一款用于 BrickLink 的离线管理工具。它具备**多平台支持**（Windows、macOS 和 Linux，以及 iOS 和 Android），支持**多种语言**（目前包括英语、德语、西班牙语、瑞典语和法语），并且运行**快速且稳定**。

## 许可证

BrickStore 的版权所有者为 Robert Griebl，版权时间为 ©2004-2026，该软件依据
[GNU 通用公共许可证（GPL）第 3 版](https://www.gnu.org/licenses/gpl-3.0.html)进行授权。
[www.bricklink.com](https://www.bricklink.com)上的所有数据均归 BrickLink 所有。BrickLink 和 LEGO 均为 LEGO 集团的商标，该集团并未赞助、授权或认可此软件。其他所有商标均受到相应保护。

感谢 [Dan Jezek](https://www.danjezek.com/) 的支持，才使得该项目得以实现。

## 第三方组件

该项目还包含了其他遵循不同许可证协议的开源第三方组件：
* assets/icons：请参阅 COPYING-ICONS 和 COPYING-FLAGS
* 3rdparty/minizip：采用 ZLIB 许可证
* 3rdparty/lzma：采用 LGPL-2.1 许可证
* 3rdpath/qtwinextras：采用 GPL-3 许可证
* 3rdpath/qtsingleapplication：采用 BSD-3 许可证
* https://github.com/danvratil/qcoro：采用 MIT 许可证
* https://github.com/getsentry/sentry-native.git：采用 MIT 许可证
* https://github.com/FedoraQt/QAdwaitaDecorations.git：采用 LGPL-2.1 许可证
