// TRTargetDirectorSubsystem.cpp

#include "Mass/Targeting/TRTargetDirectorSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

namespace
{
	/**
	 * 충돌 영역에 더해지는 전역 여유(cm). 플레이 중 감을 잡기 위한 임시 손잡이다.
	 *
	 * 영구 설정이 아니다 — 크기는 기차의 TargetVolume 컴포넌트에서 정한다.
	 * `tr.Mass.Debug.DrawTarget 1`과 같이 켜면 박스가 실시간으로 늘었다 줄었다 하므로,
	 * 마음에 드는 값을 찾아 볼륨 크기에 반영하는 용도로 쓴다.
	 */
	float GTargetBoundsPadding = 0.f;

	FAutoConsoleVariableRef CVarTargetBoundsPadding(
		TEXT("tr.Mass.Target.Padding"),
		GTargetBoundsPadding,
		TEXT("추적 목표 충돌 영역에 더할 여유(cm). 음수면 줄어든다. 전 채널 공통."),
		ECVF_Cheat);

	/**
	 * 액터의 추적 영역을 로컬 좌표로 잰다.
	 *
	 * **TRMassTargetVolume 태그가 붙은 컴포넌트가 있으면 그것만 쓴다.** 그게 에디터에서
	 * 직접 크기를 조절하는 경로다. 없으면 메시 전체 바운드로 물러난다 — 태그를 붙이는 걸
	 * 잊어도 적이 목표를 잃지는 않게.
	 *
	 * 볼륨을 액터에 대해 회전시켜 두면 축 정렬 상자로 근사된다(실제보다 커진다).
	 * 기차처럼 축에 맞춰 놓는 것이 정확하다.
	 */
	bool MeasureLocalBounds(const AActor& Actor, FBox& OutLocalBounds)
	{
		const FName VolumeTag = UE::TR::Mass::GetTargetVolumeTagName();
		const FTransform ActorTransform = Actor.GetActorTransform();

		for (const UActorComponent* Component : Actor.GetComponents())
		{
			const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
			if (Primitive == nullptr || !Primitive->ComponentHasTag(VolumeTag))
			{
				continue;
			}

			// 액터 기준 상대 트랜스폼으로 바운드를 다시 계산한다.
			// 월드 바운드를 쓰면 등록 당시의 기차 위치가 박제되어, 기차가 움직이는 순간 어긋난다.
			const FTransform RelativeTransform = Primitive->GetComponentTransform().GetRelativeTransform(ActorTransform);
			OutLocalBounds = Primitive->CalcBounds(RelativeTransform).GetBox();

			return OutLocalBounds.IsValid != 0;
		}

		// 콜리전이 없는 컴포넌트(장식 메시 등)까지 포함시킨다. 접근 목표는 시각적 실루엣에
		// 맞는 편이 자연스럽고, 콜리전만 잡으면 차량 일부가 목표에서 빠진다.
		OutLocalBounds = Actor.CalculateComponentsBoundingBoxInLocalSpace(
			/*bNonColliding=*/true, /*bIncludeFromChildActors=*/true);

		return OutLocalBounds.IsValid != 0;
	}
}

void UTRTargetDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Channels.SetNum(ChannelCount);
	Snapshot.SetNum(ChannelCount);
}

TStatId UTRTargetDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTRTargetDirectorSubsystem, STATGROUP_Tickables);
}

void UTRTargetDirectorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshSnapshot();
}

void UTRTargetDirectorSubsystem::RefreshSnapshot()
{
	for (int32 Index = 0; Index < ChannelCount; ++Index)
	{
		const FTRTargetChannelState& State = Channels[Index];
		FTRResolvedTarget& Resolved = Snapshot[Index];

		Resolved = FTRResolvedTarget();

		if (!State.bHasTarget)
		{
			continue;
		}

		if (const AActor* TargetActor = State.TargetActor.Get())
		{
			Resolved.Center = TargetActor->GetActorLocation();
			Resolved.Transform = TargetActor->GetActorTransform();
			Resolved.bHasBounds = State.bHasBounds;
			Resolved.bValid = true;

			if (State.bHasBounds)
			{
				// 콘솔 여유는 저장된 값을 고치지 않고 여기서 얹는다. 그래야 값을 이리저리
				// 바꿔봐도 원래 실측값이 남아 있고, 0으로 되돌리면 그대로 복원된다.
				//
				// 음수 여유가 반크기를 넘으면 박스가 뒤집혀 최근접점이 엉뚱한 곳으로 간다.
				const FVector Center = State.LocalBounds.GetCenter();
				const FVector Extent = (State.LocalBounds.GetExtent() + FVector(GTargetBoundsPadding))
					.ComponentMax(FVector::ZeroVector);

				Resolved.LocalBounds = FBox(Center - Extent, Center + Extent);
			}

			continue;
		}

		// 액터 목표였는데 파괴된 경우 — 고정 위치 목표와 구분해야 하므로 여기서 걸러낸다.
		if (State.TargetActor.IsExplicitlyNull())
		{
			Resolved.Center = State.TargetLocation;
			Resolved.Transform = FTransform(State.TargetLocation);
			Resolved.bValid = true;
		}
	}
}

void UTRTargetDirectorSubsystem::SetTargetActor(ETRTargetChannel Channel, AActor* NewTarget)
{
	if (!IsValidChannel(Channel))
	{
		return;
	}

	FTRTargetChannelState& State = Channels[static_cast<int32>(Channel)];

	// 같은 대상 재지정이면 바운드를 다시 재지 않는다. 형상 계산은 컴포넌트를 전부 훑는다.
	if (State.bHasTarget && State.TargetActor.Get() == NewTarget)
	{
		return;
	}

	State.TargetActor = NewTarget;
	State.bHasTarget = (NewTarget != nullptr);

	// 형상은 등록 시 한 번만 잰다. 기차는 매 프레임 움직이지만 로컬 바운드는 그대로다.
	State.bHasBounds = false;

	if (NewTarget != nullptr)
	{
		State.bHasBounds = MeasureLocalBounds(*NewTarget, State.LocalBounds);
	}

	// 첫 틱 전에 조회하는 쪽이 있다(웨이브 스포너의 BeginPlay 검사).
	RefreshSnapshot();
}

void UTRTargetDirectorSubsystem::RefreshTargetBounds(ETRTargetChannel Channel)
{
	if (!IsValidChannel(Channel))
	{
		return;
	}

	FTRTargetChannelState& State = Channels[static_cast<int32>(Channel)];
	AActor* TargetActor = State.TargetActor.Get();
	if (TargetActor == nullptr)
	{
		return;
	}

	// SetTargetActor는 같은 대상이면 조기 반환하므로, 재계산은 이 경로로만 가능하다.
	State.bHasBounds = MeasureLocalBounds(*TargetActor, State.LocalBounds);

	RefreshSnapshot();
}

void UTRTargetDirectorSubsystem::SetTargetLocation(ETRTargetChannel Channel, FVector NewLocation)
{
	if (!IsValidChannel(Channel))
	{
		return;
	}

	FTRTargetChannelState& State = Channels[static_cast<int32>(Channel)];
	State.TargetActor = nullptr;
	State.TargetLocation = NewLocation;
	State.bHasTarget = true;
	State.bHasBounds = false;

	RefreshSnapshot();
}

void UTRTargetDirectorSubsystem::ClearTarget(ETRTargetChannel Channel)
{
	if (!IsValidChannel(Channel))
	{
		return;
	}

	FTRTargetChannelState& State = Channels[static_cast<int32>(Channel)];
	State.TargetActor = nullptr;
	State.bHasTarget = false;
	State.bHasBounds = false;

	RefreshSnapshot();
}

bool UTRTargetDirectorSubsystem::HasTarget(ETRTargetChannel Channel) const
{
	FTRResolvedTarget Unused;
	return GetResolvedTarget(Channel, Unused);
}

AActor* UTRTargetDirectorSubsystem::GetTargetActor(ETRTargetChannel Channel) const
{
	if (!IsValidChannel(Channel))
	{
		return nullptr;
	}

	return Channels[static_cast<int32>(Channel)].TargetActor.Get();
}

bool UTRTargetDirectorSubsystem::GetResolvedTarget(ETRTargetChannel Channel, FTRResolvedTarget& OutTarget) const
{
	OutTarget = FTRResolvedTarget();

	if (!IsValidChannel(Channel) || !Snapshot.IsValidIndex(static_cast<int32>(Channel)))
	{
		return false;
	}

	OutTarget = Snapshot[static_cast<int32>(Channel)];

	return OutTarget.bValid;
}
