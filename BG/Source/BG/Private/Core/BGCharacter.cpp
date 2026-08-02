#include "Core/BGCharacter.h"
#include "GAS/BGAbilitySystemComponent.h"
#include "GAS/BGAttributeSet.h"
#include "Debug/BGCardLog.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "BGGameplayTags.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ABGCharacter::ABGCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UBGAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// 7인 턴제라 대역폭이 무의미하고, 모든 클라가 동일한 GE 상태를 봐 디버깅이 쉽다 (md §M3).
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UBGAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* ABGCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ⚠️ 역할은 뱅의 핵심 은닉 정보다. 조건 없이 복제하면 모든 클라가 모든 역할을 알게 되어
	//    게임이 성립하지 않는다. 본인에게만 보낸다(폰의 Owner 는 소유 PlayerController).
	//    공개되어야 하는 역할(보안관 / 사망자 / 판 종료)은 ABGPlayerState::PublicRoleTag 로
	//    따로 올린다 — UI 는 반드시 그쪽을 봐야 한다.
	DOREPLIFETIME_CONDITION(ABGCharacter, CharacterTag, COND_OwnerOnly);
}

void ABGCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버: ActorInfo 초기화 후 GAS 최초 부여.
	InitializeAbilityActorInfo();
	ServerInitGAS();
}

void ABGCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// 소유 클라: ActorInfo 초기화 (없으면 UI 가 어트리뷰트/태그를 못 읽는다).
	InitializeAbilityActorInfo();
}

void ABGCharacter::InitializeAbilityActorInfo()
{
	if (AbilitySystemComponent)
	{
		// OwnerActor·AvatarActor 모두 this (Character 소유 ASC).
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void ABGCharacter::ServerInitGAS()
{
	if (!HasAuthority() || !AbilitySystemComponent || bGASInitialized)
	{
		return;
	}
	bGASInitialized = true;

	ApplyCharacterDataRow(CharacterTag);
	RefreshCharacterLooseTag();
}

void ABGCharacter::SetCharacterTag(const FGameplayTag& NewTag)
{
	CharacterTag = NewTag;

	// 이미 초기화됐다면 즉시 재적용, 아니면 ServerInitGAS 가 최초 적용한다.
	if (HasAuthority() && bGASInitialized)
	{
		ApplyCharacterDataRow(CharacterTag);
		RefreshCharacterLooseTag();
	}
}

void ABGCharacter::OnRep_CharacterTag()
{
	// 소유 클라: 역할 태그를 로컬 ASC 루즈 태그에 반영 (HUD/능력 조건 검사용).
	RefreshCharacterLooseTag();
}

void ABGCharacter::NotifyHealthDepleted(AActor* Killer)
{
	if (!HasAuthority() || bDead)
	{
		return;
	}
	bDead = true;

	// GA 의 ActivationBlockedTags(State.Dead) 를 걸기 위한 서버 측 태그.
	// 루즈 태그는 복제되지 않으므로, 클라가 보는 "죽었다" 는 PlayerState::bIsDead 가 담당한다.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(BG::State::Dead.GetTag());
	}

	ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	if (!PS)
	{
		return;
	}
	PS->SetDead(true);

	// 죽인 사람의 좌석 (보상/벌칙 판정용). 자살이거나 출처를 모르면 INDEX_NONE.
	int32 KillerSeat = INDEX_NONE;
	if (const APawn* KillerPawn = Cast<APawn>(Killer))
	{
		if (const ABGPlayerState* KillerPS = KillerPawn->GetPlayerState<ABGPlayerState>())
		{
			KillerSeat = KillerPS->GetSeatIndex();
		}
	}

	UE_LOG(LogBGCard, Log, TEXT("[Death] 좌석 %d 사망 (가해자 좌석 %d)"), PS->GetSeatIndex(), KillerSeat);

	// 이후 규칙 처리(카드 정리·보상/벌칙·승패·턴)는 룰 계층이 맡는다.
	if (ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr)
	{
		GS->HandleSeatDeath(PS->GetSeatIndex(), KillerSeat);
	}
}

void ABGCharacter::ApplyCharacterDataRow(const FGameplayTag& RowTag)
{
	if (!HasAuthority() || !AbilitySystemComponent || !RowTag.IsValid())
	{
		return;
	}

	const UGameInstance* GI = GetGameInstance();
	UBGDataSubsystem* DataSubsystem = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	if (!DataSubsystem)
	{
		return;
	}

	const FBGCharacterRow* Row = DataSubsystem->GetCharacterRow(RowTag);
	if (!Row)
	{
		return;
	}

	// 유효한 Row 를 찾았을 때만 이전 부여분을 정리한다
	// (잘못된 태그로 능력이 전부 날아가는 것 방지).
	ClearCharacterDataRow();

	UE_LOG(LogTemp, Log, TEXT("[BG] 능력 부여 시작 — %s (역할=%s, GE %d개 / 어빌리티 %d개)"),
		*GetName(), *RowTag.ToString(),
		(Row->DefaultAttributeGE ? 1 : 0) + Row->PassiveEffects.Num(),
		Row->GrantedAbilities.Num());

	// ── 스탯 GE + 패시브 GE 적용 ──
	TArray<TSubclassOf<UGameplayEffect>> EffectsToApply;
	if (Row->DefaultAttributeGE)
	{
		EffectsToApply.Add(Row->DefaultAttributeGE);
	}
	EffectsToApply.Append(Row->PassiveEffects);

	for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectsToApply)
	{
		if (!EffectClass)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, 1.f, Context);
		if (!Spec.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[BG]   이펙트 적용 실패 — %s (Spec 무효)"), *EffectClass->GetName());
			continue;
		}

		// Infinite/Duration GE 만 유효 핸들을 돌려준다(Instant 는 무효) → 재적용 시 제거용으로 추적.
		const FActiveGameplayEffectHandle Handle =
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (Handle.IsValid())
		{
			AppliedEffectHandles.Add(Handle);
		}
		UE_LOG(LogTemp, Log, TEXT("[BG]   이펙트 적용 — %s (%s)"),
			*EffectClass->GetName(), Handle.IsValid() ? TEXT("지속, 추적됨") : TEXT("즉시"));
	}

	// ── 능동 어빌리티 부여 ──
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Row->GrantedAbilities)
	{
		if (!AbilityClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BG]   능력 부여 스킵 — %s 의 GrantedAbilities 에 빈 항목."),
				*RowTag.ToString());
			continue;
		}

		const FGameplayAbilitySpecHandle Handle =
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));

		if (Handle.IsValid())
		{
			GrantedAbilityHandles.Add(Handle);
			UE_LOG(LogTemp, Log, TEXT("[BG]   능력 부여 성공 — %s"), *AbilityClass->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[BG]   능력 부여 실패 — %s (핸들 무효)"), *AbilityClass->GetName());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BG] 능력 부여 완료 — %s (어빌리티 %d개 부여됨)"),
		*GetName(), GrantedAbilityHandles.Num());
}

void ABGCharacter::ClearCharacterDataRow()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const FActiveGameplayEffectHandle& Handle : AppliedEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}
	AppliedEffectHandles.Reset();

	for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}
	GrantedAbilityHandles.Reset();
}

void ABGCharacter::RefreshCharacterLooseTag()
{
	if (!AbilitySystemComponent || AppliedLooseTag == CharacterTag)
	{
		return;
	}

	if (AppliedLooseTag.IsValid())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(AppliedLooseTag);
	}
	if (CharacterTag.IsValid())
	{
		AbilitySystemComponent->AddLooseGameplayTag(CharacterTag);
	}
	AppliedLooseTag = CharacterTag;
}
