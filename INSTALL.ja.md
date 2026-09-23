# インストールガイド

開発環境なしで、このRPN電卓ファームウェアを Cardputer ADV に書き込む方法です。

## 方法1: ブラウザから書き込む(推奨)

1. **[こちらのインストールページ](https://azek-dev.github.io/cardputer-rpn-calculator/)** を **Chrome** または **Edge** で開く
   (Web Serial API を使うため、Safari / Firefox では動作しません)
2. Cardputer ADV を USB Type-C ケーブルで PC に接続する
3. ページ内のボタンを押し、表示されたポート一覧から接続したデバイスを選んで `INSTALL` を押す
4. 書き込みが終わると自動的に再起動し、電卓画面が表示されます

2回目以降のアップデート時も、保存済みのスタック・メモリレジスタ・Wi-Fi設定は消えません。

本機は積極的にディープスリープするので、**書き込み前に本体上部の G0(BtnA)ボタンを
押して起動しておいてください**。デバイスがポート一覧に出てこないときは、まずこれを
確認してください。

## 方法2: esptool.py で手動書き込み

Web Serial が使えない環境(Firefox など)向けの代替手段です。Python が必要です。

1. 以下の4ファイルを同じフォルダにダウンロードする:
   - [bootloader.bin](docs/firmware/bootloader.bin)
   - [partitions.bin](docs/firmware/partitions.bin)
   - [boot_app0.bin](docs/firmware/boot_app0.bin)
   - [firmware.bin](docs/firmware/firmware.bin)
2. esptool をインストールする:
   ```bash
   pip install esptool
   ```
3. Cardputer ADV を USB Type-C で接続し、シリアルポート名を確認する
   - Mac: `ls /dev/cu.usbmodem*`
   - Windows: デバイスマネージャーで `COM*` を確認
4. 以下のコマンドを実行する(`<PORT>` は自分の環境のポート名に置き換える):
   ```bash
   python -m esptool --chip esp32s3 --port <PORT> --baud 460800 \
     --before default_reset --after hard_reset write_flash -z \
     --flash_mode dio --flash_freq 80m --flash_size 8MB \
     0x0000 bootloader.bin \
     0x8000 partitions.bin \
     0xe000 boot_app0.bin \
     0x10000 firmware.bin
   ```

## 書き込み後の使い方

数値を入力してEnterでスタックに積み、演算子や関数名でその場で変形していきます
(例: `3` Enter `4` Enter `+` で 7)。詳しい使い方は [MANUAL.ja.md](MANUAL.ja.md)、
公式を使った練習例は [PRACTICE.ja.md](PRACTICE.ja.md) を参照してください。

## ソースから自分でビルドしたい場合

[README.md](README.md) の「ビルド・書き込み」セクション
([PlatformIO Core](https://platformio.org/install/cli) が必要)を参照してください。
