# 干潟図鑑 — Steam 版（UE5）

現実の潮汐と連動する干潟を探索し、生き物を観察・採集して図鑑を完成させるゲームの Steam 版。Unreal Engine 5 で作る。

- Web 版・スマホ版（Three.js）: [Gerupamasini/gerupamasini](https://github.com/Gerupamasini/gerupamasini)。開発はいったん止めていて、このリポジトリに移植する計算の正解として使う
- 決定と計画: [docs/10_engine_decision.md](docs/10_engine_decision.md)（エンジン選定）、[docs/11_ue5_port_plan.md](docs/11_ue5_port_plan.md)（移植計画）、[docs/09_steam_readiness_audit.md](docs/09_steam_readiness_audit.md)（Web 版を調べた障害の一覧）

## 準備（Windows）

1. **Unreal Engine 5.8**（5.8.3 で開発）を Epic Games Launcher から入れる
2. **Visual Studio**（2022 を最新に更新、または 2026）を入れる。インストーラーで「C++ によるゲーム開発」「C++ によるデスクトップ開発」、「Unreal Engine インストーラー」、Windows 11 SDK、**MSVC Build Tools v14.50**（UE 5.8 は 14.44 以前を受け付けない）を選ぶ
3. **Git LFS** を入れて `git lfs install` を一度実行する。`.uasset` / `.umap` / `.glb` などは LFS に入る
4. このリポジトリを clone する
5. **`BuildEditor.bat`** をダブルクリックする（GitHub から更新してビルド。結果は `build-log.txt` にも残る）
6. `SUCCESS` と出たら `HigataZukan.uproject` を開く（初回はシェーダーの準備に時間がかかる）

エディターが起動中に落ちるときは **`OpenEditorDX11.bat`** で DirectX 11 で開いてみる（DirectX 12・レイトレーシング・GPU ドライバーの問題かどうかの切り分け）。

GPU は DX12 対応のもの（目安は RTX 20 番台以降）。

## 構成

| 場所 | 内容 |
|---|---|
| `Source/HigataZukan/` | ゲーム本体のモジュール |
| `Source/HigataTide/` | 潮汐。`TideCore/` は UE のヘッダーを使わない C++（Web 版の `src/tide/` の移植）、`UTideSubsystem` はブループリントとゲームから使う窓口 |
| `Content/Data/` | ゲームデータ（種・行動ツリー・地図・潮汐観測点・文言の JSON）。Web 版の `public/data/` から引き継いだもの |
| `Source/HigataZukan/` の `HigataFlat` / `HigataMap` | 地図データから地形・海・太陽を作る |
| `Source/HigataZukan/` の `HigataWalker` | 歩くプレイヤー。Enhanced Input をコードで組み立てる（キーボード・マウス・ゲームパッド） |
| `tests/tide/` | 潮汐と太陽位置（`TideCore` / `SunCore`）のゴールデンテスト。UE なしでビルドでき、CI で動く |
| `tools/convert_map_png.py` | Web 版の地形 PNG を UE で読む raw ファイルに変換する |
| `tools/unreal/make_materials.py` | 材質を作る（BuildEditor.bat の 4 段目） |
| `Config/` | 描画（Lumen・Nanite・仮想シャドウマップ・HW レイトレーシング）、Enhanced Input、パッケージ設定 |

## 潮汐の使い方

```cpp
UTideSubsystem* Tide = GetGameInstance()->GetSubsystem<UTideSubsystem>();
Tide->LoadStation(TEXT("jma_yokosuka"));                       // Content/Data/tide/stations/jma_yokosuka.json
const double LevelM = Tide->GetLevelAt(FDateTime::UtcNow());   // 平均水面からの高さ（m）
TArray<FTideExtremum> Today = Tide->GetExtrema(FromUtc, ToUtc);
```

ブループリントからも同じ名前で呼べる（カテゴリ「Tide」）。

## 遊んで確かめる

エディターでどのレベルを開いていても、▶（プレイ）を押すと **走水の干潟** が現れる（`UHigataStartSubsystem` が `AHigataFlat` を置く。テンプレートの床や山は隠れる）。

- 地形: `Content/Data/maps/hashirimizu.json` と `height.u16`（Web 版の `height.png` を `tools/convert_map_png.py` で変換したもの）
- 海面: 横須賀の観測点の今の潮位で上下する
- 太陽: 走水の緯度経度と今の時刻の位置（夜は暗い）
- 歩く: 目の高さ 1.6 m で干潟を歩く（`AHigataWalker`）。水が深いほど遅くなる（胴長は 1 m まで）
- 操作（キーボード / ゲームパッド）: 移動 WASD / 左スティック、視点 マウス / 右スティック、走る Shift / 左スティック押し込み、飛ぶ切り替え F / Y（飛行中の上下 E・Q / 右・左トリガー）
- 時刻: **T** で 1 時間進める、**G** で 1 時間戻す、**R** で今に戻す
- 地図の外: 縁の起伏をそのまま 1 km 先まで延ばした粗い地面（西は陸、東は海底）

- 見た目: 地面は底質ごとの色（砂・泥まじりの砂・泥・礫・澪筋）。直近の満潮でかぶった所は暗くつやが出て、潮が引くと 1 時間に 5 cm ずつ乾く。海は Single Layer Water（浅い所は底が透ける）

材質（マテリアル）は `tools/unreal/make_materials.py` がコードから作る。`BuildEditor.bat` がビルドの後に実行し、`Content/Generated/` に書き出す（git には入れない）。材質が無いときは単色で動く。

## テスト

```bash
cmake -S tests/tide -B build/tide && cmake --build build/tide && ctest --test-dir build/tide --output-on-failure
```

フィクスチャ（`tests/tide/fixtures/`）は Web 版の TypeScript で作ったもので、固定してある。C++ の潮汐を直したらこのテストで確かめる。計算式を意図して変えるときだけ、フィクスチャも作り直す。
