// TRArmor.cpp


#include "Train/TRArmor.h"
#include "Components/BoxComponent.h"
#include "TRTypes.h"

ATRArmor::ATRArmor()
{
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
}

float ATRArmor::GetMaxHP() const
{
	if (LevelTable)
	{
		if (const FTRArmorLevelRow* Row = LevelTable->FindRow<FTRArmorLevelRow>(FName(*FString::FromInt(CurrentLevel)), TEXT("GetMaxHP")))
		{
			return Row->MaxHP;
		}
	}
	return 0.f;
}

int32 ATRArmor::GetInstallCost() const
{
	if (LevelTable)
	{
		if (const FTRArmorLevelRow* Row = LevelTable->FindRow<FTRArmorLevelRow>(FName(*FString::FromInt(CurrentLevel)), TEXT("GetInstallCost")))
		{
			return Row->InstallCost;
		}
	}
	return 0;
}
