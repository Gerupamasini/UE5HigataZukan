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
| `tests/tide/` | `TideCore` のゴールデンテスト。UE なしでビルドでき、CI で動く |
| `Config/` | 描画（Lumen・Nanite・仮想シャドウマップ・HW レイトレーシング）、Enhanced Input、パッケージ設定 |

## 潮汐の使い方

```cpp
UTideSubsystem* Tide = GetGameInstance()->GetSubsystem<UTideSubsystem>();
Tide->LoadStation(TEXT("jma_yokosuka"));                       // Content/Data/tide/stations/jma_yokosuka.json
const double LevelM = Tide->GetLevelAt(FDateTime::UtcNow());   // 平均水面からの高さ（m）
TArray<FTideExtremum> Today = Tide->GetExtrema(FromUtc, ToUtc);
```

ブループリントからも同じ名前で呼べる（カテゴリ「Tide」）。

**エディターでの動作確認:** レベルブループリントで `Event BeginPlay` → `Get Game Instance Subsystem`（TideSubsystem）→ `Load Station`（`jma_tokyo`）→ `Get Level At`（`Utc Now`）→ `Print String` とつないでプレイする。今の東京の潮位（m）が表示されれば成功。

## テスト

```bash
cmake -S tests/tide -B build/tide && cmake --build build/tide && ctest --test-dir build/tide --output-on-failure
```

フィクスチャ（`tests/tide/fixtures/`）は Web 版の TypeScript で作ったもので、固定してある。C++ の潮汐を直したらこのテストで確かめる。計算式を意図して変えるときだけ、フィクスチャも作り直す。
