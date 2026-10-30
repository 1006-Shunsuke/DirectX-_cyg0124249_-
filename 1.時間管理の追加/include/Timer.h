#pragma once
#include <windows.h>

class Timer {
public:
	void Init();
	void Tick();//フレーム更新
	float GetDeltaTime() const { return m_DeltaSeconds; }//前フレームからの経過時間を取得
	float GetTotalTime() const { return m_TotalSeconds; }//アプリケーション開始からの経過時間を取得
private:
	LARGE_INTEGER m_Frequency{};//周波数
	LARGE_INTEGER m_preCount{};//前フレームのカウント
	float m_DeltaSeconds = 0.0f;//前回からの経過時間
	double m_TotalSeconds = 0.0f;//起動からの経過時間
};