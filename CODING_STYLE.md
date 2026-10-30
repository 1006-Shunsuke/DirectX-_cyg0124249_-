# コーディング規約

このリポジトリのコードは、以下の規約に従って書いています。
DirectX 12 まわりの変数名は、Microsoft 公式サンプル（[DirectX-Graphics-Samples](https://github.com/microsoft/DirectX-Graphics-Samples)）の付け方にそろえています。

## 1. 命名規則

| 対象 | 規則 | 例 |
|------|------|----|
| クラス・構造体・関数 | PascalCase | `App`、`LightParam`、`InitD3D()` |
| メンバー変数 | `m_` + PascalCase | `m_Width`、`m_FrameIndex` |
| ポインタ・ComPtr のメンバー | `m_p` + PascalCase | `m_pDevice`、`m_pCommandList` |
| ローカル変数・引数 | camelCase | `vbSize`、`texturePath` |
| ローカルのポインタ | `p` + PascalCase | `pVSBlob`、`pScene` |
| ポインタの配列（API に渡すもの） | `pp` + PascalCase | `ppHeaps`、`ppCommandLists` |
| 定数 | PascalCase | `FrameCount` |
| 関数名 | 省略しない | `InitWnd()`（`Init()` に縮めない） |

## 2. DirectX 12 の設定用構造体の名前

「**何の（用途）** + **型を表す接尾辞**」で名前を付けます。
`desc` だけ・`heap` だけのように、何の設定か分からない名前は使いません。
迷ったときは、**型名の中にある言葉を使います**（例：`D3D12_DESCRIPTOR_HEAP_DESC` → `rtvHeapDesc`、`D3D12_RENDER_TARGET_VIEW_DESC` → `rtvDesc`）。

| 型 | 変数名 |
|----|--------|
| `D3D12_COMMAND_QUEUE_DESC` | `queueDesc` |
| `DXGI_SWAP_CHAIN_DESC` | `swapChainDesc` |
| `D3D12_DESCRIPTOR_HEAP_DESC` | `rtvHeapDesc` / `dsvHeapDesc` / `cbvSrvHeapDesc` |
| `D3D12_HEAP_PROPERTIES` | `heapProps` |
| `D3D12_RESOURCE_DESC` | `resourceDesc`（深度バッファは `depthDesc`） |
| `D3D12_CLEAR_VALUE` | `depthClearValue` |
| `D3D12_RENDER_TARGET_VIEW_DESC` | `rtvDesc` |
| `D3D12_DEPTH_STENCIL_VIEW_DESC` | `dsvDesc` |
| `D3D12_CONSTANT_BUFFER_VIEW_DESC` | `cbvDesc` |
| `D3D12_SHADER_RESOURCE_VIEW_DESC` | `srvDesc` |
| `D3D12_DESCRIPTOR_RANGE` | `cbvRange` / `srvRange` |
| `D3D12_ROOT_PARAMETER`（配列） | `rootParameters` |
| `D3D12_ROOT_SIGNATURE_DESC` | `rootSignatureDesc` |
| `D3D12_RASTERIZER_DESC` | `rasterizerDesc` |
| `D3D12_BLEND_DESC` | `blendDesc` |
| `D3D12_DEPTH_STENCIL_DESC` | `depthStencilDesc` |
| `D3D12_GRAPHICS_PIPELINE_STATE_DESC` | `psoDesc` |
| ディスクリプタ1個分のサイズ | `rtvDescriptorSize` / `cbvSrvDescriptorSize` |

> **間違えやすい点**：`D3D12_HEAP_PROPERTIES` は「どの種類のメモリに置くか」という**設定**であり、ヒープそのものではありません。
> ヒープは `CreateCommittedResource` の中でリソースと一緒に作られるため、変数名は `heap` ではなく `heapProps` にします。

## 3. 書式

書式と文字コードは、ルートの `.editorconfig` に設定してあり、Visual Studio が保存時に自動で従います。

- インデントはスペース4つ（タブは使わない）
- `{` は次の行に置く
- `if` は本体が1行でも必ず `{ }` を付け、複数行で書く

  ```cpp
  // ○
  if (FAILED(hr))
  {
      return false;
  }

  // ×
  if (FAILED(hr))return false;
  if (FAILED(hr)) { return false; }
  ```
- ただし、初期化関数を順番に呼ぶだけの `OnInit` のような「目次」になる関数では、一覧で見やすくするため `if (!Func()) { return false; }` を1行で書いてよい
- 自作のヘッダは `#include "App.h"`、標準・外部ライブラリは `#include <vector>` と書き分ける

## 4. 文字コード

| ファイル | 文字コード | 理由 |
|----------|------------|------|
| `.h` / `.cpp` | UTF-8（BOM 付き） | BOM が無いと、Visual Studio が Shift-JIS として読んでしまうため |
| `.hlsl` / `.hlsli` | UTF-8（BOM 無し） | シェーダコンパイラ（fxc）は BOM を読めず、`error X3000: Illegal character` になるため |
| `.md` | UTF-8（BOM 無し） | GitHub での表示用 |

## 5. コメント

- `//` のあとに空白を1つ入れる（`// コメント`）。コードの後ろに書くときも、`//` の前に空白を入れる
- 日本語で、「何をしているか」だけでなく「なぜそうするか」を書く
- 処理のまとまりには `// 1.` `// 1-1` のように番号を振る
- 「追加」「変更」などの変更履歴はコメントに書かない（履歴は Git のコミットで残す）
