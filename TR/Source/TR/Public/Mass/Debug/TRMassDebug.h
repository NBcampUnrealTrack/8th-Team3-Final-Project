// TRMassDebug.h

#pragma once

#include "CoreMinimal.h"

#include "Stats/Stats.h"

TR_API DECLARE_LOG_CATEGORY_EXTERN(LogTRMass, Log, All);

/**
 * 프로세서별 비용 측정용 스탯 그룹. 콘솔에서 `stat TRMass`로 본다.
 *
 * "Mass가 느리다"는 진단은 쓸모가 없다. 어느 프로세서가 얼마를 먹는지 갈라야
 * 고칠 대상이 정해진다. 각 프로세서 Execute에 SCOPE_CYCLE_COUNTER를 건다.
 * Unreal Insights용 트레이스 이벤트도 같이 남긴다.
 */
DECLARE_STATS_GROUP(TEXT("TR Mass"), STATGROUP_TRMass, STATCAT_Advanced);

/**
 * TR의 Mass 관련 디버그 스위치를 한곳에 모은다.
 *
 * 콘솔 변수를 각 프로세서 cpp에 흩어놓으면 어떤 명령이 있는지 코드를 뒤져야만 알 수 있다.
 * 명령 목록은 TRMassDebug.cpp 상단 주석에 정리해 두었다.
 *
 * 셰이핑 빌드에서는 전부 상수 false로 접히므로 호출부에 #if를 두지 않아도 된다.
 */
namespace UE::TR::MassDebug
{
#if !UE_BUILD_SHIPPING
	/** tr.Mass.Debug.Draw — 엔티티 위치/방향/목표선 그리기 */
	TR_API bool IsDrawEnabled();

	/** tr.Mass.Debug.DrawTarget — 목표의 충돌 영역(바운드)과 표면 목표점 그리기 */
	TR_API bool IsDrawTargetEnabled();

	/** tr.Mass.Debug.StatusRepeat — 상태를 주기적으로 로그 */
	TR_API bool IsStatusRepeatEnabled();

	/** tr.Mass.Debug.Status — 1회 요청이 걸려 있으면 true를 돌려주고 요청을 소비한다. */
	TR_API bool ConsumeStatusRequest();
#else
	FORCEINLINE bool IsDrawEnabled() { return false; }
	FORCEINLINE bool IsDrawTargetEnabled() { return false; }
	FORCEINLINE bool IsStatusRepeatEnabled() { return false; }
	FORCEINLINE bool ConsumeStatusRequest() { return false; }
#endif
}
