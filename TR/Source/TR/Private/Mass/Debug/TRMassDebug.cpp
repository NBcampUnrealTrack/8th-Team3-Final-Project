// TRMassDebug.cpp
//
// ─────────────────────────────────────────────────────────────────────────────
//  TR Mass 디버그 명령 목록
// ─────────────────────────────────────────────────────────────────────────────
//  tr.Mass.Debug.Draw 0|1        추적 엔티티를 디버그 드로우한다.
//                                빨강=목표 없음 / 노랑=추적 중 / 초록=도달
//                                캡슐(위치), 화살표(바라보는 방향), 회색 선(목표까지)
//
//  tr.Mass.Debug.DrawTarget 0|1  추적 목표의 충돌 영역(기차 바운드)을 그린다.
//                                청록 박스 = 적이 목표로 삼는 표면
//                                주황 점   = 각 적이 향하는 표면 최근접점
//                                엔티티가 하나도 없어도 박스는 그려진다.
//
//  tr.Mass.Debug.Status          현재 상태를 로그에 1회 출력한다.
//                                엔티티 수, 표현/액터 프래그먼트 유무, LOD 분포,
//                                표현 타입 분포, 살아있는 액터 수
//
//  tr.Mass.Debug.StatusRepeat 0|1  위 내용을 1초마다 반복 출력한다.
//
//  tr.Mass.Target.Padding <cm>   충돌 영역을 임시로 늘리거나(양수) 줄인다(음수).
//                                플레이 중 감을 잡는 용도. **영구 설정이 아니다** —
//                                크기는 기차의 TargetVolume 컴포넌트에서 정한다.
//
//  ── 엔진 쪽 명령 (같이 쓰면 유용) ───────────────────────────────────────────
//  mass.debug.Representation 1     표현 타입을 색으로 표시
//  mass.debug.RepresentationLOD 1  LOD를 색으로 표시
//  stat Mass                       Mass 처리 비용
// ─────────────────────────────────────────────────────────────────────────────

#include "Mass/Debug/TRMassDebug.h"

DEFINE_LOG_CATEGORY(LogTRMass);

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"

namespace UE::TR::MassDebug
{
	namespace Private
	{
		int32 GDraw = 0;
		int32 GDrawTarget = 0;
		int32 GStatusRepeat = 0;

		bool bStatusRequested = false;

		FAutoConsoleVariableRef CVarDraw(
			TEXT("tr.Mass.Debug.Draw"),
			GDraw,
			TEXT("추적 엔티티의 위치/방향/목표를 그린다. 빨강=목표없음, 노랑=추적중, 초록=도달"),
			ECVF_Cheat);

		// 엔티티 드로우와 분리해 둔다. 100마리 캡슐이 겹쳐 있으면 정작 봐야 할
		// 충돌 영역 경계가 안 보인다.
		FAutoConsoleVariableRef CVarDrawTarget(
			TEXT("tr.Mass.Debug.DrawTarget"),
			GDrawTarget,
			TEXT("추적 목표의 충돌 영역(기차 바운드)과 각 적의 표면 목표점을 그린다."),
			ECVF_Cheat);

		FAutoConsoleVariableRef CVarStatusRepeat(
			TEXT("tr.Mass.Debug.StatusRepeat"),
			GStatusRepeat,
			TEXT("추적 엔티티의 LOD/표현 상태를 1초마다 로그로 출력한다."),
			ECVF_Cheat);

		FAutoConsoleCommand CmdStatus(
			TEXT("tr.Mass.Debug.Status"),
			TEXT("추적 엔티티의 LOD/표현 상태를 로그에 1회 출력한다."),
			FConsoleCommandDelegate::CreateLambda([]
			{
				bStatusRequested = true;
			}),
			ECVF_Cheat);
	}

	bool IsDrawEnabled()
	{
		return Private::GDraw != 0;
	}

	bool IsDrawTargetEnabled()
	{
		return Private::GDrawTarget != 0;
	}

	bool IsStatusRepeatEnabled()
	{
		return Private::GStatusRepeat != 0;
	}

	bool ConsumeStatusRequest()
	{
		const bool bRequested = Private::bStatusRequested;
		Private::bStatusRequested = false;
		return bRequested;
	}
}

#endif // !UE_BUILD_SHIPPING
