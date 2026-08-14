// TRChaserTrait.cpp

#include "Mass/Core/TRChaserTrait.h"

#include "Engine/World.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassRepresentationFragments.h"

void UTRChaserTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	
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
