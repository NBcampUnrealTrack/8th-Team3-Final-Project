// TRTrain.cpp

#include "Train/TRTrain.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Debug/TRGameplayLog.h"
#include "TRTypes.h"
#include "Train/TRArmor.h"
#include "Train/TRTurret.h"

// 비어 있는 슬롯 이름들을 이어 반환한다. 전부 채워졌으면 빈 문자열.
// ensure 는 호출하지 않는다 — 템플릿 인스턴스가 링커에 접히면 정적 상태를 공유해
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

// 폭발 구가 대상 컴포넌트에 조금이라도 걸치면 맞은 것으로 본다.
// 액터 전체 바운드를 쓰면 안 된다 — 포탑은 외부 메시와 실내 조종 구조물을 함께 갖고 있어
// 합친 박스가 차체를 가로지르고, 반대편에서 터진 폭발까지 걸리게 된다.
static bool IsWithinBlast(const UPrimitiveComponent* Target, const FVector& ImpactPoint, float RadiusSq)
{
	if (!IsValid(Target))
	{
		return false;
	}

	// 점이 박스 안이면 0 을 반환하므로 "박스를 통째로 삼키는 폭발" 도 그대로 걸린다.
	// 축 정렬 박스라 회전한 메시는 약간 넉넉하게 잡힌다.
	return Target->Bounds.GetBox().ComputeSquaredDistanceToPoint(ImpactPoint) <= RadiusSq;
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

// FindRef 는 키가 없으면 기본값(빈 TObjectPtr)을 값으로 돌려주므로,
// "슬롯 자체가 없음" 과 "슬롯은 있는데 액터 미할당" 이 똑같이 nullptr 로 나온다.
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
	// TODO: 게임오버 처리.
}

void ATRTrain::ApplyImpactDamage(const FVector& ImpactPoint, float Damage, float Radius, AController* EventInstigator, AActor* DamageCauser)
{
	// 선체가 이미 파괴됐으면(게임 오버) 더 이상 아무것도 피해를 받지 않는다.
	if (!HasAuthority() || IsHullDestroyed() || Damage <= 0.f)
	{
		return;
	}

	// 터진 자리를 덮는 장갑이 살아있으면 그 장갑이 100% 흡수한다
	if (ATRArmor* Armor = FindArmorAt(ImpactPoint))
	{
		UGameplayStatics::ApplyDamage(Armor, Damage, EventInstigator, DamageCauser, nullptr);
		UE_LOG(LogTRGameplay, Verbose, TEXT("Impact absorbed by %s (%.0f)"), *Armor->GetName(), Damage);
		return;
	}

	// 장갑이 없거나 파괴됨 → 폭발에 걸친 포탑들과 차체가 각각 풀 데미지 (나눠 갖지 않는다)
	// 호출부가 반경을 안 넘겼으면 기차 기본값으로 대체한다.
	const float EffectiveRadius = Radius > 0.f ? Radius : ImpactRadius;
	const float EffectiveRadiusSq = EffectiveRadius * EffectiveRadius;
	for (const TPair<ETRTurretSlot, TObjectPtr<ATRTurret>>& Slot : TurretSlots)
	{
		ATRTurret* Turret = Slot.Value;
		if (IsValid(Turret) && !Turret->IsPartDestroyed()
			&& IsWithinBlast(Turret->GetTurretMesh(), ImpactPoint, EffectiveRadiusSq))
		{
			UGameplayStatics::ApplyDamage(Turret, Damage, EventInstigator, DamageCauser, nullptr);
		}
	}

	// 선체는 반경 판정 없이 항상 맞는다.
	UGameplayStatics::ApplyDamage(this, Damage, EventInstigator, DamageCauser, nullptr);

	UE_LOG(LogTRGameplay, Verbose, TEXT("Impact exposed: hull and turrets within %.0f hit for %.0f"), EffectiveRadius, Damage);
}

ATRArmor* ATRTrain::FindArmorAt(const FVector& Point) const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	// 반지름 0 은 물리 백엔드에 따라 결과가 나오지 않을 수 있어 최소값을 쓴다.
	constexpr float PointQueryRadius = 1.f;

	// TRDamage 는 장갑 DamageVolume 만 반응하는 필터용 채널이다 (막는 용도가 아니다)
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Point, FQuat::Identity, TRCollisionChannel::Damage, FCollisionShape::MakeSphere(PointQueryRadius));

	for (const FOverlapResult& Overlap : Overlaps)
	{
		// 슬롯 검사 필수 — 오버랩은 월드 전체를 훑으므로 다른 기차/레벨에 놓인 장갑도 걸린다.
		ATRArmor* Armor = Cast<ATRArmor>(Overlap.GetActor());
		if (IsValid(Armor) && !Armor->IsPartDestroyed() && ArmorSlots.FindKey(Armor) != nullptr)
		{
			return Armor;
		}
	}
	return nullptr;
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
