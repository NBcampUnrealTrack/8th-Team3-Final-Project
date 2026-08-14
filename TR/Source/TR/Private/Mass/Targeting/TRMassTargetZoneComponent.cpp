// TRMassTargetZoneComponent.cpp

#include "Mass/Targeting/TRMassTargetZoneComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Targeting/TRTargetDirectorSubsystem.h"

UTRMassTargetZoneComponent::UTRMassTargetZoneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTRMassTargetZoneComponent::BeginPlay()
{
	Super::BeginPlay();

	// 적 시뮬레이션은 서버 권위다. 클라에서 등록하면 표적 목록이 서로 어긋난다.
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	RegisterZone();
}

void UTRMassTargetZoneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterZone();

	Super::EndPlay(EndPlayReason);
}

void UTRMassTargetZoneComponent::RegisterZone()
{
	if (bRegistered)
	{
		return;
	}

	UWorld* World = GetWorld();
	UTRTargetDirectorSubsystem* Director = World ? World->GetSubsystem<UTRTargetDirectorSubsystem>() : nullptr;
	if (Director == nullptr)
	{
		return;
	}

	Volume = ResolveVolume();
	if (!ensureMsgf(Volume != nullptr,
		TEXT("'%s'에서 표적 구역으로 쓸 프리미티브를 찾지 못했습니다."), *GetNameSafe(GetOwner())))
	{
		return;
	}

	Director->RegisterTargetZone(Channel, Volume, Tier);
	bRegistered = true;

	UE_LOG(LogTRMass, Log, TEXT("표적 구역 등록: '%s'의 '%s' (등급 %s)"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Volume),
		*StaticEnum<ETRTargetZoneTier>()->GetNameStringByValue(static_cast<int64>(Tier)));
}

void UTRMassTargetZoneComponent::UnregisterZone()
{
	if (!bRegistered)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (UTRTargetDirectorSubsystem* Director = World ? World->GetSubsystem<UTRTargetDirectorSubsystem>() : nullptr)
	{
		Director->UnregisterTargetZone(Channel, Volume);
	}

	bRegistered = false;
}

UPrimitiveComponent* UTRMassTargetZoneComponent::ResolveVolume() const
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return nullptr;
	}

	UPrimitiveComponent* FirstPrimitive = nullptr;

	for (UActorComponent* Component : Owner->GetComponents())
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (Primitive == nullptr)
		{
			continue;
		}

		if (VolumeComponentName.IsNone())
		{
			return Primitive;
		}

		if (Primitive->GetFName() == VolumeComponentName)
		{
			return Primitive;
		}

		if (FirstPrimitive == nullptr)
		{
			FirstPrimitive = Primitive;
		}
	}

	// 이름을 못 찾으면 첫 프리미티브로 물러난다. 이름 오타로 구역이 통째로 빠지지 않게 한다.
	if (VolumeComponentName.IsNone() || FirstPrimitive == nullptr)
	{
		return FirstPrimitive;
	}

	UE_LOG(LogTRMass, Warning, TEXT("'%s'에 '%s' 컴포넌트가 없어 '%s'를 씁니다."),
		*GetNameSafe(Owner), *VolumeComponentName.ToString(), *GetNameSafe(FirstPrimitive));

	return FirstPrimitive;
}
