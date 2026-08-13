// TRArmor.cpp


#include "Train/TRArmor.h"
#include "Components/BoxComponent.h"
#include "TRTypes.h"
#include "Core/TRGameState.h"

ATRArmor::ATRArmor()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// 창문 방범창 — 실내에서 수리·설치를 상호작용하는 지점.
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(Root);
	// 박스 기본 프로파일은 Visibility 가 Overlap 이라 상호작용 트레이스에 안 잡힌다. 지우면 안 됨.
	CollisionBox->SetCollisionProfileName(TRCollisionProfile::Interactable);

	// 이 장갑이 지키는 구역. 파괴돼도 반응은 유지한다 — 흡수 여부는 ATRTrain::FindArmorAt 이 판정한다.
	DamageVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageVolume"));
	DamageVolume->SetupAttachment(Root);
	DamageVolume->SetCollisionProfileName(TRCollisionProfile::ArmorDamageVolume);
}

const FTRArmorLevelRow* ATRArmor::GetCurrentLevelRow(const TCHAR* ContextName) const
{
	if (!ensureMsgf(LevelTable, TEXT("%s: LevelTable not assigned"), *GetName()))
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	const ATRGameState* GS = IsValid(World) ? World->GetGameState<ATRGameState>() : nullptr;
	const int32 Level = IsValid(GS) ? GS->GetArmorLevel() : 1;

	const FTRArmorLevelRow* Row = LevelTable->FindRow<FTRArmorLevelRow>(FName(*FString::FromInt(Level)), ContextName);
	ensureMsgf(Row, TEXT("%s: level row %d not found in LevelTable"), *GetName(), Level);
	return Row;
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
