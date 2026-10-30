//-----------------------------------------------------------------------------
// File : App_Loop.cpp
// Desc : メインループと毎フレームの処理。Run・MainLoop・WndProc・Render・Present
//-----------------------------------------------------------------------------
#include "App.h"
#include <cmath>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace DirectX;

void App::Run()
{
    if (InitApp())
    {
        MainLoop();
    }
    TermApp();
}

void App::MainLoop()
{
    MSG msg = {};

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) == TRUE)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            m_Timer.Tick();
            Render();
        }
    }
}

// ウィンドウプロシージャ
LRESULT App::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wp, lp))
    {
        return true;
    }

    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        break;
    }
    return DefWindowProc(hWnd, msg, wp, lp);
}

//------------------------------------------------------------------------------------
//                            描画　処理
//------------------------------------------------------------------------------------
void App::Render()
{
    // コマンドの記録の開始
    // 使い終わったメモリの再利用準備
    m_pCommandAllocator[m_FrameIndex]->Reset();
    m_pCommandList->Reset(m_pCommandAllocator[m_FrameIndex].Get(), nullptr);
    // リソースバリアの設定：表示用から描画用に切り替える
    // 例
    // バックバッファに三角形を描き込む処理A
    // Present()で表示エンジンがそのバックバッファを画面に表示する処理B
    // その処理Aと処理Bが同時に行われないためにリソースバリアは必要
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 使用用途の遷移
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_pColorBuffer[m_FrameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    // リソースバリアの生成
    m_pCommandList->ResourceBarrier(1, &barrier);

    // レンダーターゲットの設定
    m_pCommandList->OMSetRenderTargets(1, &m_HandleRTV[m_FrameIndex], FALSE, &m_HandleDSV);
    m_pCommandList->ClearDepthStencilView(m_HandleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // レンダーターゲットビューをクリアする。色はInspectorで編集した値を使う
    m_pCommandList->ClearRenderTargetView(m_HandleRTV[m_FrameIndex], m_ClearColor, 0, nullptr);

    // ワールド行列を、エディタで編集した値から組み立てる
    if (m_AutoRotate)
    {
        m_RotationDeg.y += 60.0f * m_Timer.GetDeltaTime();          // 1秒に60度
        m_RotationDeg.y = fmodf(m_RotationDeg.y, 360.0f);          // 値が増え続けないように
    }
    XMMATRIX S = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(m_RotationDeg.x),
        XMConvertToRadians(m_RotationDeg.y),
        XMConvertToRadians(m_RotationDeg.z));
    XMMATRIX T = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    XMMATRIX world = S * R * T;                                     // 下で逆行列にも使うので変数に入れる
    m_pTransform[m_FrameIndex]->World = world;

    // 法線用の行列 = ワールド行列の逆行列の転置
    // 拡大率が XYZ で違うと、ワールド行列そのままでは法線が面に垂直でなくなるため
    m_pTransform[m_FrameIndex]->WorldInvTrans = XMMatrixTranspose(XMMatrixInverse(nullptr, world));

    // ライトの値を定数バッファに書く。Inspector で変えた値が毎フレーム反映される
    {
        auto pLight = m_pLight[m_FrameIndex];

        // 向きは長さ1にしてから渡す。全成分が0で長さ0だと計算できないので、真上からの光にする
        auto dir = XMLoadFloat3(&m_LightDir);
        if (XMVectorGetX(XMVector3LengthSq(dir)) < 1e-6f)
        {
            dir = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
        }
        XMStoreFloat3(&pLight->LightDir, XMVector3Normalize(dir));

        pLight->LightIntensity    = m_LightIntensity;
        pLight->LightColor        = XMFLOAT3(m_LightColor[0], m_LightColor[1], m_LightColor[2]);
        pLight->Shininess         = m_Shininess;
        pLight->CameraPos         = m_CameraPos;
        pLight->SpecularIntensity = m_SpecularIntensity;
        pLight->AmbientColor      = XMFLOAT3(m_AmbientColor[0], m_AmbientColor[1], m_AmbientColor[2]);
        pLight->Padding           = 0.0f;
    }

    // 描画処理
    {
        // ポリゴン描画用の処理
        m_pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());

        // 先に「どのヒープを使うか」を設定する。GPUハンドルはこのヒープ内の位置として読まれるため
        ID3D12DescriptorHeap* ppHeaps[] = { m_pHeapCBV_SRV_UAV.Get() };
        m_pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        // [0] b0 : 今のフレームの CBV。ヒープの m_FrameIndex 番目
        auto cbvHandle = m_pHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart();
        cbvHandle.ptr += m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * m_FrameIndex;
        m_pCommandList->SetGraphicsRootDescriptorTable(RootParam_Transform, cbvHandle);

        // [1] t0 : テクスチャの SRV。ヒープの FrameCount 番目
        m_pCommandList->SetGraphicsRootDescriptorTable(RootParam_Texture, m_HandleGPU_SRV);

        // [2] b1 : ライトの定数バッファ。ルートCBVなので GPU アドレスを直接渡す
        m_pCommandList->SetGraphicsRootConstantBufferView(RootParam_Light, m_pLightCB[m_FrameIndex]->GetGPUVirtualAddress());

        m_pCommandList->SetPipelineState(m_pPSO.Get());

        m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pCommandList->IASetVertexBuffers(0, 1, &m_VBV);
        m_pCommandList->IASetIndexBuffer(&m_IBV);
        m_pCommandList->RSSetViewports(1, &m_Viewport);
        m_pCommandList->RSSetScissorRects(1, &m_Scissor);

        // メッシュごとに描く。頂点・インデックスは1本のバッファにまとめてあるので、範囲だけ変える
        for (const auto& range : m_DrawRanges)
        {
            m_pCommandList->DrawIndexedInstanced(
                range.IndexCount,   // 描くインデックスの数
                1,                  // インスタンス数は1
                range.StartIndex,   // インデックスバッファの何番目から読むか
                range.BaseVertex,   // 読んだインデックスに足す値
                0);                 // インスタンスの開始番号
        }
    }

    DrawImGui();

    // リソースバリアの設定：描画用から表示用に戻す
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_pColorBuffer[m_FrameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    // リソースバリア
    m_pCommandList->ResourceBarrier(1, &barrier);

    // コマンドの記録を終了
    m_pCommandList->Close();

    // コマンドを実行
    ID3D12CommandList* ppCommandLists[] = { m_pCommandList.Get() };
    m_pQueue->ExecuteCommandLists(1, ppCommandLists);

    // 画面に表示
    Present(1);
}

//------------------------------------------------------------------
//              画面に表示し、次のフレームの準備
//------------------------------------------------------------------
void App::Present(uint32_t interval)
{
    // 画面に表示
    m_pSwapChain->Present(interval, 0);

    // シグナル処理
    const auto currentValue = m_FenceCounter[m_FrameIndex];
    m_pQueue->Signal(m_pFence.Get(), currentValue);

    // バックバッファ番号を更新
    m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

    // 次のフレームの描画準備がまだであれば待機する
    if (m_pFence->GetCompletedValue() < m_FenceCounter[m_FrameIndex])
    {
        m_pFence->SetEventOnCompletion(m_FenceCounter[m_FrameIndex], m_FenceEvent);
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    // 次のフレームのフェンスカウンターを増やす
    m_FenceCounter[m_FrameIndex] = currentValue + 1;
}

void App::DrawImGui()
{
    ImGui_ImplDX12_NewFrame(); // ImGui用のリソース作成
    ImGui_ImplWin32_NewFrame(); // ウィンドウの現在のサイズ、経過時間、マウスカーソルの位置などをImGuiに渡す
    ImGui::NewFrame(); // ImGui本体のフレーム開始

    // 初回だけウィンドウの位置とサイズを決める。以降はユーザーが動かした位置を保つ
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 380), ImGuiCond_FirstUseEver);   // ライトの項目まで収まる大きさ

    ImGui::Begin("Inspector"); // Inspectorウィンドウへ記録の開始
    ImGui::DragFloat3("Position", &m_Position.x, 0.01f); // 位置用のfloat3つ分の入力欄を作る
    ImGui::DragFloat3("Rotation", &m_RotationDeg.x, 1.0f); // 回転用のfloat3つ分の入力欄を作る
    ImGui::DragFloat3("Scale", &m_Scale.x, 0.01f, 0.01f, 100.0f); // 大きさ用のfloat3つ分の入力欄を作る
    ImGui::Checkbox("Auto Rotate", &m_AutoRotate); // boolの切り替えを行うことができる
    ImGui::ColorEdit4("Clear Color", m_ClearColor); // カラーピッカーで色を変更できる。配列名がそのまま先頭のポインタになる

    // ライトの設定
    ImGui::SeparatorText("Light");                                       // 見出し付きの区切り線
    ImGui::DragFloat3("Direction", &m_LightDir.x, 0.01f, -1.0f, 1.0f);  // 光が進む向き。Render で正規化してから定数バッファに書き込む
    ImGui::ColorEdit3("Light Color", m_LightColor);                     // 光の色
    ImGui::DragFloat("Intensity", &m_LightIntensity, 0.01f, 0.0f, 10.0f); // 光の強さ
    ImGui::ColorEdit3("Ambient", m_AmbientColor);                       // 環境光の色
    ImGui::DragFloat("Specular", &m_SpecularIntensity, 0.01f, 0.0f, 5.0f); // 鏡面反射の強さ。0でハイライトなし
    ImGui::DragFloat("Shininess", &m_Shininess, 0.5f, 1.0f, 256.0f);   // 鏡面反射の鋭さ。1未満は不自然になるので1から
    ImGui::End(); // 記録の終了

    ImGui::Render(); // ここまでに組み立てたUIを描画データに変換し確定させる

    // ImGuiがテクスチャを参照するためのヒープを設定
    ID3D12DescriptorHeap* ppHeaps[1] = { m_pHeapImGui.Get() }; // m_pHeapImGuiはディスクリプタヒープのポイント
    m_pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps); // コマンドリストにこのディスクリプタヒープを使うよという設定
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCommandList.Get()); // ImGuiの描画命令を記録
}