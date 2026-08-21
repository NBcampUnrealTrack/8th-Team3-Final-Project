// TRRepairTargetComponent.cpp


#include "Train/TRRepairTargetComponent.h"

#include "Debug/TRGameplayLog.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Interaction/TRActionComponent.h"
#include "Net/UnrealNetwork.h"
#include "Train/TRInstallablePart.h"
#include "Train/TRTrain.h"

UTRRepairTargetComponent::UTRRepairTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTRRepairTargetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTRRepairTargetComponent, CycleStartServerTime);
	DOREPLIFETIME(UTRRepairTargetComponent, CycleDuration);
}

float UTRRepairTargetComponent::GetCycleProgress01() const
{
	if (CycleDuration <= 0.f)
	{
		return 0.f;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	if (!IsValid(GameState))
	{
		return 0.f;
	}

	const float Elapsed = GameState->GetServerWorldTimeSeconds() - CycleStartServerTime;
	return FMath::Clamp(Elapsed / CycleDuration, 0.f, 1.f);
}

void UTRRepairTargetComponent::RefreshCycle(int32 WorkerCount)
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	// 완료 처리 중에는 무시한다.
	if (bCompletingCycle)
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	
	const float NewDuration = ResolveCycleDuration(WorkerCount);
	
	if (NewDuration <= 0.f)
	{
		World->GetTimerManager().ClearTimer(CycleTimerHandle);
		CycleDuration = 0.f;
		CycleStartServerTime = 0.f;
		return;
	}
	
	const float Elapsed01 = GetCycleProgress01();
	
	if (FMath::IsNearlyEqual(NewDuration, CycleDuration))
	{
		return;
	}
	
	const float Now = World->GetTimeSeconds();
	CycleStartServerTime = Now - NewDuration * Elapsed01;
	CycleDuration = NewDuration;
	
	const float Remaining = FMath::Max(NewDuration * (1.f - Elapsed01), KINDA_SMALL_NUMBER);
	World->GetTimerManager().SetTimer(
		CycleTimerHandle,
		this,
		&UTRRepairTargetComponent::HandleCycleComplete,
		Remaining,
		false
	);
}

float UTRRepairTargetComponent::ResolveCycleDuration(int32 WorkerCount) const
{
	if (WorkerCount <= 0 || BaseCycleDuration <= 0.f)
	{
		return 0.f;
	}
	
	if (ConcurrentCycleSpeedup.IsEmpty())
	{
		return BaseCycleDuration;
	}
	
	const int32 Index = FMath::Clamp(WorkerCount -1, 0, ConcurrentCycleSpeedup.Num() - 1);
	const float Multiplier = ConcurrentCycleSpeedup[Index];
	
	if (Multiplier <= 0.f)
	{
		UE_LOG(LogTRGameplay, Warning, TEXT("%s: ConcurrentSpeedMultipliers[%d] is %.2f — must be positive. Using base duration."),
			*GetNameSafe(GetOwner()), Index, Multiplier);
		
		return BaseCycleDuration;
	}
	
	return BaseCycleDuration / Multiplier;
}

void UTRRepairTargetComponent::HandleCycleComplete()
{
	// 사이클을 먼저 닫는다. RefreshCycle 은 시간이 같으면 조기 반환하므로,
	// CycleDuration 이 남아 있으면 아래에서 다음 사이클 타이머가 걸리지 않는다.
	CycleDuration = 0.f;
	CycleStartServerTime = 0.f;

	const UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!IsValid(World) || !IsValid(Owner))
	{
		return;
	}

	bCompletingCycle = true;

	TArray<UTRActionComponent*> Workers;
	UTRActionComponent::GatherRepairWorkers(World, Owner, Workers);

	// 각자 자기 회복량과 물자를 쓴다. 3명이 붙어 있으면 3인분이 들어간다.
	// 물자가 바닥난 사람은 false 를 돌려주므로 여기서 떼어낼 목록에 넣는다.
	TArray<UTRActionComponent*> Detach;
	for (UTRActionComponent* Worker : Workers)
	{
		if (!Worker->CommitRepairContribution())
		{
			Detach.Add(Worker);
		}
	}

	// 다 고쳐졌으면 전원 떼어낸다.
	if (!IsOwnerRepairable())
	{
		Detach = Workers;
	}

	// 떼는 일은 목록을 다 돌린 뒤에 한다. 루프 도중에 하면 순회 중 상태가 바뀐다.
	for (UTRActionComponent* Worker : Detach)
	{
		Worker->ServerCancelAction();
	}

	bCompletingCycle = false;

	// 남은 인원으로 다음 사이클. 아무도 없으면 RefreshCycle(0) 이 리셋한다.
	TArray<UTRActionComponent*> Remaining;
	UTRActionComponent::GatherRepairWorkers(World, Owner, Remaining);
	RefreshCycle(Remaining.Num());
}

bool UTRRepairTargetComponent::IsOwnerRepairable() const
{
	if (const ATRInstallablePart* Part = Cast<ATRInstallablePart>(GetOwner()))
	{
		return Part->CanBeRepaired();
	}

	if (const ATRTrain* Train = Cast<ATRTrain>(GetOwner()))
	{
		return Train->CanBeRepaired();
	}

	return false;
}

void UTRRepairTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CycleTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}
