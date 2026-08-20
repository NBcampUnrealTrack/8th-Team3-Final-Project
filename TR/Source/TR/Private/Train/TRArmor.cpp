// TRArmor.cpp


#include "Train/TRArmor.h"
#include "Components/BoxComponent.h"
#include "TRTypes.h"
#include "Core/TRGameState.h"
#include "NiagaraComponent.h"

ATRArmor::ATRArmor()
{
	WindowCollision1 = CreateDefaultSubobject<UBoxComponent>(TEXT("WindowCollision1"));
	WindowCollision1->SetupAttachment(Root);
	// 박스 기본 프로파일은 Visibility 가 Overlap 이라 상호작용 트레이스에 안 잡힌다. 지우면 안 됨.
	WindowCollision1->SetCollisionProfileName(TRCollisionProfile::Interactable);

	WindowCollision2 = CreateDefaultSubobject<UBoxComponent>(TEXT("WindowCollision2"));
	WindowCollision2->SetupAttachment(Root);
	WindowCollision2->SetCollisionProfileName(TRCollisionProfile::Interactable);

	// 이 장갑이 지키는 구역. 파괴돼도 반응은 유지한다 — 흡수 여부는 ATRTrain::FindArmorAt 이 판정한다.
	DamageVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageVolume"));
	DamageVolume->SetupAttachment(Root);
	DamageVolume->SetCollisionProfileName(TRCollisionProfile::ArmorDamageVolume);
}

void ATRArmor::BeginPlay()
{
	Super::BeginPlay();

	// CurrentHP 는 서버가 정하고 복제된다. 클라이언트가 붙을 이유가 없다.
	if (!HasAuthority())
	{
		return;
	}

	const UWorld* World = GetWorld();
	ATRGameState* GS = IsValid(World) ? World->GetGameState<ATRGameState>() : nullptr;
	if (IsValid(GS))
	{
		GS->OnArmorLevelUpgraded.AddUObject(this, &ATRArmor::ApplyLevelUpgrade);
	}
}

void ATRArmor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const UWorld* World = GetWorld();
	ATRGameState* GS = IsValid(World) ? World->GetGameState<ATRGameState>() : nullptr;
	if (IsValid(GS))
	{
		GS->OnArmorLevelUpgraded.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

const FTRArmorLevelRow* ATRArmor::GetCurrentLevelRow(const TCHAR* ContextName) const
{
	// GameState 가 아직 복제되지 않은 클라이언트에서는 잠시 nullptr 이다.
	// CurrentHP 는 복제되고 설치는 서버에서만 하므로 이 창에서 판단이 뒤집히지는 않는다.
	const UWorld* World = GetWorld();
	const ATRGameState* GS = IsValid(World) ? World->GetGameState<ATRGameState>() : nullptr;
	return IsValid(GS) ? GS->GetArmorLevelRow(ContextName) : nullptr;
}


float ATRArmor::GetMaxHP() const
{
	if (const FTRArmorLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->MaxHP;
	}
	return 0.f;
}

int32 ATRArmor::GetInstallCost() const
{
	if (const FTRArmorLevelRow* Row = GetCurrentLevelRow(ANSI_TO_TCHAR(__FUNCTION__)))
	{
		return Row->InstallCost;
	}
	return 0;
}
