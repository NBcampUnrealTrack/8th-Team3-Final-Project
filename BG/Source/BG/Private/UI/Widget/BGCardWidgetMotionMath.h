// UBGCardWidget 의 모션 계산에 쓰는 순수 함수들.
//
// 원래 BGCardWidget.cpp 의 익명 네임스페이스에 있었다. 그 파일을 책임별로 쪼개면서
// 두 개 이상의 번역 단위가 같은 함수를 쓰게 되어 여기로 옮겼다.
//
// ⚠️ Private 전용 헤더다. 위젯 바깥에서 include 하지 말 것 — 여기 있는 것은 카드 연출의
//    내부 사정이고, 공개하면 튜닝 곡선을 바꿀 때마다 바깥까지 살펴야 한다.
// ⚠️ 상태를 두지 않는다. 전부 인자만 보고 값을 돌려주는 함수여야 이 분리가 유지된다.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"

namespace BGCardMotion
{
	/** 목표를 살짝 지나쳤다 되돌아오는 감속(ease-out-back). Overshoot 이 0 이면 평범한 감속. */
	inline float EaseOutBack(float Alpha, float Overshoot)
	{
		const float A = Alpha - 1.f;
		return 1.f + (Overshoot + 1.f) * A * A * A + Overshoot * A * A;
	}

	/** 시작→목표의 수직 방향(궤적을 활처럼 휘게 한다). 아래로 처지면 떨어지는 것처럼 보이므로 위로 고정. */
	inline FVector2D UpwardPerpendicular(const FVector2D& Delta)
	{
		FVector2D Perpendicular(-Delta.Y, Delta.X);
		if (Perpendicular.Y > 0.f)
		{
			Perpendicular = -Perpendicular;   // 화면 좌표는 아래로 갈수록 Y 가 크다
		}

		//	출발점과 목표가 거의 같으면 방향을 정할 수 없다. 그냥 위로 둔다.
		return Perpendicular.Normalize() ? Perpendicular : FVector2D(0.f, -1.f);
	}

	/** 고정 시간 연출의 진행도 0~1. 튜닝값 0 은 "그 단계를 건너뛴다" 라 실제로 일어난다. */
	inline float Progress(float Elapsed, float Duration)
	{
		return FMath::Clamp(Elapsed / FMath::Max(Duration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	}

	/** 커브가 있으면 그것이 모양을 정한다. 없어도 동작해야 튜닝을 나중으로 미룰 수 있다. */
	inline float CurveOr(const UCurveFloat* Curve, float Alpha, float Fallback)
	{
		return Curve ? Curve->GetFloatValue(Alpha) : Fallback;
	}

	/**
	 * -1~+1 입력에 응답 곡선을 먹인다. 부호는 두고 크기만 굽힌다.
	 * 양끝(±1)에서는 항상 ±1 이라 Max 값과 곡선을 따로 만질 수 있다.
	 */
	inline float ShapeResponse(float Value, float Exponent)
	{
		if (FMath::IsNearlyEqual(Exponent, 1.f))
		{
			return Value;   // 선형. Pow 를 태울 이유가 없다.
		}

		return FMath::Sign(Value)
			* FMath::Pow(FMath::Abs(Value), FMath::Max(Exponent, KINDA_SMALL_NUMBER));
	}
}
