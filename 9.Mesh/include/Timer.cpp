#include "Timer.h"

void Timer::Init()
{
	// 周波数を取得する。1秒間に何回カウントするかを表す
	QueryPerformanceFrequency(&m_Frequency);
	// 開始時のカウントを取得
	QueryPerformanceCounter(&m_preCount);
}

void Timer::Tick()
{
	// 1秒間に何回カウントするかを取得
	LARGE_INTEGER currentCount;
	QueryPerformanceCounter(&currentCount);

	// 経過時間を計算
	const LONGLONG elapsedCount = currentCount.QuadPart - m_preCount.QuadPart;
	m_preCount = currentCount;

	// 周波数を使って秒単位に変換
	double DeltaSeconds = static_cast<double>(elapsedCount) / static_cast<double>(m_Frequency.QuadPart);

	// 経過時間を更新
	m_DeltaSeconds = static_cast<float>(DeltaSeconds);
	m_TotalSeconds += DeltaSeconds;
}