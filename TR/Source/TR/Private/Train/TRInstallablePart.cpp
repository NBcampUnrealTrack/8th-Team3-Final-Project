// TRInstallablePart.cpp


#include "Train/TRInstallablePart.h"
#include "Net/UnrealNetwork.h"

ATRInstallablePart::ATRInstallablePart()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ATRInstallablePart::BeginPlay()
{
	Super::BeginPlay();

	// 레벨 배치 파츠는 설치 완료 상태로 게임을 시작한다.
	if (HasAuthority())
	{
		ApplyInstall();
	}
}

void ATRInstallablePart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRInstallablePart, CurrentHP);
}

void ATRInstallablePart::ApplyRepair(float Amount)
{
	CurrentHP = FMath::Clamp(CurrentHP + Amount, 0.f, GetMaxHP());
}

void ATRInstallablePart::ApplyInstall()
{
	CurrentHP = GetMaxHP();
}
