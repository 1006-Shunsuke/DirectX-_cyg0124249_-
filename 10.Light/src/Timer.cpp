//-----------------------------------------------------------------------------
// File : Timer.cpp
// Desc : 経過時間の計測。QueryPerformanceCounter を使った高精度タイマー
//-----------------------------------------------------------------------------
#include "Timer.h"

void Timer::Init()
{
    // 周波数を取得する。1秒間に何回カウントするかを表す
    QueryPerformanceFrequency(&m_Frequency);
    // 開始時のカウントを取得
    QueryPerformanceCounter(&m_PrevCount);
}

void Timer::Tick()
{
    // 今のカウントを取得
    LARGE_INTEGER currentCount;
    QueryPerformanceCounter(&currentCount);

    // 前フレームから進んだカウント数を計算
    const LONGLONG elapsedCount = currentCount.QuadPart - m_PrevCount.QuadPart;
    m_PrevCount = currentCount;

    // カウント数 ÷ 周波数 で秒単位に変換
    const double deltaSeconds = static_cast<double>(elapsedCount) / static_cast<double>(m_Frequency.QuadPart);

    // 経過時間を更新
    m_DeltaSeconds = static_cast<float>(deltaSeconds);
    m_TotalSeconds += deltaSeconds;
}
