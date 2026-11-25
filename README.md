# Euclase

Euclase は、Qt 6 と C++17 で実装されたデスクトップ画像編集アプリです。ブラシ・消しゴム、矩形選択、画像のリサイズと切り抜き、ぼかしなどのフィルタを備えています。キャンバス内部はレイヤーと 256×256 ピクセルのタイルで構成されています。

PNG と JPEG の読み込み・保存に対応しています。保存時は `.png`、`.jpg`、`.jpeg` のいずれかをファイル名に付けてください。

## ビルドと起動（Linux）

必要なものは、C++17 対応のコンパイラ、`make`、Qt 6 の開発用ファイル（Core、Gui、Widgets、Svg と `qmake6`）、OpenMP、libpng と libjpeg の開発用ファイルです。GUI の起動にはデスクトップ環境が必要です。

リポジトリのルートから実行します。

```sh
mkdir -p build
cd build
qmake6 ../Euclase.pro -after DESTDIR=.
make -j2
./Euclase
```

`Euclase.pro` がアプリ本体のビルド定義です。`-after DESTDIR=.` は、実行ファイルを `build/` に置くための指定です。

## CUDA プラグイン（任意）

アプリ本体は CUDA プラグインなしでも起動します。Linux で CUDA を使う場合は、対応する CUDA Toolkit と GPU 環境を用意し、リポジトリのルートから次を実行します。

```sh
make -C libEuclaseCUDA libEuclaseCUDA.so
cp libEuclaseCUDA/libEuclaseCUDA.so build/
```

起動時にアプリは実行ファイルと同じディレクトリの `libEuclaseCUDA.so` を探します。プラグインを読み込めない場合は CPU 上で画像を処理します。

## 主なファイル

| ファイル | 役割 |
| --- | --- |
| `Euclase.pro` | Qt アプリのビルド定義 |
| `MainWindow.cpp` / `.ui` | メニュー、ツール、ファイル操作 |
| `Canvas.cpp` / `.h` | レイヤー、タイル、選択、合成 |
| `ImageViewWidget.cpp` | キャンバスの表示と更新 |
| `euclase.cpp` / `.h` | 画像バッファ、画素形式、変換、リサイズ、ぼかし |
| `png.cpph` / `jpeg.cpph` | 画像ファイルの入出力 |
| `libEuclaseCUDA/` | 任意の CUDA プラグイン |

既存の安全性・描画処理の修正履歴と検証内容は [PLAN.md](PLAN.md) にまとめています。開発時の注意事項は [AGENTS.md](AGENTS.md) を参照してください。
