// TRMassNetContract.h
//
// **이 파일은 규칙이지 기능이 아니다.** 아래 표가 "어떤 Mass 상태가 클라이언트에도
// 존재하는가"에 대한 유일한 기준이며, 프래그먼트를 새로 만들거나 프로세서의 넷 모드를
// 바꿀 때 여기부터 고친다.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "MassProcessingTypes.h"

/**
 * ## 왜 이 파일이 있는가
 *
 * 이 프로젝트의 적은 서버만 시뮬레이션하고, 클라이언트는 ATRMassNetRelay 가 내려주는
 * 스냅샷으로 **같은 모양의 엔티티를 흉내 낸다**. 프래그먼트는 트레이트가 만들기 때문에
 * 클라이언트에도 전부 존재하지만, 값을 채우는 프로세서가 서버 전용이면 그 값은
 * **스폰 시 기본값에서 영원히 변하지 않는다.**
 *
 * 이 실패는 다음 성질을 전부 갖는다.
 *
 *  - 컴파일된다. 프래그먼트가 실제로 있으므로 쿼리도 매칭된다.
 *  - 에러도 경고도 없다. 그냥 기본값을 읽는다.
 *  - **호스트에서 혼자 돌리면 절대 재현되지 않는다.** 리슨 서버의 호스트는 서버라서
 *    모든 값이 정상이다. 원격 클라이언트를 붙여야만 갈린다.
 *
 * 실제로 이 유형으로 세 번 잡혔다 — 적 손상 표현이 호스트에서만 보였고, 레이더의 시체
 * 필터가 클라이언트에서 통과만 했고, 계측 프로세서가 클라이언트에서 거짓 수치를 찍었다.
 * 세 번 다 코드는 "맞아 보였다".
 *
 * ## 상태별 권한 표
 *
 * | 상태 | 클라이언트에서 | 근거 |
 * |---|---|---|
 * | `FTRChaserTag`            | ✅ 같음    | 트레이트가 붙인다. 양쪽 동일 |
 * | `FTRMassNetIdFragment`    | ✅ 복제됨  | 서버가 부여, 릴레이가 채운다 |
 * | `FTREnemyStateFragment`   | ✅ 복제됨  | `ActionState` 가 스냅샷에 실린다 |
 * | `FTREnemyVisualFragment`  | ✅ 복제됨  | `DamageRatio` 가 스냅샷에 실린다 |
 * | `FTRChaseChannelFragment` | ✅ 같음    | 스폰 시 결정, 이후 안 변한다 |
 * | `FTREnemyCombatFragment`  | ❌ **기본값 고정** | 체력을 깎는 곳이 전부 서버 전용 |
 * | `FTRTargetFragment`       | ❌ **기본값 고정** | 조종·표적 프로세서가 Server\|Standalone |
 * | `FTRGroundFragment`       | ❌ **기본값 고정** | 지형 추종 프로세서가 Server\|Standalone |
 * | `FTRApproachFragment`     | ⚠️ **T1 기준값** | 티어 배수는 스포너(서버)가 넣는다 |
 * | `FTRArrivedTag`           | ❌ **안 붙는다** | 조종 프로세서가 Server\|Standalone |
 * | `FTRDeadTag`              | ❌ **안 붙는다** | 전투 서브시스템·StateTree 둘 다 서버 |
 * | `FTRDetonatingTag`        | ❌ **안 붙는다** | StateTree = Standalone\|Server (엔진이 박아 둠) |
 * | `FTRLaunchedTag`          | ❌ **안 붙는다** | 위와 같음 |
 *
 * ## 규칙
 *
 *  1. ❌ 줄의 상태를 클라이언트에서도 도는 코드가 읽으면 안 된다. 아래
 *     `AddServerAuthoredRequirement` 를 쓰면 시작 시 ensure 로 잡힌다.
 *  2. 클라이언트에서 그 정보가 필요하면 **스냅샷에 실어야 한다**. 추가 지점은
 *     `FTRMassAgentSnapshot` 한 곳이며, 수집·적용 함수가 구조체에 붙어 있다.
 *  3. 태그로 거르던 것을 클라이언트에서도 걸러야 하면 **행동 상태로 바꾼다**
 *     (`ActionState == Death` 등). 상태는 복제되고 태그는 안 된다. 덤으로 태그의
 *     한 프레임 지연도 없어진다.
 */
namespace UE::TR::Mass
{
	/**
	 * 이 프로세서가 원격 클라이언트에서도 도는가.
	 *
	 * Standalone 은 세지 않는다 — 싱글 플레이는 자기가 서버라 모든 값이 정상이다.
	 * 문제가 되는 것은 Client 플래그뿐이며, AllNetModes 는 그걸 포함한다.
	 */
	inline bool RunsOnClient(const UMassProcessor& Processor)
	{
		return EnumHasAnyFlags(Processor.GetExecutionFlags(), EProcessorExecutionFlags::Client);
	}

	/**
	 * 서버에서만 채워지는 프래그먼트를 쿼리에 요구한다.
	 *
	 * 하는 일은 `AddRequirement` 와 같고, **클라이언트에서도 도는 프로세서면 시작 시
	 * ensure 로 잡는 것**이 존재 이유다. 위 표의 ❌ 줄을 읽을 때는 이걸 쓴다.
	 *
	 * Presence 를 Optional 로 줘도 봐주지 않는다. 값이 기본값으로 고정된다는 사실은
	 * 프래그먼트가 있느냐 없느냐와 무관하고, Optional 은 오히려 "없으면 넘어간다"는
	 * 인상을 줘서 문제를 가린다.
	 */
	template<typename TFragment>
	void AddServerAuthoredRequirement(FMassEntityQuery& Query, const UMassProcessor& Processor,
		const EMassFragmentAccess Access, const EMassFragmentPresence Presence = EMassFragmentPresence::All)
	{
		ensureMsgf(!RunsOnClient(Processor),
			TEXT("[%s] 서버에서만 채워지는 프래그먼트를 요구하는데 이 프로세서는 클라이언트에서도 돈다. ")
			TEXT("클라이언트에서는 스폰 기본값을 읽게 되며 에러 없이 조용히 틀린다. ")
			TEXT("넷 모드를 Server|Standalone 으로 좁히거나, 필요한 값을 FTRMassAgentSnapshot 에 실을 것. ")
			TEXT("(TRMassNetContract.h 참고)"),
			*Processor.GetClass()->GetName());

		Query.AddRequirement<TFragment>(Access, Presence);
	}

	/**
	 * 서버에서만 붙는 태그로 거른다.
	 *
	 * 태그는 프래그먼트보다 더 조용히 실패한다. `Presence::All` 이면 클라이언트에서
	 * **아무것도 매칭되지 않아** 프로세서가 통째로 놀고, `Presence::None` 이면
	 * **전부 통과해** 필터가 없는 것과 같아진다. 어느 쪽이든 로그 한 줄 남지 않는다.
	 */
	template<typename TTag>
	void AddServerAuthoredTagRequirement(FMassEntityQuery& Query, const UMassProcessor& Processor,
		const EMassFragmentPresence Presence)
	{
		ensureMsgf(!RunsOnClient(Processor),
			TEXT("[%s] 서버에서만 붙는 태그로 거르는데 이 프로세서는 클라이언트에서도 돈다. ")
			TEXT("클라이언트에서는 필터가 전부 통과하거나(None) 아무것도 안 잡힌다(All). ")
			TEXT("행동 상태(ActionState) 로 거르는 쪽을 검토할 것. (TRMassNetContract.h 참고)"),
			*Processor.GetClass()->GetName());

		Query.AddTagRequirement<TTag>(Presence);
	}
}
