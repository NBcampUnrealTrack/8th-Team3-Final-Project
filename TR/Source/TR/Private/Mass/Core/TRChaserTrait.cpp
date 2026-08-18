// TRChaserTrait.cpp

#include "Mass/Core/TRChaserTrait.h"

#include "Engine/World.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassRepresentationFragments.h"
#include "Mass/Debug/TRMassDebug.h"

namespace
{
	/**
	 * 애님 파라미터의 값 제약을 검사한다.
	 *
	 * 전부 어겨도 컴파일도 실행도 되고, 화면만 조용히 어색해진다. 그런 종류가 제일 찾기
	 * 어려우므로 템플릿을 만들 때 한 번 짚어준다(엔티티마다가 아니라 종류마다 한 번이다).
	 */
	void ValidateAnimParams(const FTREnemyAnimParams& Anim, const FTREnemyCombatParams& Combat, const UObject* Owner)
	{
		const FString Name = GetNameSafe(Owner);

		// 루프 위상은 Duration을 법으로 접어서 구한다. 0이면 접을 길이가 없어 전원이
		// 같은 지점에서 시작하고, 무리가 한 몸처럼 같은 발을 딛는다.
		if (Anim.Idle.Duration <= 0.f)
		{
			UE_LOG(LogTRMass, Warning, TEXT("[%s] Idle.Duration이 0이다. 대기 루프의 개체별 위상이 죽는다."), *Name);
		}
		if (Anim.Move.Duration <= 0.f)
		{
			UE_LOG(LogTRMass, Warning, TEXT("[%s] Move.Duration이 0이다. 이동 루프의 개체별 위상이 죽는다."), *Name);
		}

		// 종류마다 실제로 쓰는 단발이 다르다. 안 쓰는 칸까지 경고하면 로그가 무의미해진다.
		switch (Combat.EnemyType)
		{
		case ETREnemyType::Melee:
			// Duration이 0이면 "끝나지 않는 단발"이 되어 로코모션으로 영영 돌아오지 않는다.
			if (Anim.Attack.Duration <= 0.f)
			{
				UE_LOG(LogTRMass, Warning, TEXT("[%s] Attack.Duration이 0이다. 공격 포즈에서 못 빠져나온다."), *Name);
			}
			// 애니메이션이 준비 시간보다 짧으면 공격 → 대기 → 폭발 순서가 되어 어색하다.
			else if (Anim.Attack.Duration < Combat.DetonationWindup)
			{
				UE_LOG(LogTRMass, Warning,
					TEXT("[%s] Attack.Duration(%.2f)이 DetonationWindup(%.2f)보다 짧다. 폭발 전에 대기 모션으로 되돌아간다."),
					*Name, Anim.Attack.Duration, Combat.DetonationWindup);
			}
			break;

		case ETREnemyType::Launcher:
			if (Anim.Launch.Duration <= 0.f)
			{
				UE_LOG(LogTRMass, Warning, TEXT("[%s] Launch.Duration이 0이다. 사출 포즈에서 못 빠져나온다."), *Name);
			}
			break;

		case ETREnemyType::Shooter:
			if (Anim.Fire.Duration <= 0.f)
			{
				UE_LOG(LogTRMass, Warning, TEXT("[%s] Fire.Duration이 0이다. 사격 포즈에서 못 빠져나온다."), *Name);
			}
			// 원거리 표현은 시퀀스 인덱스가 바뀔 때만 트랙을 다시 설정한다. 발 사이에
			// 로코모션으로 되돌아가는 틈이 없으면 인덱스가 계속 사격에 머물러,
			// 둘째 발부터 애니메이션이 재시작되지 않는다(액터 표현은 정상이다).
			else if (Anim.Fire.Duration >= Combat.FireInterval)
			{
				UE_LOG(LogTRMass, Warning,
					TEXT("[%s] Fire.Duration(%.2f)이 FireInterval(%.2f) 이상이다. ISM 표현에서 둘째 발부터 사격 모션이 안 나온다."),
					*Name, Anim.Fire.Duration, Combat.FireInterval);
			}
			break;

		default:
			break;
		}
	}
}

void UTRChaserTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	ValidateAnimParams(AnimParams, CombatParams, GetOuter());

	BuildContext.RequireFragment<FTransformFragment>();

	// 반대로 반지름은 UMassMovementTrait이 요구만 하고 만들지는 않는다.
	// 엔진 기본 제공자(디버그 시각화/네비메시/캡슐 동기화 트레이트)는 모두 불필요한 기능을
	// 함께 끌고 오므로, 추적 엔티티의 기본 속성으로 보고 여기서 채워준다.
	BuildContext.AddFragment_GetRef<FAgentRadiusFragment>().Radius = AgentRadius;

	// 엔티티↔액터 연결 고리. 표현 트레이트가 요구하지만 엔진에 이걸 제공하는 트레이트가 없다
	// (에디터에서 MassAssortedFragmentsTrait으로 수동 추가하는 방법도 있으나, 빠뜨리기 쉬워 여기서 보장한다).
	BuildContext.AddFragment<FMassActorFragment>();

	// 인스턴스 스킨드 메시의 애니메이션 상태.
	// UMassConsumeInstancedSkinnedMeshAnimationProcessor가 이걸 필수로 요구하는데,
	// 없으면 T포즈로 고정된다
	BuildContext.AddFragment<FMassRepresentationAnimationFragment>();

	BuildContext.AddFragment<FTRTargetFragment>();
	BuildContext.AddFragment<FTREnemyStateFragment>();
	BuildContext.AddFragment<FTRGroundFragment>();
	BuildContext.AddTag<FTRChaserTag>();

	// 복제 식별자. 서버는 스폰 시 채우고, 클라이언트는 릴레이가 넣어준다.
	// 두 월드가 같은 적을 가리키는 유일한 수단이라 트레이트가 항상 보장한다.
	BuildContext.AddFragment<FTRMassNetIdFragment>();

	// 종류에서 파생되는 값들. 스폰 시 스포너가 티어 배수를 곱해 다시 채우지만,
	// 스포너를 거치지 않고 만들어진 엔티티(레벨에 놓은 AMassSpawner 등)도
	// 그대로 동작해야 하므로 기준(T1) 수치를 여기서 넣어둔다.
	UE::TR::Enemy::InitCombat(CombatParams, /*StatScale=*/1.f, BuildContext.AddFragment_GetRef<FTREnemyCombatFragment>());
	BuildContext.AddFragment_GetRef<FTRApproachFragment>() = UE::TR::Enemy::MakeApproach(CombatParams);

	// 채널은 엔티티마다 다를 수 있으므로 일반 프래그먼트로 두고, 초기값을 여기서 박아둔다.
	FTRChaseChannelFragment& ChannelFragment = BuildContext.AddFragment_GetRef<FTRChaseChannelFragment>();
	ChannelFragment.Channel = Channel;

	// 파라미터는 값이 같으면 자동으로 하나의 공유 인스턴스로 합쳐진다.
	const FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(ChaseParams);
	BuildContext.AddConstSharedFragment(ParamsFragment);

	const FConstSharedStruct CombatParamsFragment = EntityManager.GetOrCreateConstSharedFragment(CombatParams);
	BuildContext.AddConstSharedFragment(CombatParamsFragment);

	// 상태 → 시퀀스 인덱스 매핑. UTRMassAnimSelectProcessor가 이걸 보고 애니메이션을 고른다.
	const FConstSharedStruct AnimParamsFragment = EntityManager.GetOrCreateConstSharedFragment(AnimParams);
	BuildContext.AddConstSharedFragment(AnimParamsFragment);
}
