# cardputer-rpn-calculator

M5Stack Cardputer ADV (ESP32-S3) 向けの、逆ポーランド記法(RPN)関数電卓ファームウェアです。
HP電卓のような X/Y/Z/T の4段スタックを常時画面に表示し、数値や関数名を入力してEnterで
スタックへ積む・関数を適用する、古典的なRPN操作モデルを採用しています。

代数式(通常の数式をそのまま入力する)電卓は姉妹プロジェクト
[cardputer-adv-calculator](https://github.com/azek-dev/cardputer-adv-calculator) を参照してください。

## 特徴

- HP式 X/Y/Z/T 4段スタックを常時表示(専用画面設計)
- 三角関数・双曲線関数・対数・べき乗・組合せ論など、代数式電卓と同等の関数セット
  (加えて1/x・x²も搭載)
- STO/RCL方式の番号付きメモリレジスタ(0〜9、`sto(n)`/`rcl(n)`)
- Tab補完による関数名入力
- fn+;(ロールアップ) / fn+.(ロールダウン) / fn+S(X⇔Yスワップ)などHP式スタック操作キー
- opt+D で度数法(DEG)/弧度法(RAD)切り替え、opt+- でCHS(符号反転)
- スタック状態とDEG/RAD設定はNVS(内蔵フラッシュ)に自動保存され、電源断後も復元
- `save` コマンドでmicroSDカードへスナップショットを追記保存(`/rpn_log.txt`)
- Wi-Fi/NTP時刻同期、USBマスストレージ(SDカード共有)、アイドルタイムアウト式ディープスリープ
  ([cardputer-common](https://github.com/azek-dev/cardputer-common) 共有ライブラリを使用)
- `help` コマンドで画面上のキー・関数リファレンスを表示

詳しいキー操作・関数一覧は [MANUAL.ja.md](MANUAL.ja.md)、[src/main.cpp](src/main.cpp) 冒頭の
コメント、または実機上で `help` と入力してEnterを押すと確認できます。

## ビルド・書き込み

PlatformIO CLI が必要です(IDE不要)。

```bash
pio run
```

書き込み前に、本機は非常に積極的にディープスリープします。書き込み前に本体上部の
G0(BtnA)ボタンを押して起動しておいてください。また `pio run -t upload` 実行後、
60〜120秒以上ログが止まっても失敗と判断せず、150〜175秒程度かかることがある点に
注意してください。

```bash
pio run -t upload
```

## ライセンス

USBマスストレージの低レベルSD-over-SPIルーチンは
[M5-cardputer-mass-storage](https://github.com/MOY-lightening-firmware/M5-cardputer-mass-storage)
(MIT License, Copyright (c) 2026 OZAN) を参考にしています。
