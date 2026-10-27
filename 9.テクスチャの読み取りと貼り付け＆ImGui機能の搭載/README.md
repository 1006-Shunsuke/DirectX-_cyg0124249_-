# 009 テクスチャの読み取りと貼り付け ＋ ImGuiの実装

![実行画面1](<docs/images/スクリーンショット 2026-10-27 194107.png>)
![実行画面2](<docs/images/スクリーンショット 2026-10-27 194255.png>)
![実行画面3](<docs/images/スクリーンショット 2026-10-27 194433.png>)

## 概要
- 008（立方体の描画）をベースに、次の2つを追加した。
  1. **テクスチャ**：頂点色の代わりに、DDS画像を立方体の各面へ貼り付けた。
  2. **ImGui**：「Inspector」ウィンドウを作り、実行中に立方体の位置・回転・拡大率と背景色を変えられるようにした。
- あわせて、`App.cpp` を役割ごとに4ファイルへ分け、経過時間を測る `Timer` クラスを追加した。

## 手法
### テクスチャ
- 画像の読み込みとGPUへの転送には DirectXTK12（NuGet: directxtk12_desktop_2019）を使用。
- 画像は `res/texture/SampleTexture.dds`（256×256、ミップマップ9段、BC7 sRGB形式）。
- 画像の流れ：DDS → Upload Heap → Default Heapのテクスチャ（`m_pTexture`）→ SRV（ヒープの2番目）→ ルートパラメータ1 → PSの `t0`
- 「どこを読むか」は頂点のUV、「どう読むか」はサンプラー `s0` が決める。

### ImGui
- Dear ImGui（1.93.1 WIP）の本体と、`imgui_impl_win32`（入力・ウィンドウ）と `imgui_impl_dx12`（描画）を組み合わせて使用。
- ImGuiのファイルは `imgui/` フォルダにまとめて置いた（バックエンドも同じ階層）。
- ImGui専用のSRVヒープ（`m_pHeapImGui`）を立方体用とは**別に**作った。
- 毎フレーム、立方体を描いた後に、ヒープをImGui用に切り替えてImGuiを描く。

### ディスクリプタヒープの中身
| ヒープ | 番号 | 中身 | シェーダーでの名前 |
|---|---|---|---|
| `m_pHeapCBV_SRV_UAV`（3枠） | [0] | CBV：フレーム0用 `Transform` | `b0` |
| | [1] | CBV：フレーム1用 `Transform` | `b0` |
| | [2] | SRV：`m_pTexture` | `t0` |
| `m_pHeapImGui`（64枠） | [0]〜 | ImGuiが使うSRV（フォントなど） | ImGui内部 |

## やったこと（008からの変更点）
### A. ファイル構成の整理
- `App.cpp` を次の4つに分けた。
  - `App_Init.cpp`：コンストラクタ・`InitApp`・`InitWnd`・`InitD3D`・`InitImGui`
  - `App_OnInit.cpp`：描画に使うGPUリソースの作成（`OnInit`）
  - `App_Loop.cpp`：メインループ・`WndProc`・`Render`・`Present`・`DrawImGui`
  - `App_Term.cpp`：終了処理（`TermApp`・`OnTerm`・`TermD3D`・`TermImGui` など）
- `Timer` クラス（`Timer.h` / `Timer.cpp`）を追加。`QueryPerformanceCounter` で1フレームの経過時間（`GetDeltaTime`）と起動からの時間（`GetTotalTime`）を測る。

### B. テクスチャ（`App_OnInit.cpp`）
1. **頂点データを色からUVへ**
   - `Vertex` の色を `XMFLOAT2 TexCoord` に変更。
   - 各面の4頂点（外から見て左上・右上・右下・左下）に (0,0)・(1,0)・(1,1)・(0,1) を割り当てた。
2. **入力レイアウトの変更**（5.2）
   - `"TEXCOORD"` / `DXGI_FORMAT_R32G32_FLOAT`
3. **ディスクリプタヒープを作り直す**（2.1）
   - `CBV_SRV_UAV` ヒープを `SHADER_VISIBLE` で作成。
   - `NumDescriptors = FrameCount + 1`（CBV×2 ＋ SRV×1）。
   - 定数バッファのCBVもこのヒープの [0]・[1] に置いた（2.5-2）。
4. **テクスチャの読み込み**（2.6）
   - 2.6-1 `SearchFilePath` で `res\texture\SampleTexture.dds` を探す（作業フォルダ → exeのフォルダ → その親…と最大5階層）。
   - 2.6-2 `CreateDDSTextureFromFileEx`（`DDS_LOADER_FORCE_SRGB`）＋ `ResourceUploadBatch` でGPUへ転送し、`finished.wait()` で完了を待つ。
   - `SetName(L"SampleTexture")` でPIXで見つけやすくした。
5. **SRVの作成**（2.7）
   - フォーマットとミップ段数は `m_pTexture->GetDesc()` からそのまま取る。
   - ヒープの [2]（`incSize * FrameCount` ずらした位置）にSRVを作る。CPUハンドルは作成用、GPUハンドルは描画用。
6. **ルートシグネチャ**（3）
   - `params[0]`：ディスクリプタテーブル（CBV 1個、`b0`、VSのみ）
   - `params[1]`：ディスクリプタテーブル（SRV 1個、`t0`、PSのみ）
   - 静的サンプラー（`s0`）：`MIN_MAG_MIP_LINEAR`、アドレスモード `CLAMP`
   - `NumParameters = 2`、`NumStaticSamplers = 1`
7. **シェーダーの切り替え**（5.6・5.7）
   - `SimpleVS/PS.cso` → `SimpleTexVS/PS.cso`
   - `SimpleTexVS.hlsl`：`World`・`View`・`Proj` を順にかけて、UVはそのままPSへ渡す。
   - `SimpleTexPS.hlsl`：`ColorMap.Sample(ColorSmp, input.TexCoord)` で色を取得。
8. **描画処理**（`App_Loop.cpp` の `Render`）
   - `SetDescriptorHeaps` → `SetGraphicsRootDescriptorTable(0, そのフレームのCBV)` → `SetGraphicsRootDescriptorTable(1, m_HandleGPU_SRV)`
9. **終了処理**（`OnTerm`）
   - `m_pTexture`・`m_pHeapCBV_SRV_UAV` を `Reset()` し、SRVハンドルを空に戻す。
10. **COMの初期化**
    - `InitApp` で `CoInitializeEx`、`TermApp` で `CoUninitialize`。

### C. ImGui
1. **プロジェクトへの追加**
   - `imgui.cpp` / `imgui_draw.cpp` / `imgui_tables.cpp` / `imgui_widgets.cpp` / `imgui_demo.cpp` / `imgui_impl_win32.cpp` / `imgui_impl_dx12.cpp`
   - インクルードディレクトリに `$(ProjectDir)..\imgui` を追加。
2. **初期化**（`App_Init.cpp` の `InitImGui`。`InitD3D` の後、`OnInit` の前に呼ぶ）
   - ImGui専用のSRVヒープを `SHADER_VISIBLE`・64枠で作成（1.92以降はフォント以外にもSRVを使うので余裕を持たせた）。
   - `ImGui::CreateContext()` → `ImGui::StyleColorsDark()` → `ImGui_ImplWin32_Init(m_hWnd)`
   - `ImGui_ImplDX12_InitInfo` に次を設定して `ImGui_ImplDX12_Init(&info)`
     - `Device`・`CommandQueue`・`NumFramesInFlight = FrameCount`
     - `RTVFormat = R8G8B8A8_UNORM_SRGB`、`DSVFormat = D32_FLOAT`（立方体のPSOと同じ）
     - `SrvDescriptorHeap = m_pHeapImGui`
     - `SrvDescriptorAllocFn`：ヒープの空き枠を先頭から順に1つずつ渡すラムダ（`m_ImGuiHeapNext` を進める）
     - `SrvDescriptorFreeFn`：何もしない
     - `UserData = this`（ラムダの中から `App` のメンバーを使うため）
3. **入力をImGuiへ渡す**（`WndProc`）
   - 先頭で `ImGui_ImplWin32_WndProcHandler` を呼び、ImGuiが処理したら `true` を返す。宣言は `extern` で自分で書いた。
4. **毎フレームの処理**（`DrawImGui`。`Render` の中で、立方体の描画後・`PRESENT` へのバリアの前に呼ぶ）
   - `ImGui_ImplDX12_NewFrame()` → `ImGui_ImplWin32_NewFrame()` → `ImGui::NewFrame()`
   - 「Inspector」ウィンドウ（初回だけ位置(10,10)・サイズ300×180）に次を置いた。

     | 項目 | ウィジェット | 変える変数 |
     |---|---|---|
     | Position | `DragFloat3` | `m_Position` |
     | Rotation | `DragFloat3`（度） | `m_RotationDeg` |
     | Scale | `DragFloat3`（0.01〜100） | `m_Scale` |
     | Auto Rotate | `Checkbox` | `m_AutoRotate` |
     | Clear Color | `ColorEdit4` | `m_ClearColor` |

   - `ImGui::Render()` → `SetDescriptorHeaps(m_pHeapImGui)` → `ImGui_ImplDX12_RenderDrawData(...)`
5. **Inspectorの値を描画に反映**（`Render`）
   - `Auto Rotate` がONなら、Y軸回転を1秒に60度ずつ進める（`m_Timer.GetDeltaTime()` を使用）。
   - ワールド行列を `拡大(S) × 回転(R) × 移動(T)` で組み立て、定数バッファに書く。
   - 背景のクリア色に `m_ClearColor` を使う。
6. **終了処理**（`App_Term.cpp`）
   - `TermApp` の順番：`WaitGpu` → `TermImGui` → `OnTerm` → `TermD3D` → `TermWnd` → `CoUninitialize`
   - `TermImGui`：コンテキストが無ければ（初期化の途中で失敗したら）何もしない。あれば `ImGui_ImplDX12_Shutdown` → `ImGui_ImplWin32_Shutdown` → `ImGui::DestroyContext` → ヒープを `Reset()`

## 学んだこと
### テクスチャ
- SRVは画像そのものではなく「この画像をどう読むか」の説明書き。画像本体はリソース（`m_pTexture`）。
- `params[1]` だから `register(t1)` になるわけではない。レジスタ番号は `BaseShaderRegister` で決まる。
- GPUハンドルは「セットされたヒープ内の位置」なので、`SetDescriptorHeaps` を先に呼ぶ必要がある。
- RTVやDSVのヒープはシェーダーから見えなくてよいが、CBV・SRVのヒープは `SHADER_VISIBLE` が必要。
- CBVとSRVは1枠のサイズが同じなので、同じ `CBV_SRV_UAV` ヒープに並べられる。
- 静的サンプラーはルートシグネチャに埋め込むので、サンプラー用ヒープが要らない。
- 画像を sRGB として読み、RTVも `_SRGB` なので、色が二重に補正されない。
- `ResourceUploadBatch` が Upload→Default のコピーとリソースバリアを内部で行っている（自前実装は今後の課題）。

### ImGui
- ImGuiは自前のルートシグネチャ・PSOを持っているので、立方体用の設定とは関係なく描画される。
- `SetDescriptorHeaps` でセットできる `CBV_SRV_UAV` ヒープは1つだけ。なので立方体を描いた後、ImGuiを描く前にヒープを切り替えている。
- ImGuiの描画も、同じコマンドリストに積む描画命令の1つ。だから順番（立方体 → ImGui → `PRESENT` へのバリア）が大事。
- Win32バックエンドに入力を渡さないと、表示はされてもクリックやドラッグが効かない。
- 終了時は、GPUがImGuiのリソースを使い終わるのを待ってから、デバイスより先にImGuiを片付ける。

## 詰まったこと
- `SimpleTexVS.hlsl` のセマンティクスを `POSITIONT` と誤記していた → `POSITION` に修正。
- `packoffset(b4)` と書いていた → 定数バッファ内のオフセットは `c` 指定なので `packoffset(c4)` に修正。

## 今後の課題
- `ResourceUploadBatch` を使わず、Upload→Default のコピーを自前で書く。
- 面ごとに違う画像を貼る（サイコロなど）。
- `Render` に残っている、使っていない `clearColor`（sin波で明滅させていた頃の名残）を消す。
- コード中のコメントのずれを直す（`Render` の「[2] t0」は実際はルートパラメータ1、ルートシグネチャの「パラメータの数(3)」は実際は2）。

## 環境
- Windows 11 / Visual Studio 2022 / DirectX 12
- DirectXTK12（NuGet: directxtk12_desktop_2019 2025.10.28.1）
- Dear ImGui 1.93.1 WIP
