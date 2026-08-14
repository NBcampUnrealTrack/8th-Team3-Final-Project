// TRInstallablePart.cpp


#include "Train/TRInstallablePart.h"

#include "Debug/TRGameplayLog.h"
#include "Net/UnrealNetwork.h"

ATRInstallablePart::ATRInstallablePart()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

// 서버 전용. 클라에서는 Super 를 타지 않으므로 OnTakeAnyDamage / BP ReceiveAnyDamage 도 호출되지 않는다.
float ATRInstallablePart::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || IsPartDestroyed())
	{
		return 0.f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage <= 0.f)
	{
		return 0.f;
	}
	
	const float PrevHP = CurrentHP;
	CurrentHP = FMath::Max(0.f, CurrentHP - ActualDamage);
	UE_LOG(LogTRGameplay, Verbose, TEXT("Damaged: %s HP %.0f -> %.0f"), *GetName(), PrevHP, CurrentHP);

	if (IsPartDestroyed())
	{
		OnPartDestroyed();
	}
	
	return PrevHP - CurrentHP;
}

void ATRInstallablePart::OnPartDestroyed()
{
	UE_LOG(LogTRGameplay, Log, TEXT("Destroyed: %s"), *GetName());
}

void ATRInstallablePart::BeginPlay()
{
	Super::BeginPlay();

	// 레벨 배치 파츠는 설치 완료 상태로 게임을 시작한다. 단, 에디터에서 CurrentHP를 미리 세팅해뒀으면(테스트용 — 데미지 시스템 생기면 제거 검토) 덮어쓰지 않음.
	if (HasAuthority() && CurrentHP <= 0.f)
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
