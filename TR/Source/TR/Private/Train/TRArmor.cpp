// TRArmor.cpp


#include "Train/TRArmor.h"
#include "Components/BoxComponent.h"
#include "TRTypes.h"

ATRArmor::ATRArmor()
{
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
}

const FTRArmorLevelRow* ATRArmor::GetCurrentLevelRow(const TCHAR* ContextName) const
{
	if (!ensureMsgf(LevelTable, TEXT("%s: LevelTable not assigned"), *GetName()))
	{
		return nullptr;
	}

	const FTRArmorLevelRow* Row = LevelTable->FindRow<FTRArmorLevelRow>(FName(*FString::FromInt(CurrentLevel)), ContextName);
	ensureMsgf(Row, TEXT("%s: level row %d not found in LevelTable"), *GetName(), CurrentLevel);
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
