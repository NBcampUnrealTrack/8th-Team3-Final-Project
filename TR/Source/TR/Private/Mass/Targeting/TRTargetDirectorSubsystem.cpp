// TRTargetDirectorSubsystem.cpp

#include "Mass/Targeting/TRTargetDirectorSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Train/TRInstallablePart.h"
#include "Train/TRTrain.h"

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

	/**
	 * 게임플레이상 죽은 액터인지. **UObject 로서의 유효성과 다르다.**
	 *
	 * 기차도 장갑·포탑도 파괴되어도 액터는 그대로 남는다(잔해를 보여주고 수리도 받아야
	 * 하므로). 그래서 TWeakObjectPtr 이 유효하다는 것만으로 "살아 있는 표적"이라고
	 * 판단하면, 기차가 터진 뒤에도 적들이 계속 몰려든다.
	 */
	bool IsTargetActorDead(const AActor* Actor)
	{
		if (Actor == nullptr)
		{
			return true;
		}

		if (const ATRTrain* Train = Cast<ATRTrain>(Actor))
		{
			return Train->IsHullDestroyed();
		}

		if (const ATRInstallablePart* Part = Cast<ATRInstallablePart>(Actor))
		{
			return Part->IsPartDestroyed();
		}

		// 죽음 개념이 없는 액터(고정 위치 목표 등)는 살아 있는 것으로 본다.
		return false;
	}

	/** 파괴된 장갑·포탑인지. 액터는 남아 있어도 표적에서 빠져야 한다. */
	bool IsZoneOwnerDestroyed(const FTRTargetZoneState& Zone)
	{
		return IsTargetActorDead(Zone.Owner.Get());
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
			// 파괴된 목표는 더 이상 표적이 아니다.
			//
			// 액터는 파괴된 뒤에도 남아 있으므로(잔해 표시·수리) 포인터 유효성만으로는
			// 걸러지지 않는다. 이걸 빠뜨리면 기차가 터진 뒤에도 적들이 계속 몰려든다.
			// bValid 를 false 로 두면 TargetSync 가 bHasTarget 을 내리고, Steering 이
			// 조종력을 0으로 만들면서 도달 태그까지 풀어준다.
			if (IsTargetActorDead(TargetActor))
			{
				continue;
			}

			Resolved.Center = TargetActor->GetActorLocation();
			Resolved.Transform = TargetActor->GetActorTransform();
			Resolved.bHasBounds = State.bHasBounds;
			Resolved.bValid = true;

			// 죽은 구역도 자리를 지킨다. 사격형이 번호로 표적을 고정하기 때문이다.
			for (const FTRTargetZoneState& Zone : State.Zones)
			{
				if (Resolved.ZoneCount >= FTRResolvedTarget::MaxZones)
				{
					break;
				}

				const UPrimitiveComponent* Volume = Zone.Volume.Get();
				const bool bAlive = (Volume != nullptr) && Zone.Owner.IsValid() && !IsZoneOwnerDestroyed(Zone);

				Resolved.ZoneBounds[Resolved.ZoneCount] = bAlive ? Volume->Bounds.GetBox() : FBox(ForceInit);
				Resolved.ZoneAlive[Resolved.ZoneCount] = bAlive;
				Resolved.ZoneTier[Resolved.ZoneCount] = Zone.Tier;
				++Resolved.ZoneCount;
			}

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

	State.Zones.Reset();

	if (NewTarget != nullptr)
	{
		State.bHasBounds = MeasureLocalBounds(*NewTarget, State.LocalBounds);

		// 구역은 UTRMassTargetZoneComponent 등록이 유일한 출처다.
		UE_LOG(LogTRMass, Log, TEXT("채널[%d] 목표 '%s' 등록"),
			static_cast<int32>(Channel), *NewTarget->GetName());
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
	// 바운드만 다시 잰다. 구역은 건드리지 않는다.
	State.bHasBounds = MeasureLocalBounds(*TargetActor, State.LocalBounds);

	RefreshSnapshot();
}

void UTRTargetDirectorSubsystem::RegisterTargetZone(ETRTargetChannel Channel, UPrimitiveComponent* Volume,
	ETRTargetZoneTier Tier)
{
	if (!ensureMsgf(IsValidChannel(Channel) && Volume != nullptr,
		TEXT("추적 구역 등록 실패 — 채널이나 볼륨이 잘못됐습니다.")))
	{
		return;
	}

	FTRTargetChannelState& State = Channels[static_cast<int32>(Channel)];

	int32 FreeIndex = INDEX_NONE;
	for (int32 Index = 0; Index < State.Zones.Num(); ++Index)
	{
		const FTRTargetZoneState& Existing = State.Zones[Index];
		if (Existing.Volume.Get() == Volume)
		{
			return;
		}

		// 비워 둔 자리를 다시 쓴다.
		if (FreeIndex == INDEX_NONE && Existing.Volume.Get() == nullptr)
		{
			FreeIndex = Index;
		}
	}

	if (FreeIndex == INDEX_NONE && State.Zones.Num() >= FTRResolvedTarget::MaxZones)
	{
		UE_LOG(LogTRMass, Warning, TEXT("채널[%d] 추적 구역이 %d개를 넘어 '%s'를 등록하지 않습니다."),
			static_cast<int32>(Channel), FTRResolvedTarget::MaxZones, *GetNameSafe(Volume));
		return;
	}

	FTRTargetZoneState& Zone = (FreeIndex != INDEX_NONE)
		? State.Zones[FreeIndex]
		: State.Zones.AddDefaulted_GetRef();

	Zone.Volume = Volume;
	Zone.Owner = Volume->GetOwner();
	Zone.Tier = Tier;

	RefreshSnapshot();
}

void UTRTargetDirectorSubsystem::UnregisterTargetZone(ETRTargetChannel Channel, UPrimitiveComponent* Volume)
{
	if (!IsValidChannel(Channel) || Volume == nullptr)
	{
		return;
	}

	// 자리는 남기고 볼륨만 비운다. 번호가 밀리면 사격형이 고정해 둔 표적이 어긋난다.
	FTRTargetChannelState& State = Channels[static_cast<int32>(Channel)];
	for (FTRTargetZoneState& Zone : State.Zones)
	{
		if (Zone.Volume.Get() == Volume)
		{
			Zone.Volume = nullptr;
			Zone.Owner = nullptr;
			break;
		}
	}

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
	State.Zones.Reset();

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
	State.Zones.Reset();

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
