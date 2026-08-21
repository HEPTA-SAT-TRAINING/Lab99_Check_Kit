# Lab99 Check Kit

HEPTA-SAT Full の手動動作確認用 Arduino スケッチです。

スタッフ用のフルキット（ステーション自動検査・Python ツール）は別リポジトリです:  
https://github.com/HEPTA-SAT-TRAINING/Lab99_Check_Kit_Staff

## 内容

| パス | 説明 |
| --- | --- |
| `Lab99_Check_Kit.ino` | 手動検証スケッチ |
| `empty_sketch/empty_sketch.ino` | 検査後に残す空スケッチ（USB CDC 維持） |
| `src/` | [HEPTA-SAT-Library](https://github.com/HEPTA-SAT-TRAINING/HEPTA-SAT-Library) サブモジュール |

## セットアップ

```bash
git clone --recurse-submodules https://github.com/HEPTA-SAT-TRAINING/Lab99_Check_Kit.git
cd Lab99_Check_Kit
```

既に clone 済みなら:

```bash
git submodule update --init --recursive
```

Arduino IDE で `Lab99_Check_Kit.ino` を開き、HEPTA-SAT Full（RP2040）向けに書き込んでください。

## 使い方

1. USB でキットを接続し、Serial Monitor を **9600 baud** で開く
2. 対向 XBee を PC に接続している場合は、そちらでも進捗（`From Sat:`）が見える
3. コマンド 1 文字を送り、Enter（改行）する

### コマンド

| コマンド | 内容 |
| --- | --- |
| `a` | すべて実行（XBee 受信テストは最後） |
| `l` | 基板 LED（ピン 25 / 29 / 24）点滅 |
| `e` | EPS（電圧） |
| `i` | 検流計（ISOL / IBUS / ICHG）。太陽電池に光を当てて ISOL の変化を確認 |
| `t` | 温度 |
| `m` | IMU |
| `s` | SD 読み書き |
| `c` | カメラ撮影（SD へ JPEG） |
| `g` | GPS NMEA センテンス有無（FIX 不要） |
| `n` | XBee AT 識別 |
| `p` | XBee 受信テスト（対話式） |

### 進捗表示の見分け

同じ進捗が 2 経路に出ます。

| 経路 | プレフィックス | 見る場所 |
| --- | --- | --- |
| USB（CDH） | `[CDH]` | Arduino Serial Monitor |
| XBee（COM） | `From Sat:` | PC 側の XBee 受信 |

### XBee 受信テスト (`p`)

1. `p` を送る
2. `[CDH]` / `From Sat:` に「PC から XBee でコマンドを送ってください」と出る
3. 対向 XBee から任意の 1 文字（など）を送る
4. 衛星が受信したら `XBEE_RX: OK`

30 秒以内に届かない場合は NG です。あらかじめ XBee 同士がペア済みであること。

### 検流計 (`i`)

5 秒ほど電流をサンプルします。屋内でも `IBUS` は通常 0 より大きくなります。`ISOL` は太陽電池に光を当てると増えることを目視確認してください。すべて 0 固定なら配線 / MCP3208 / シャント系を疑ってください。

## 空スケッチ

検査後に何もしないファームを残す場合は `empty_sketch/empty_sketch.ino` を書き込んでください。USB は COM ポートのまま残ります。
