#include "Core/BGPlayerState.h"

#include "Debug/BGCardLog.h"
#include "Card/BGCardType.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "Core/BGCharacter.h"       // OnRep_IsDead 가 폰의 애니메이션 상태를 갱신한다
#include "BGGameplayTags.h"
#include "GAS/BGAttributeSet.h"
#include "GAS/Effect/GE_BGDamage.h"
#include "Lobby/BGReadyComponent.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ABGPlayerState::ABGPlayerState()
{
	// 로비 표시가 즉시 따라오도록 기본값(2Hz)보다 자주 보낸다.
	SetNetUpdateFrequency(10.f);

	Ready = CreateDefaultSubobject<UBGReadyComponent>(TEXT("Ready"));
}

void ABGPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABGPlayerState, Hand, COND_OwnerOnly);
	DOREPLIFETIME(ABGPlayerState, SeatIndex);
	DOREPLIFETIME(ABGPlayerState, HandCount);
	DOREPLIFETIME(ABGPlayerState, Equipment);
	DOREPLIFETIME(ABGPlayerState, bIsDead);
	DOREPLIFETIME(ABGPlayerState, PublicRoleTag);
	DOREPLIFETIME(ABGPlayerState, CardsUsedThisTurn);
	DOREPLIFETIME(ABGPlayerState, PendingStatuses);
	DOREPLIFETIME(ABGPlayerState, bDisconnected);
	DOREPLIFETIME(ABGPlayerState, RopeStrikes);
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  이탈과 복귀
// ══════════════════════════════════════════════════════════════════════════════════════

FOnBGDisconnectChanged        ABGPlayerState::OnAnyDisconnectChanged;
FOnBGSeatPublicInfoChanged    ABGPlayerState::OnAnySeatPublicInfoChanged;

void ABGPlayerState::OnRep_Disconnected()
{
	OnAnyDisconnectChanged.Broadcast(SeatIndex, bDisconnected);
}

void ABGPlayerState::SetDisconnected(bool bInDisconnected)
{
	if (!HasAuthority() || bDisconnected == bInDisconnected)
	{
		return;
	}

	bDisconnected = bInDisconnected;

	//	리슨서버 호스트는 자기 OnRep 을 받지 못한다 — 서버에서도 직접 알린다.
	OnAnyDisconnectChanged.Broadcast(SeatIndex, bDisconnected);

	//	나갈 때도 돌아올 때도 0 이다. 이 값은 "자리를 비운 채 넘긴 연속 턴" 을 세므로,
	//	잠수로 쌓인 기록을 이월하면 방금 튕긴 사람이 한 턴 만에 죽는다.
	RopeStrikes = 0;
}

int32 ABGPlayerState::AddRopeStrike()
{
	if (HasAuthority())
	{
		++RopeStrikes;
	}
	return RopeStrikes;
}

void ABGPlayerState::ResetRopeStrikes()
{
	if (HasAuthority())
	{
		RopeStrikes = 0;
	}
}

void ABGPlayerState::SaveVitals(APawn* LeavingPawn)
{
	if (!HasAuthority())
	{
		return;
	}

	//	폰은 파괴되지 않고 UnPossess 만 된다 — 놓치면 복귀 때 같은 자리에 하나 더 생긴다.
	AbandonedPawn = LeavingPawn;

	const UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(LeavingPawn);
	if (!ASC)
	{
		return;
	}

	SavedHealth = ASC->GetNumericAttribute(UBGAttributeSet::GetHealthAttribute());
}

void ABGPlayerState::RestoreVitals()
{
	UAbilitySystemComponent* ASC = GetOwnedAbilitySystemComponent();
	if (!HasAuthority() || !ASC || SavedHealth <= 0.f)
	{
		return;
	}

	//	장비 GE 를 먼저 붙인다 — MaxHealth 를 바꾸는 장비가 있으면 체력 계산이 달라진다.
	const UGameInstance* GI = GetGameInstance();
	const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	int32 Reapplied = 0;

	for (const FBGCardInstance& Card : Equipment)
	{
		const FBGCardRow* Row = Data ? Data->GetCardRow(Card.CardId) : nullptr;
		if (!Row || !Row->EquipEffect)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Row->EquipEffect, 1.f, Context);
		if (Spec.IsValid())
		{
			EquipEffectHandles.Add(Card.CardId, ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
			++Reapplied;
		}
	}

	//	새 폰은 만피로 시작한다. 적어 둔 값까지 깎아 내린다(회복은 하지 않는다).
	const float Current = ASC->GetNumericAttribute(UBGAttributeSet::GetHealthAttribute());
	const float Delta   = Current - SavedHealth;
	if (Delta > 0.f)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec =
			ASC->MakeOutgoingSpec(UGE_BGDamage::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(BG::SetByCaller::Damage.GetTag(), -Delta);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	UE_LOG(LogBGCard, Log, TEXT("[Vitals] 좌석 %d 복원 — 체력 %.0f, 장비 GE %d개"),
		SeatIndex, SavedHealth, Reapplied);

	SavedHealth = 0.f;
}

void ABGPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	ABGPlayerState* To = Cast<ABGPlayerState>(NewPlayerState);
	if (!To)
	{
		return;
	}

	//	판 상태를 통째로 넘긴다. 하나라도 빠지면 돌아온 사람이 "빈손의 무직" 이 된다.
	To->SeatIndex         = SeatIndex;
	To->Hand              = Hand;
	To->HandCount         = HandCount;
	To->Equipment         = Equipment;
	To->bIsDead           = bIsDead;
	To->PublicRoleTag     = PublicRoleTag;
	To->CardsUsedThisTurn = CardsUsedThisTurn;
	To->PendingStatuses   = PendingStatuses;
	To->CachedRoleTag     = CachedRoleTag;
	To->CachedJobTag      = CachedJobTag;
	To->PlayerKey         = PlayerKey;
	To->SavedHealth       = SavedHealth;
	To->AbandonedPawn     = AbandonedPawn;

	//	EquipEffectHandles 는 넘기지 않는다 — 옛 폰의 ASC 를 가리켜 새 ASC 에서는 무효다.
	//	장비 GE 는 폰을 다시 물린 뒤 재적용해야 한다.
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  "다음 1회" 대기 상태
// ══════════════════════════════════════════════════════════════════════════════════════

bool ABGPlayerState::HasPendingStatus(FGameplayTag StatusTag) const
{
	if (!StatusTag.IsValid())
	{
		return false;
	}
	return PendingStatuses.ContainsByPredicate(
		[StatusTag](const FBGPendingStatus& S) { return S.Tag == StatusTag; });
}

int32 ABGPlayerState::GetPendingStatusAmount(FGameplayTag StatusTag) const
{
	for (const FBGPendingStatus& S : PendingStatuses)
	{
		if (S.Tag == StatusTag)
		{
			return S.Amount;
		}
	}
	return 0;
}

bool ABGPlayerState::AddPendingStatus(FGameplayTag StatusTag, int32 Amount)
{
	if (!HasAuthority() || !StatusTag.IsValid())
	{
		return false;
	}

	//	중첩 금지. 같은 상태를 두 번 걸 수 없다 — 사용 가능 여부는 HandlePlayCard 가
	//	카드를 소비하기 "전에" 미리 검사하므로, 여기까지 오면 정상적으로는 중복이 없다.
	if (HasPendingStatus(StatusTag))
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[Status] 좌석 %d 은 이미 %s 를 갖고 있습니다 — 중첩하지 않습니다."),
			SeatIndex, *StatusTag.ToString());
		return false;
	}

	FBGPendingStatus& Added = PendingStatuses.AddDefaulted_GetRef();
	Added.Tag    = StatusTag;
	Added.Amount = Amount;

	UE_LOG(LogBGCard, Log, TEXT("[Status] 좌석 %d 에 %s (수치 %d) 부여"),
		SeatIndex, *StatusTag.ToString(), Amount);
	return true;
}

bool ABGPlayerState::ConsumePendingStatus(FGameplayTag StatusTag, int32& OutAmount)
{
	OutAmount = 0;
	if (!HasAuthority() || !StatusTag.IsValid())
	{
		return false;
	}

	const int32 Index = PendingStatuses.IndexOfByPredicate(
		[StatusTag](const FBGPendingStatus& S) { return S.Tag == StatusTag; });
	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutAmount = PendingStatuses[Index].Amount;
	PendingStatuses.RemoveAt(Index);

	UE_LOG(LogBGCard, Log, TEXT("[Status] 좌석 %d 의 %s 소모 (수치 %d)"),
		SeatIndex, *StatusTag.ToString(), OutAmount);
	return true;
}

void ABGPlayerState::ClearPendingStatuses()
{
	if (HasAuthority())
	{
		PendingStatuses.Reset();
	}
}

void ABGPlayerState::ResetCardsUsedThisTurn()
{
	if (HasAuthority())
	{
		CardsUsedThisTurn = 0;
	}
}

void ABGPlayerState::IncrementCardsUsedThisTurn()
{
	if (HasAuthority())
	{
		++CardsUsedThisTurn;
	}
}

int32 ABGPlayerState::DecrementCardsUsedThisTurn(int32 Count)
{
	if (!HasAuthority() || Count <= 0)
	{
		return 0;
	}

	//	0 미만으로 내려가면 한도를 넘는 추가 사용이 되어 버린다.
	//	"회복" 은 이번 턴에 쓴 만큼까지만으로 해석한다(GDD §14 미결정 — 확정 시 재검토).
	const int32 Restored = FMath::Min(Count, CardsUsedThisTurn);
	CardsUsedThisTurn -= Restored;
	return Restored;
}

void ABGPlayerState::SetPublicRole(const FGameplayTag& InRole)
{
	if (HasAuthority())
	{
		PublicRoleTag = InRole;
	}
}

void ABGPlayerState::SetDead(bool bInDead)
{
	if (!HasAuthority() || bIsDead == bInDead)
	{
		return;
	}

	bIsDead = bInDead;

	//	리슨서버 호스트는 자기 OnRep 을 받지 못한다 — 값을 바꾼 자리에서 직접 부른다.
	OnRep_IsDead();
}

void ABGPlayerState::OnRep_IsDead()
{
	//	좌석 이름표의 사망 표시.
	BroadcastPublicInfoChanged();

	//	쓰러지는 애니메이션. 폰이 아직 붙지 않았으면 손댈 것이 없다 —
	//	그 폰은 BeginPlay 에서 이 상태를 스스로 읽어 간다.
	if (ABGCharacter* Char = Cast<ABGCharacter>(GetPawn()))
	{
		Char->RefreshAnimState();
	}
}

void ABGPlayerState::TakeAllCards(TArray<FBGCardInstance>& OutCards)
{
	OutCards.Reset();
	if (!HasAuthority())
	{
		return;
	}

	OutCards.Append(Hand);
	OutCards.Append(Equipment);

	//	장비를 배열에서만 지우면 GE 가 남아 "장비는 없는데 효과는 계속" 이 된다.
	if (UAbilitySystemComponent* ASC = GetOwnedAbilitySystemComponent())
	{
		for (const TPair<FName, FActiveGameplayEffectHandle>& Pair : EquipEffectHandles)
		{
			if (Pair.Value.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(Pair.Value);
			}
		}
	}
	EquipEffectHandles.Reset();

	Hand.Reset();
	Equipment.Reset();
	HandCount = 0;

	OnHandChanged.Broadcast();          //	서버(리슨 호스트) 자신의 UI 갱신용
	BroadcastPublicInfoChanged();       //	남의 이름표에 보이는 장수도 같이 바뀌었다
}

void ABGPlayerState::SetSeatIndex(int32 InSeat)
{
	if (HasAuthority())
	{
		SeatIndex = InSeat;
	}
}

void ABGPlayerState::AddCardToHand(const FBGCardInstance& Card)
{
	if (!HasAuthority())
	{
		return;
	}
	Hand.Add(Card);
	//	Hand 는 COND_OwnerOnly 라 남에게 복제되지 않는다. Hand 를 건드릴 때마다 반드시 함께 갱신한다.
	HandCount = Hand.Num();

	//	서버에는 OnRep 이 오지 않는다 — 리슨서버 호스트의 UI 를 위해 여기서 직접 알린다.
	OnHandChanged.Broadcast();
	BroadcastPublicInfoChanged();
}

void ABGPlayerState::OnRep_Hand()
{
	UE_LOG(LogBGCard, Verbose, TEXT("[PlayerState] OnRep_Hand — 좌석 %d, %d 장"), SeatIndex, Hand.Num());
	OnHandChanged.Broadcast();
}

void ABGPlayerState::OnRep_PublicInfo()
{
	BroadcastPublicInfoChanged();
}

void ABGPlayerState::BroadcastPublicInfoChanged() const
{
	if (SeatIndex != INDEX_NONE)
	{
		OnAnySeatPublicInfoChanged.Broadcast(SeatIndex);
	}
}

bool ABGPlayerState::RemoveCardFromHand(int32 InstanceId, FBGCardInstance& OutRemoved)
{
	if (!HasAuthority())
	{
		return false;
	}
	for (int32 i = 0; i < Hand.Num(); i++)
	{
		if (Hand[i].InstanceId == InstanceId)
		{
			OutRemoved = Hand[i];
			Hand.RemoveAt(i);
			HandCount = Hand.Num();
			OnHandChanged.Broadcast();          //	서버(리슨 호스트) 자신의 UI 갱신용
	BroadcastPublicInfoChanged();       //	남의 이름표에 보이는 장수도 같이 바뀌었다
			return true;
		}
	}
	return false;
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  장비(파란 카드)
// ══════════════════════════════════════════════════════════════════════════════════════

UAbilitySystemComponent* ABGPlayerState::GetOwnedAbilitySystemComponent() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn());
}

bool ABGPlayerState::HasEquippedCard(FName CardId) const
{
	return Equipment.ContainsByPredicate(
		[CardId](const FBGCardInstance& C) { return C.CardId == CardId; });
}

bool ABGPlayerState::EquipCard(const FBGCardInstance& Card, TSubclassOf<UGameplayEffect> EquipEffect)
{
	if (!HasAuthority() || !Card.IsValid())
	{
		return false;
	}

	//	중복 장착 금지 — 같은 장비를 두 번 붙이면 효과가 두 배가 된다.
	if (HasEquippedCard(Card.CardId))
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Equip] 좌석 %d 은 이미 %s 를 장착 중입니다."),
			SeatIndex, *Card.CardId.ToString());
		return false;
	}

	UAbilitySystemComponent* ASC = GetOwnedAbilitySystemComponent();

	//	EquipEffect 가 비어 있어도 장착은 허용한다 — 아직 GE 를 만들지 않은 장비를
	//	데이터로 먼저 넣어보는 경우가 있다. 다만 효과가 없다는 것은 남긴다.
	if (EquipEffect && ASC)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EquipEffect, 1.f, Context);
		if (Spec.IsValid())
		{
			const FActiveGameplayEffectHandle Applied = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			if (Applied.IsValid())
			{
				EquipEffectHandles.Add(Card.CardId, Applied);
			}
			else
			{
				//	Instant GE 는 유효 핸들을 주지 않는다 — 장비는 Infinite 여야 한다.
				UE_LOG(LogBGCard, Warning,
					TEXT("[Equip] %s 의 EquipEffect 가 지속 효과가 아닙니다(Infinite 인지 확인). 해제 시 되돌릴 수 없습니다."),
					*Card.CardId.ToString());
			}
		}
	}
	else if (!EquipEffect)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Equip] %s 에 EquipEffect 가 없습니다 — 효과 없이 장착만 됩니다."),
			*Card.CardId.ToString());
	}

	Equipment.Add(Card);
	BroadcastPublicInfoChanged();

	UE_LOG(LogBGCard, Log, TEXT("[Equip] 좌석 %d 이 %s#%d 장착 (장비 %d개)"),
		SeatIndex, *Card.CardId.ToString(), Card.InstanceId, Equipment.Num());
	return true;
}

bool ABGPlayerState::UnequipCard(FName CardId, FBGCardInstance& OutRemoved)
{
	if (!HasAuthority())
	{
		return false;
	}

	const int32 Index = Equipment.IndexOfByPredicate(
		[CardId](const FBGCardInstance& C) { return C.CardId == CardId; });
	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutRemoved = Equipment[Index];
	Equipment.RemoveAt(Index);
	BroadcastPublicInfoChanged();

	//	GE 를 함께 걷어낸다 — 배열만 지우면 효과가 영구히 남는다.
	if (FActiveGameplayEffectHandle* Handle = EquipEffectHandles.Find(CardId))
	{
		if (Handle->IsValid())
		{
			if (UAbilitySystemComponent* ASC = GetOwnedAbilitySystemComponent())
			{
				ASC->RemoveActiveGameplayEffect(*Handle);
			}
		}
		EquipEffectHandles.Remove(CardId);
	}

	UE_LOG(LogBGCard, Log, TEXT("[Equip] 좌석 %d 이 %s 해제"), SeatIndex, *CardId.ToString());
	return true;
}
