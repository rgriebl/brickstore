# 任务目标

<!-- hy-mt2-i18n:start -->
[English](./README.md) | [中文](./README_zh-CN.md) | **日本語** | [Español](./README_es.md)
<!-- hy-mt2-i18n:end -->

下記のMarkdown形式のデータを日本語に翻訳する。

# 严格约束
1. **只输出訳文**：コードラッパーなしで直接訳文のみを出力する。
2. **结构锁定**：元のMarkdownの構造、インデント、見出しの階層、表、リンク、URL、バッジ、コードブロック、インラインコードは一切変更しない。
3. **选择性翻译**：ユーザーが見ることのできる自然言語の内容（本文、見出し、説明文、表のテキスト）のみを翻訳する。
4. **禁止修改**：コードタグ、キー名、変数プレースホルダー（{{var}}、${var}、%s、%dなど）、コマンド例、ファイルパス、プロジェクト名、API名、パッケージ名、モデル名、識別子、コード記号は**絶対に**翻訳や変更を行わない。原文に対応する訳名が既に記載されている場合は除く。

# データ入力
ソースファイル：README.md

Markdown内容：
<img src="https://raw.githubusercontent.com/rgriebl/brickstore/main/assets/brickstore.png" align="right"
     alt="BrickStoreのロゴ" width="192" height="192">

[![ビルドバッジ](https://img.shields.io/github/actions/workflow/status/rgriebl/brickstore/build_cmake.yml?branch=main&logo=github&label=Build%20matrix)](https://github.com/rgriebl/brickstore/actions)
[![DLバッジ](https://img.shields.io/github/downloads/rgriebl/brickstore/latest/total?label=Latest%20version%20のダウンロード数)](https://github.com/rgriebl/brickstore/releases)
[![DBバッジ](https://img.shields.io/github/v/release/rgriebl/brickstore-database?display_name=リリース情報&label=最終データベース更新日時(UTC))](https://github.com/rgriebl/brickstore-database)
[![LDバッジ](https://img.shields.io/github/v/release/rgriebl/brickstore-ldraw?display_name=リリース情報&label=最終LDraw更新日時(UTC)&color=%23cc4444)](https://github.com/rgriebl/brickstore-ldraw)

> [!注意]
> # ダウンロード方法や利用に関する詳細は、https://www.brickstore.devをご覧ください。

## BrickStore

BrickStoreはBrickLink用のオフライン管理ツールです。**マルチプラットフォーム対応**（Windows、macOS、Linuxはもちろん、iOSやAndroidにも対応）、**多言語対応**（現在は英語、ドイツ語、スペイン語、スウェーデン語、フランス語）、**高速かつ安定**しています。

## ライセンス

BrickStoreはRobert Grieblによって©2004-2026で著作権が保護されており、
[GNU一般公共許諾契約書(GPL)バージョン3](https://www.gnu.org/licenses/gpl-3.0.html)のもとでライセンスが付与されています。
[www.bricklink.com](https://www.bricklink.com)からのすべてのデータはBrickLinkが所有しています。BrickLinkおよびLEGOはLEGOグループの商標であり、このソフトウェアを後援、認可、または支持していません。その他のすべての商標も有効です。

[Dan Jezek氏](https://www.danjezek.com/)のサポートによってのみ実現可能です。

## サードパーティコンポーネント

このプロジェクトには、異なるライセンスの下で提供されているその他のオープンソースのサードパーティコンポーネントが含まれています：
* assets/icons：COPYING-ICONSおよびCOPYING-FLAGSを参照
* 3rdparty/minizip：ZLIBライセンス
* 3rdparty/lzma：LGPL-2.1ライセンス
* 3rdpath/qtwinextras：GPL-3ライセンス
* 3rdpath/qtsingleapplication：BSD-3ライセンス
* https://github.com/danvratil/qcoro：MITライセンス
* https://github.com/getsentry/sentry-native.git：MITライセンス
* https://github.com/FedoraQt/QAdwaitaDecorations.git：LGPL-2.1ライセンス
