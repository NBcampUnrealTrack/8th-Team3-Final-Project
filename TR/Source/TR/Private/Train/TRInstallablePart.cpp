// TRInstallablePart.cpp


#include "Train/TRInstallablePart.h"

#include "Components/SceneComponent.h"
#include "Debug/TRGameplayLog.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"

ATRInstallablePart::ATRInstallablePart()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	DamageEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DamageEffect"));
	DamageEffect->SetupAttachment(Root);
	DamageEffect->bAutoActivate = false;
}

void ATRInstallablePart::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		RefreshDamageEffect();
		return;
	}

	// 레벨 배치 파츠는 설치 완료 상태로 게임을 시작한다.
	// 단, 에디터에서 CurrentHP를 미리 세팅해뒀으면 덮어쓰지 않는다.
	if (CurrentHP <= 0.f)
	{
		ApplyInstall();
	}
	else
	{
		UpdateDamageEffectState();
	}
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
	UpdateDamageEffectState();
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

void ATRInstallablePart::OnPartInstalled()
{
	UE_LOG(LogTRGameplay, Log, TEXT("Installed: %s"), *GetName());
}

void ATRInstallablePart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATRInstallablePart, CurrentHP);
	DOREPLIFETIME(ATRInstallablePart, bDamageEffectActive);
}

void ATRInstallablePart::ApplyRepair(float Amount)
{
	CurrentHP = FMath::Clamp(CurrentHP + Amount, 0.f, GetMaxHP());
	UpdateDamageEffectState();
}

void ATRInstallablePart::ApplyInstall()
{
	CurrentHP = GetMaxHP();
	UpdateDamageEffectState();
	
	if (!IsPartDestroyed())
	{
		OnPartInstalled();
	}
}

void ATRInstallablePart::ApplyLevelUpgrade(float PrevMaxHP)
{
	// 파괴된 파츠는 재설치 비용을 내고 살려야 한다. 여기서 채우면 공짜 부활이 된다.
	if (IsPartDestroyed())
	{
		return;
	}

	// 행 조회에 실패해 이전 최대치를 모르면 비율을 구할 수 없다. 최대치만 오르고 현재 HP 는 그대로 둔다.
	if (PrevMaxHP <= 0.f)
	{
		UpdateDamageEffectState();
		UE_LOG(LogTRGameplay, Warning, TEXT("%s: level upgrade kept HP as-is — previous MaxHP unknown"), *GetName());
		return;
	}

	// HP 는 정수로만 다루므로 반올림한다. 풀피였다면 비율이 정확히 1 이라 새 최대치에 그대로 맞는다.
	const float NewMaxHP = GetMaxHP();
	CurrentHP = FMath::Clamp(FMath::RoundToFloat(NewMaxHP * (CurrentHP / PrevMaxHP)), 0.f, NewMaxHP);
	UpdateDamageEffectState();

	UE_LOG(LogTRGameplay, Verbose, TEXT("%s: level upgraded, HP %.0f / %.0f"), *GetName(), CurrentHP, NewMaxHP);
}

void ATRInstallablePart::UpdateDamageEffectState()
{
	// 손상 지속 상태는 서버 한 곳에서만 판정하고 결과 bool만 복제한다.
	if (!HasAuthority())
	{
		return;
	}

	const float MaxHP = GetMaxHP();
	const float HealthRatio = MaxHP > 0.f ? CurrentHP / MaxHP : 0.f;
	bDamageEffectActive = CurrentHP > 0.f && MaxHP > 0.f
		&& HealthRatio <= DamageEffectThresholdRatio;
	RefreshDamageEffect();
}

void ATRInstallablePart::RefreshDamageEffect()
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(DamageEffect))
	{
		return;
	}

	if (bDamageEffectActive)
	{
		if (!DamageEffect->IsActive())
		{
			DamageEffect->Activate(true);
		}
	}
	else if (DamageEffect->IsActive())
	{
		DamageEffect->Deactivate();
	}
}

void ATRInstallablePart::OnRep_DamageEffectActive()
{
	RefreshDamageEffect();
}
