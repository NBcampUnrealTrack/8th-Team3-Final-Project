// TRTrain.cpp

#include "Train/TRTrain.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Debug/TRGameplayLog.h"
#include "Core/TRGameMode.h"
#include "TRTypes.h"
#include "Train/TRArmor.h"
#include "Train/TRTurret.h"

// 비어 있는 슬롯 이름들을 이어 반환한다. 전부 채워졌으면 빈 문자열.
// ensure 는 호출하지 않는다. 템플릿 인스턴스가 링커에 접히면 정적 상태를 공유해
// 한쪽 ensure 가 다른 쪽을 억눌러버린다. 판정은 여기서, 보고는 ValidateSlots 에서.
template <typename TSlotEnum, typename TPart>
static FString FindMissingSlots(const TMap<TSlotEnum, TObjectPtr<TPart>>& Slots)
{
	const UEnum* SlotEnum = StaticEnum<TSlotEnum>();
	if (!IsValid(SlotEnum))
	{
		return TEXT("<enum not found>");
	}

	TArray<FString> Missing;
	// UHT 가 마지막에 _MAX 를 자동으로 붙이므로 하나 뺀다.
	const int32 SlotCount = SlotEnum->NumEnums() - 1;
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		const TObjectPtr<TPart>* Found = Slots.Find(static_cast<TSlotEnum>(SlotEnum->GetValueByIndex(Index)));
		if (!Found || !IsValid(*Found))
		{
			Missing.Add(SlotEnum->GetNameStringByIndex(Index));
		}
	}
	return FString::Join(Missing, TEXT(", "));
}

ATRTrain::ATRTrain()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;
}

void ATRTrain::BeginPlay()
{
	Super::BeginPlay();

	// DefaultEngine.ini 의 채널 등록 순서가 바뀌면 TRCollisionChannel::Damage 가 엉뚱한 채널을 가리키게 된다.
	// 그 경우 장갑 흡수 판정이 조용히 실패하므로, 여기서 인덱스와 이름이 맞는지 확인한다.
	if (const UCollisionProfile* CollisionProfile = UCollisionProfile::Get())
	{
		const FName ExpectedName(TRCollisionChannel::DamageName);
		const FName ActualName = CollisionProfile->ReturnChannelNameFromContainerIndex(static_cast<int32>(TRCollisionChannel::Damage));
		ensureMsgf(ActualName == ExpectedName,
			TEXT("TRCollisionChannel::Damage points at '%s', expected '%s'. Check channel order in DefaultEngine.ini."),
			*ActualName.ToString(), *ExpectedName.ToString());
	}

	ValidateSlots();
}

void ATRTrain::ValidateSlots() const
{
	// 두 ensure 는 반드시 서로 다른 줄에 있어야 각각 독립적으로 발동한다.
	const FString MissingArmor = FindMissingSlots(ArmorSlots);
	ensureMsgf(MissingArmor.IsEmpty(), TEXT("%s: Armor slots not assigned in the level: %s"), *GetName(), *MissingArmor);

	const FString MissingTurret = FindMissingSlots(TurretSlots);
	ensureMsgf(MissingTurret.IsEmpty(), TEXT("%s: Turret slots not assigned in the level: %s"), *GetName(), *MissingTurret);
}

ATRArmor* ATRTrain::GetArmorAt(ETRArmorSlot Slot) const
{
	return ArmorSlots.FindRef(Slot);
}

ATRTurret* ATRTrain::GetTurretAt(ETRTurretSlot Slot) const
{
	return TurretSlots.FindRef(Slot);
}

// 서버 전용. 클라에서는 Super 를 타지 않으므로 OnTakeAnyDamage / BP ReceiveAnyDamage 도 호출되지 않는다.
float ATRTrain::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || IsHullDestroyed())
	{
		return 0.f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage <= 0.f)
	{
		return 0.f;
	}

	const float PrevHealth = CurrentHullHealth;
	CurrentHullHealth = FMath::Max(0.f, CurrentHullHealth - ActualDamage);
	UE_LOG(LogTRGameplay, Verbose, TEXT("Hull damaged: %.0f -> %.0f"), PrevHealth, CurrentHullHealth);

	if (IsHullDestroyed())
	{
		OnHullDestroyed();
	}

	return PrevHealth - CurrentHullHealth;
}

void ATRTrain::OnHullDestroyed()
{
	UE_LOG(LogTRGameplay, Log, TEXT("Hull destroyed - game over"));

	// 판을 끝내는 일 자체는 게임모드가 총괄한다. 기차는 "부서졌다"만 알린다 —
	// 웨이브 중단·잔존 적 정리·상태 복제는 순서가 중요해서 한 곳에 모아 둔다.
	//
	// GetAuthGameMode 는 클라이언트에서 nullptr 이라, 서버 권위 검사가 함께 걸린다.
	UWorld* World = GetWorld();
	if (ATRGameMode* GameMode = World ? World->GetAuthGameMode<ATRGameMode>() : nullptr)
	{
		GameMode->EndMatch();
	}
}

void ATRTrain::ApplyImpactDamage(const FVector& ImpactPoint, float Damage, AController* EventInstigator, AActor* DamageCauser)
{
	// 선체가 이미 파괴됐으면(게임 오버) 더 이상 아무것도 피해를 받지 않는다.
	if (!HasAuthority() || IsHullDestroyed() || Damage <= 0.f)
	{
		return;
	}

	// 착탄점에 가장 가까운 부위 하나만 맞는다.
	ATRInstallablePart* NearestPart = nullptr;
	double NearestDistanceSq = TNumericLimits<double>::Max();

	const auto Consider = [&](ATRInstallablePart* Part, const UPrimitiveComponent* Volume)
	{
		if (!IsValid(Volume))
		{
			return;
		}

		const double DistanceSq = Volume->Bounds.GetBox().ComputeSquaredDistanceToPoint(ImpactPoint);
		if (DistanceSq < NearestDistanceSq)
		{
			NearestDistanceSq = DistanceSq;
			NearestPart = Part;
		}
	};

	for (const TPair<ETRArmorSlot, TObjectPtr<ATRArmor>>& Slot : ArmorSlots)
	{
		if (ATRArmor* Armor = Slot.Value.Get())
		{
			Consider(Armor, Armor->GetDamageVolume());
		}
	}

	for (const TPair<ETRTurretSlot, TObjectPtr<ATRTurret>>& Slot : TurretSlots)
	{
		if (ATRTurret* Turret = Slot.Value.Get())
		{
			Consider(Turret, Turret->GetTurretMesh());
		}
	}

	if (NearestPart == nullptr)
	{
		// 슬롯이 비었거나 볼륨이 전부 없는 구성 오류다.
		UE_LOG(LogTRGameplay, Warning, TEXT("ApplyImpactDamage: 부위를 찾지 못했다 — 슬롯 할당을 확인할 것"));
		return;
	}

	if (NearestPart->IsPartDestroyed())
	{
		// 가장 가까운 부위가 파괴 상태면 선체가 직접 맞는다.
		UGameplayStatics::ApplyDamage(this, Damage, EventInstigator, DamageCauser, nullptr);
		
		UE_LOG(LogTRGameplay, Verbose, TEXT("Impact through destroyed %s -> hull (%.0f)"),
			*NearestPart->GetName(), Damage);
		
		return;
	}

	UGameplayStatics::ApplyDamage(NearestPart, Damage, EventInstigator, DamageCauser, nullptr);
	
	UE_LOG(LogTRGameplay, Verbose, TEXT("Impact taken by %s (%.0f, %.0fcm)"),
		*NearestPart->GetName(), Damage, FMath::Sqrt(NearestDistanceSq));
}

void ATRTrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRTrain, CurrentHullHealth);
}

int32 ATRTrain::GetActiveArmorCount() const
{
	int32 Count = 0;
	for (const TPair<ETRArmorSlot, TObjectPtr<ATRArmor>>& Slot : ArmorSlots)
	{
		if (IsValid(Slot.Value) && !Slot.Value->IsPartDestroyed())
		{
			++Count;
		}
	}
	return Count;
}

void ATRTrain::ApplyRepair(float Amount)
{
	CurrentHullHealth = FMath::Clamp(CurrentHullHealth + Amount, 0.f, MaxHullHealth);
}

TArray<ATRArmor*> ATRTrain::GetDestroyedArmor() const
{
	TArray<ATRArmor*> Result;
	for (const TPair<ETRArmorSlot, TObjectPtr<ATRArmor>>& Slot : ArmorSlots)
	{
		if (IsValid(Slot.Value) && Slot.Value->IsPartDestroyed())
		{
			Result.Add(Slot.Value);
		}
	}
	return Result;
}

TArray<ATRTurret*> ATRTrain::GetDestroyedTurrets() const
{
	TArray<ATRTurret*> Result;
	for (const TPair<ETRTurretSlot, TObjectPtr<ATRTurret>>& Slot : TurretSlots)
	{
		if (IsValid(Slot.Value) && Slot.Value->IsPartDestroyed())
		{
			Result.Add(Slot.Value);
		}
	}
	return Result;
}
