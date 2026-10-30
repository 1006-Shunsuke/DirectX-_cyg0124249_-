//-----------------------------------------------------------------------------
// File : App_Term.cpp
// Desc : 終了処理(TermApp・TermWnd・WaitGpu・TermD3D・OnTerm)
//-----------------------------------------------------------------------------
#include "App.h"

void App::TermApp()
{
    WaitGpu();      // GPU の処理がすべて終わるまで待つ(以降の終了処理はこの1回を前提にする)
    OnTerm();
    TermD3D();
    TermWnd();
}

void App::TermWnd()
{
    if (m_hInst != nullptr)
    {
        UnregisterClass(ClassName, m_hInst);
    }
    m_hInst = nullptr;
    m_hWnd = nullptr;

}

//-------------------------------------------------------------------
//                  GPUの処理完了を待つ
//-------------------------------------------------------------------
void App::WaitGpu()
{
    if (m_pQueue == nullptr || m_pFence == nullptr || m_FenceEvent == nullptr)
    {
        return;
    }

    // シグナル処理

    m_pQueue->Signal(m_pFence.Get(), m_FenceCounter[m_FrameIndex]);

    // 完了時にイベント設定
    m_pFence->SetEventOnCompletion(m_FenceCounter[m_FrameIndex], m_FenceEvent);

    // 待機処理
    WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);

    // カウンターを増やす
    m_FenceCounter[m_FrameIndex]++;
}

//-------------------------------------------------------------------
//                  Direct3Dの終了処理
//-------------------------------------------------------------------
// 作成したオブジェクトを一つずつ開放
void App::TermD3D()
{
    // イベント破棄
    if (m_FenceEvent != nullptr)
    {
        CloseHandle(m_FenceEvent);
        m_FenceEvent = nullptr;
    }

    // 6.フェンス破棄
    m_pFence.Reset();

    // 5.RTV破棄
    m_pHeapRTV.Reset();
    for (auto i = 0u; i < FrameCount; ++i)
    {
        m_pColorBuffer[i].Reset();
    }

    // 4.2.コマンドリスト破棄
    m_pCommandList.Reset();


    // 4.1.コマンドアロケータ破棄
    for (auto i = 0u; i < FrameCount; ++i)
    {
        m_pCommandAllocator[i].Reset();
    }

    // 3.スワップチェイン破棄
    m_pSwapChain.Reset();

    // 2.コマンドキュー破棄
    m_pQueue.Reset();

    // 1.デバイス破棄
    m_pDevice.Reset();

}

//-----------------------------------------------------------------------------
//      終了時の処理
//-----------------------------------------------------------------------------
void App::OnTerm()
{
    // Depthの初期化
    m_pDepthBuffer.Reset();
    m_pHeapDSV.Reset();
    m_HandleDSV = {};

    for (auto i = 0u; i < FrameCount; ++i)
    {
        if (m_pCB[i].Get() != nullptr)
        {
            m_pCB[i]->Unmap(0, nullptr);
            m_pTransform[i] = nullptr;
        }
        m_pCB[i].Reset();
    }
    m_pVB.Reset();
    m_pIB.Reset();
    m_pPSO.Reset();
    m_pRootSignature.Reset();
}
