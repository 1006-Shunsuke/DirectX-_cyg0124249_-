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
        return true;

    switch (msg)
    {
    case WM_DESTROY:
    { PostQuitMessage(0); }
    break;
    default:
    {}
    break;
    }
    return DefWindowProc(hWnd, msg, wp, lp);
}

//------------------------------------------------------------------------------------
//                            描画　処理
//------------------------------------------------------------------------------------
void App::Render()
{
    //コマンドの記録の開始
    //使い終わったメモリの再利用準備
    m_pCommandAllocator[m_FrameIndex]->Reset();
    m_pCommandList->Reset(m_pCommandAllocator[m_FrameIndex].Get(), nullptr);
    // リソースバリアの設定：表示用から描画用に切り替える
    // 例
    //バックバッファに三角形を描き込む処理A
    //Present()で表示エンジンがそのバックバッファを画面に表示する処理B
    //その処理Aと処理Bが同時に行われないためにリソースバリアは必要
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;//使用用途の遷移
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_pColorBuffer[m_FrameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    //リソースバリアの生成
    m_pCommandList->ResourceBarrier(1, &barrier);

    //レンダーターゲットの設定
    m_pCommandList->OMSetRenderTargets(1, &m_HandleRTV[m_FrameIndex], FALSE, &m_HandleDSV);
    m_pCommandList->ClearDepthStencilView(m_HandleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    //クリアカラーの設定
    const float value = 0.5f + 0.5f * sinf(m_Timer.GetTotalTime());
    float clearColor[] = { value,value,value,value };

    // レンダーターゲットビューをクリアする。色はInspectorで編集した値を使う
    m_pCommandList->ClearRenderTargetView(m_HandleRTV[m_FrameIndex], m_ClearColor, 0, nullptr);

    //ワールド行列を、エディタで編集した値から組み立てる
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
    m_pTransform[m_FrameIndex]->World = S * R * T;

    //描画処理
    {
        //ポリゴン描画用の処理
        m_pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());

        // 先に「どのヒープを使うか」を設定する。GPUハンドルはこのヒープ内の位置として読まれるため
        ID3D12DescriptorHeap* heaps[] = { m_pHeapCBV_SRV_UAV.Get() };
        m_pCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

        // [0] b0 : 今のフレームの CBV。ヒープの m_FrameIndex 番目
        auto handleCBV = m_pHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart();
        handleCBV.ptr += m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * m_FrameIndex;
        m_pCommandList->SetGraphicsRootDescriptorTable(0, handleCBV);

        // [2] t0 : テクスチャの SRV。ヒープの FrameCount 番目
        m_pCommandList->SetGraphicsRootDescriptorTable(1, m_HandleGPU_SRV);

        m_pCommandList->SetPipelineState(m_pPSO.Get());

        m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pCommandList->IASetVertexBuffers(0, 1, &m_VBV);
        m_pCommandList->IASetIndexBuffer(&m_IBV);
        m_pCommandList->RSSetViewports(1, &m_Viewport);
        m_pCommandList->RSSetScissorRects(1, &m_Scissor);

        m_pCommandList->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);
    }

    DrawImGui();

    // リソースバリアの設定：描画用から表示用に戻す
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_pColorBuffer[m_FrameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    //リソースバリア
    m_pCommandList->ResourceBarrier(1, &barrier);

    //コマンドの記録を終了
    m_pCommandList->Close();

    //コマンドを実行
    ID3D12CommandList* ppCommandLists[] = { m_pCommandList.Get() };
    m_pQueue->ExecuteCommandLists(1, ppCommandLists);

    //画面に表示
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
    ImGui_ImplDX12_NewFrame();// ImGui用のリソース作成
    ImGui_ImplWin32_NewFrame();// ウィンドウの現在のサイズ、経過時間、マウスカーソルの位置などをImGuiに渡す
    ImGui::NewFrame();// ImGui本体のフレーム開始

    // 初回だけウィンドウの位置とサイズを決める。以降はユーザーが動かした位置を保つ
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 180), ImGuiCond_FirstUseEver);

    ImGui::Begin("Inspector");// Inspectorウィンドウへ記録の開始
    ImGui::DragFloat3("Position", &m_Position.x, 0.01f);// 位置用のfloat3つ分の入力欄を作る
    ImGui::DragFloat3("Rotation", &m_RotationDeg.x, 1.0f);// 回転用のfloat3つ分の入力欄を作る
    ImGui::DragFloat3("Scale", &m_Scale.x, 0.01f, 0.01f, 100.0f);// 大きさ用のfloat3つ分の入力欄を作る
    ImGui::Checkbox("Auto Rotate", &m_AutoRotate);// boolの切り替えを行うことができる
    ImGui::ColorEdit4("Clear Color", m_ClearColor);// カラーピッカーで色を変更できる。配列名がそのまま先頭のポインタになる
    ImGui::End();// 記録の終了

    ImGui::Render();//ここまでに組み立てたUIを描画データに変換し確定させる

    // ImGuiがテクスチャを参照するためのヒープを設定
    ID3D12DescriptorHeap* desc[1] = { m_pHeapImGui.Get() };// m_pHeapImGuiはディスクリプタヒープのポイント
    m_pCommandList->SetDescriptorHeaps(_countof(desc), desc);// コマンドリストにこのディスクリプタヒープを使うよという設定
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCommandList.Get());// ImGuiの描画命令を記録
}