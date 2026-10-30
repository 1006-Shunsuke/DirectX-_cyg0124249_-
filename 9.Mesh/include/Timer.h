//-----------------------------------------------------------------------------
// File : Timer.h
// Desc : 経過時間の計測。QueryPerformanceCounter を使った高精度タイマー
//-----------------------------------------------------------------------------
#pragma once

#include <Windows.h>

///////////////////////////////////////////////////////////////////////////////
// Timer class
///////////////////////////////////////////////////////////////////////////////
class Timer
{
public:
    void Init();    // 計測を開始する
    void Tick();    // フレームごとに呼び、経過時間を更新する

    float  GetDeltaTime() const { return m_DeltaSeconds; }   // 前フレームからの経過時間。単位は秒
    double GetTotalTime() const { return m_TotalSeconds; }   // 計測開始からの経過時間。単位は秒

private:
    LARGE_INTEGER m_Frequency{};            // 周波数：1秒間に何回カウントするか
    LARGE_INTEGER m_PrevCount{};            // 前フレームのカウント
    float         m_DeltaSeconds = 0.0f;    // 前フレームからの経過時間。単位は秒
    double        m_TotalSeconds = 0.0;     // 計測開始からの経過時間。単位は秒で、長時間でも精度が落ちないよう double にする
};
