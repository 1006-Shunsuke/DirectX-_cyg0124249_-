//-----------------------------------------------------------------------------
// File : App_Loop.cpp
// Desc : メインループと毎フレームの処理(Run・MainLoop・WndProc・Render・Present)
//-----------------------------------------------------------------------------
#include "App.h"
#include <cmath>

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
    m_pCommandList->ClearDepthStencilView(m_HandleDSV,D3D12_CLEAR_FLAG_DEPTH,1.0f,0,0,nullptr);

    // クリアカラーの設定
    const float value = 0.5f + 0.5f * sinf(static_cast<float>(m_Timer.GetTotalTime()));
    float clearColor[] = { value,value,value,value };

    // レンダーターゲットビューをクリア
    m_pCommandList->ClearRenderTargetView(m_HandleRTV[m_FrameIndex], clearColor, 0, nullptr);


    // パラメータの更新
    // m_MoveParam.OffsetX = sinf(static_cast<float>(m_Timer.GetTotalTime())) * 1.0f;

    // ワールド行列を毎フレーム書き換える処理
    const float t = static_cast<float>(m_Timer.GetTotalTime());
    XMMATRIX rot1 = XMMatrixRotationY(t); // 回転処理
    XMMATRIX rot2 = XMMatrixRotationZ(t);
    XMMATRIX rot3 = XMMatrixRotationX(t);
    m_pTransform[m_FrameIndex]->World = rot1 * rot2 * rot3;

    // 描画処理
    {
        // ポリゴン描画用の処理
        m_pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
        m_pCommandList->SetGraphicsRootConstantBufferView(0, m_pCB[m_FrameIndex]->GetGPUVirtualAddress());
        m_pCommandList->SetGraphicsRoot32BitConstants(1, sizeof(MoveParam) / 4, &m_MoveParam, 0);
        m_pCommandList->SetPipelineState(m_pPSO.Get());

        m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pCommandList->IASetVertexBuffers(0, 1, &m_VBV);
        m_pCommandList->IASetIndexBuffer(&m_IBV);
        m_pCommandList->RSSetViewports(1, &m_Viewport);
        m_pCommandList->RSSetScissorRects(1, &m_Scissor);

        m_pCommandList->DrawIndexedInstanced(m_IndexCount, 1, 0,0,0);
    }
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
