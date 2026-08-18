// TRControlConsole.cpp

#include "Train/TRControlConsole.h"

#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TRTypes.h"

ATRControlConsole::ATRControlConsole()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	ConsoleMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConsoleMeshComp"));
	RootComponent = ConsoleMeshComp;
	// 로비·게임 양쪽 조준선에 걸린다. 콜리전은 판이 시작돼도 바뀌지 않고,
	// 무엇을 열지는 잡힌 뒤 ATRPlayerCharacter::OnInteractPressed 가 IsMatchStarted() 로 가른다.
	ConsoleMeshComp->SetCollisionProfileName(TRCollisionProfile::DualInteractable);
}

void ATRControlConsole::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRControlConsole, Occupant);
}

bool ATRControlConsole::TryOccupy(APawn* NewOccupant)
{
	if (!HasAuthority() || !IsValid(NewOccupant) || IsOccupied())
	{
		return false;
	}

	Occupant = NewOccupant;
	return true;
}

void ATRControlConsole::Release(const APawn* Pawn)
{
	if (!HasAuthority() || !IsOccupiedBy(Pawn))
	{
		return;
	}

	Occupant = nullptr;
}
