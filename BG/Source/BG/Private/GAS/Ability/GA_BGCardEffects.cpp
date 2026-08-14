#include "GAS/Ability/GA_BGCardEffects.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Debug/BGCardLog.h"
#include "Card/BGCardType.h"
#include "Card/BGDataSubsystem.h"
#include "Debug/BGDebug.h"
#include "Card/BGDiceComponent.h"
#include "Core/BGGameState.h"
#include "BGGameplayTags.h"
#include "Core/BGPlayerState.h"
#include "GAS/BGAttributeSet.h"
#include "GAS/Effect/GE_BGDamage.h"
#include "GAS/Effect/GE_BGHeal.h"

UGA_BGCardEffects::UGA_BGCardEffects()
{
	ActivationBlockedTags.AddTag(BG::State::Dead.GetTag());
}

namespace
{
	/** 페이로드에 실린 Card.Id.* 태그를 찾는다. 어느 카드가 이 GA 를 발동했는지 알아내는 용도. */
	FGameplayTag FindCardIdTag(const FGameplayTagContainer& Tags)
	{
		static const FGameplayTag CardIdRoot = FGameplayTag::RequestGameplayTag(TEXT("Card.Id"));
		for (const FGameplayTag& Tag : Tags)
		{
			if (Tag.MatchesTag(CardIdRoot) && Tag != CardIdRoot)
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}
}

void UGA_BGCardEffects::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (!TriggerEventData)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[BG] GA_BGCardEffects: 페이로드가 없습니다 — 카드 경로로 발동해야 합니다."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ── 어느 카드인가 ──
	const FGameplayTag CardIdTag = FindCardIdTag(TriggerEventData->InstigatorTags);
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	const FBGCardRow* Row = (Data && CardIdTag.IsValid()) ? Data->GetCardRow(CardIdTag) : nullptr;

	if (!Row)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[BG] GA_BGCardEffects: 카드 Row 를 찾지 못했습니다 (CardId 태그=%s). "
			     "HandlePlayCard 가 Card.Id 태그를 실어 보내는지 확인하세요."),
			CardIdTag.IsValid() ? *CardIdTag.ToString() : TEXT("없음"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ── 판정 등급 수신 (판정은 서버가 이미 끝냈다 — GDD §9.1) ──
	const EBGDiceOutcome Outcome =
		UBGDiceComponent::TagsToOutcome(TriggerEventData->InstigatorTags, EBGDiceOutcome::Success);

	const TArray<FBGCardEffect>& Effects = Row->OutcomeEffects.ForOutcome(Outcome);

	AActor* TargetActor = const_cast<AActor*>(TriggerEventData->Target.Get());

	TArray<FString> Summary;
	ExecuteEffects(Effects, Avatar, TargetActor, SourceASC, Summary);

	const FString SummaryText = Summary.IsEmpty() ? TEXT("효과 없음") : FString::Join(Summary, TEXT(" + "));

	BGDebug::Screen(
		FString::Printf(TEXT("%s  %s → [%s] %s"),
			*Row->DisplayName.ToString(),
			Avatar ? *Avatar->GetName() : TEXT("?"),
			*UBGDiceComponent::GetOutcomeText(Outcome).ToString(),
			*SummaryText),
		Summary.IsEmpty() ? FColor(160, 160, 160) : FColor(120, 220, 255), /*Time=*/6.f);

	OnCardEffectsResolved(Outcome, SummaryText, TargetActor);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_BGCardEffects::ExecuteEffects(
	const TArray<FBGCardEffect>& Effects,
	AActor* Avatar,
	AActor* TargetActor,
	UAbilitySystemComponent* SourceASC,
	TArray<FString>& OutSummary)
{
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	ABGGameState* GS = World ? World->GetGameState<ABGGameState>() : nullptr;

	ABGPlayerState* SelfPS   = GetBGPlayerState(Avatar);
	ABGPlayerState* TargetPS = GetBGPlayerState(TargetActor);

	//	목록 순서대로 실행한다. 순서가 결과를 바꿀 수 있으므로(뽑기 → 한도 회복 등)
	//	DT 에 적힌 순서를 그대로 지킨다.
	for (const FBGCardEffect& Effect : Effects)
	{
		//	수치 0 은 "이 등급에서는 이 효과 없음" 으로 본다.
		//	단 ApplyStatus 는 예외다 — 강제 판정 상태는 수치를 쓰지 않으므로 0 이 정상이다.
		if (Effect.Amount <= 0 && Effect.Op != EBGCardEffectOp::ApplyStatus)
		{
			continue;
		}

		FString Line;
		switch (Effect.Op)
		{
		case EBGCardEffectOp::DamageTarget:
			Line = ApplyDamage(Effect.Amount, Avatar, TargetActor, SourceASC);
			break;

		case EBGCardEffectOp::HealSelf:
			Line = ApplyHeal(Effect.Amount, SourceASC);
			break;

		case EBGCardEffectOp::DrawSelf:
			Line = ApplyDraw(Effect.Amount, GS, SelfPS);
			break;

		case EBGCardEffectOp::DiscardTargetRandom:
			Line = ApplyTakeFromTarget(Effect.Amount, /*bSteal=*/false, /*bFromEquipment=*/false, GS, SelfPS, TargetPS, Avatar);
			break;

		case EBGCardEffectOp::StealTargetRandom:
			Line = ApplyTakeFromTarget(Effect.Amount, /*bSteal=*/true, /*bFromEquipment=*/false, GS, SelfPS, TargetPS, Avatar);
			break;

		case EBGCardEffectOp::DiscardTargetEquipRandom:
			Line = ApplyTakeFromTarget(Effect.Amount, /*bSteal=*/false, /*bFromEquipment=*/true, GS, SelfPS, TargetPS, Avatar);
			break;

		case EBGCardEffectOp::StealTargetEquipRandom:
			Line = ApplyTakeFromTarget(Effect.Amount, /*bSteal=*/true, /*bFromEquipment=*/true, GS, SelfPS, TargetPS, Avatar);
			break;

		case EBGCardEffectOp::RestoreCardUse:
			Line = ApplyRestoreCardUse(Effect.Amount, SelfPS);
			break;

		case EBGCardEffectOp::ApplyStatus:
			Line = ApplyStatusEffect(Effect, SelfPS, TargetPS);
			break;

		default:
			break;
		}

		if (!Line.IsEmpty())
		{
			OutSummary.Add(Line);
		}
	}
}

FString UGA_BGCardEffects::ApplyDamage(int32 Amount, AActor* Avatar, AActor* TargetActor, UAbilitySystemComponent* SourceASC)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!SourceASC || !TargetASC)
	{
		//	조용히 넘어가면 "판정도 성공, 카드도 소비, 피해만 0" 이 되어 원인을 짚을 수 없다.
		//	§13.2 가 기록한 뱅 피해 미적용 사고가 정확히 이 모양이었다.
		UE_LOG(LogBGCard, Warning,
			TEXT("[피해] ASC 없음 — 시전자 %s / 대상 %s (대상 액터=%s). 피해 %d 가 유실된다."),
			SourceASC ? TEXT("O") : TEXT("X"),
			TargetASC ? TEXT("O") : TEXT("X"),
			TargetActor ? *TargetActor->GetName() : TEXT("없음"),
			Amount);
		return FString();
	}

	//	피해는 카드 데이터가 전부 결정한다.
	//
	//	[비활성] 힘 배율 / 대상 지능 경감은 쓰지 않는다 (GDD §7.1 / §13.1).
	//	  §7.1 에 "공격 보정" 이 확정 스탯으로 있지만 그 자리를 채울 어트리뷰트는 아직 없고,
	//	  힘·지능은 그 축을 RPG 능력치로 다시 표현한 별개 계층이라 되살릴 대상이 아니다.
	//	  실제로 지능 1(= round(1 × 0.5) = 1)이 피해 1 을 통째로 상쇄해
	//	  "성공했는데 피해 0" 이 되는 문제가 있었다 — 카드 수치가 곧 피해여야
	//	  §10 확률·효과 표시와 §11 밸런스 지표가 성립한다.
	//	  StrengthDamageMult / IntelligenceMitigation 프로퍼티는 선언만 남는다.
	//
	//	약점 포착(Status.NextAttack.DamageBonus)은 스탯이 아니라 카드 효과이므로 유지한다.
	int32 Final = Amount;
	int32 Bonus = 0;
	if (ABGPlayerState* SelfPS = GetBGPlayerState(Avatar))
	{
		SelfPS->ConsumePendingStatus(BG::Status::NextAttack::DamageBonus.GetTag(), Bonus);
		Final += Bonus;
	}

	//	마법사(DamageBonusChance) — 확률로 피해가 더 들어간다.
	//	방어 판정(요정 갑옷)과 같은 방식이지만 **주는 쪽**이라 여기서 먼저 굴린다.
	//	무효화되면 이 보너스도 함께 사라지는 것이 맞다 — 막힌 공격에 위력을 논할 이유가 없다.
	const float BonusChance = SourceASC->GetNumericAttribute(
		UBGAttributeSet::GetDamageBonusChanceAttribute());

	int32 CritBonus = 0;
	if (BonusChance > 0.f)
	{
		UBGDiceComponent* Dice = UBGDiceComponent::Get(Avatar);
		const int32 BonusRoll = Dice ? Dice->RandomIndex(100) : INDEX_NONE;

		if (BonusRoll != INDEX_NONE && static_cast<float>(BonusRoll) < BonusChance)
		{
			CritBonus = FMath::RoundToInt(
				SourceASC->GetNumericAttribute(UBGAttributeSet::GetDamageBonusAmountAttribute()));
			Final += CritBonus;

			//	GDD §10: 자동 판정은 원인·확률·결과를 로그에 남긴다.
			UE_LOG(LogBGCard, Log,
				TEXT("[추가피해] 발동 — 눈 %d / 확률 %.0f%% → 피해 +%d"),
				BonusRoll, BonusChance, CritBonus);
		}
	}

	UE_LOG(LogBGCard, Log, TEXT("[피해] 카드%d%s%s → 최종 %d"),
		Amount,
		(Bonus > 0)     ? *FString::Printf(TEXT(" + 약점포착%d"), Bonus)   : TEXT(""),
		(CritBonus > 0) ? *FString::Printf(TEXT(" + 추가피해%d"), CritBonus) : TEXT(""),
		Final);

	if (Final <= 0)
	{
		return FString::Printf(TEXT("피해 0 (카드 수치 %d)"), Amount);
	}

	// ── 자동 방어 판정 (요정 갑옷) ────────────────────────────────
	//	GDD §14 가 "자동 방어 판정과 공격 판정의 처리 순서" 를 미결로 두었다.
	//	여기서는 **공격 판정이 성공한 뒤에 방어를 굴린다** 로 정했다:
	//	  · 공격 성공/대성공이 확정돼야 "막을 공격" 이 존재한다
	//	  · 방어를 먼저 굴리면 실패할 공격까지 방어율을 소모해 체감이 흐려진다
	//	순서를 바꾸려면 이 블록을 판정 지점(HandlePlayCard)으로 옮기면 된다.
	//
	//	⚠️ 굴림은 서버에서만 일어난다. 이 GA 는 서버 권위로 발동되므로 여기가 서버다.
	const float NegateChance = TargetASC->GetNumericAttribute(
		UBGAttributeSet::GetDamageNegateChanceAttribute());

	if (NegateChance > 0.f)
	{
		//	덱 셔플과 분리된 판정 스트림 — 같은 시드에서 재현되고 덱 순서를 밀지 않는다.
		UBGDiceComponent* Dice = UBGDiceComponent::Get(Avatar);
		const int32 Roll = Dice ? Dice->RandomIndex(100) : INDEX_NONE;

		if (Roll != INDEX_NONE && static_cast<float>(Roll) < NegateChance)
		{
			//	GDD §10: 자동 판정은 원인·확률·결과를 로그에 남긴다.
			UE_LOG(LogBGCard, Log,
				TEXT("[방어] 요정 갑옷 발동 — 눈 %d / 방어율 %.0f%% → 피해 %d 무효화"),
				Roll, NegateChance, Final);

			return FString::Printf(TEXT("방어 성공 (%.0f%%) — 피해 %d 무효"), NegateChance, Final);
		}

		UE_LOG(LogBGCard, Log,
			TEXT("[방어] 판정 실패 — 눈 %d / 방어율 %.0f%% → 피해 %d 적용"),
			Roll, NegateChance, Final);
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(this);
	Context.AddInstigator(Avatar, Avatar);

	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UGE_BGDamage::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogBGCard, Warning, TEXT("[피해] GE_BGDamage Spec 생성 실패 — 피해 %d 가 유실된다."), Final);
		return FString();
	}

	//	피해는 음수로 넣는다(Health 가산 = 감소).
	Spec.Data->SetSetByCallerMagnitude(BG::SetByCaller::Damage.GetTag(), -static_cast<float>(Final));
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

	return FString::Printf(TEXT("피해 %d"), Final);
}

FString UGA_BGCardEffects::ApplyHeal(int32 Amount, UAbilitySystemComponent* SourceASC)
{
	if (!SourceASC)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[회복] 시전자 ASC 없음 — 회복 %d 가 유실된다."), Amount);
		return FString();
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UGE_BGHeal::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogBGCard, Warning, TEXT("[회복] GE_BGHeal Spec 생성 실패 — 회복 %d 가 유실된다."), Amount);
		return FString();
	}

	//	사제(HealBonus) — 회복할 때마다 더 회복한다. 회복량이 0 인 카드는 대상이 아니다.
	const int32 Bonus = FMath::RoundToInt(
		SourceASC->GetNumericAttribute(UBGAttributeSet::GetHealBonusAttribute()));
	const int32 Final = FMath::Max(0, Amount + Bonus);

	if (Bonus != 0)
	{
		UE_LOG(LogBGCard, Log, TEXT("[회복] 카드%d + 회복보정%d → 최종 %d"), Amount, Bonus, Final);
	}

	//	MaxHealth 클램프는 AttributeSet 이 처리한다.
	Spec.Data->SetSetByCallerMagnitude(BG::SetByCaller::Heal.GetTag(), static_cast<float>(Final));
	SourceASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	return (Bonus != 0)
		? FString::Printf(TEXT("회복 %d (보정 +%d)"), Final, Bonus)
		: FString::Printf(TEXT("회복 %d"), Final);
}

FString UGA_BGCardEffects::ApplyDraw(int32 Amount, ABGGameState* GS, ABGPlayerState* SelfPS)
{
	if (!GS || !SelfPS)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[뽑기] 실행 불가 — GameState %s / 내 PlayerState %s. %d장 뽑기가 유실된다."),
			GS ? TEXT("O") : TEXT("X"), SelfPS ? TEXT("O") : TEXT("X"), Amount);
		return FString();
	}

	//	손패 상한·덱 고갈에 걸리면 덜 뽑히거나 아예 못 뽑는다.
	//
	//	⚠️ 그것은 **카드 사용 실패가 아니다.** 이 함수는 요약 문자열만 돌려주고, 호출부는
	//	   빈 문자열을 "요약할 내용 없음" 으로만 취급한다(ApplyEffects 의 IsEmpty 검사).
	//	   즉 뽑기가 막혀도 GA 는 정상 종료되고, 같은 카드의 다른 효과(속공의 피해 등)와
	//	   카드 소비·사용 횟수 처리는 그대로 일어난다.
	const int32 Drawn = GS->DrawCardsToHand(SelfPS, Amount);

	if (Drawn < Amount)
	{
		//	"카드는 썼는데 왜 안 뽑히지" 를 로그로 바로 알 수 있게 한다.
		return (Drawn > 0)
			? FString::Printf(TEXT("%d/%d장 뽑기 (손패 상한)"), Drawn, Amount)
			: FString::Printf(TEXT("뽑기 없음 (손패 상한 %d장)"), GS->GetHandLimitForSeat(SelfPS->GetSeatIndex()));
	}

	return FString::Printf(TEXT("%d장 뽑기"), Drawn);
}

FString UGA_BGCardEffects::ApplyTakeFromTarget(int32 Amount, bool bSteal, bool bFromEquipment,
	ABGGameState* GS, ABGPlayerState* SelfPS, ABGPlayerState* TargetPS, AActor* Avatar)
{
	UBGDiceComponent* Dice = UBGDiceComponent::Get(Avatar);
	if (!GS || !TargetPS || !Dice)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[탈취] 실행 불가 — GameState %s / 대상 %s / 주사위 %s. 카드는 이미 소비됐다."),
			GS ? TEXT("O") : TEXT("X"), TargetPS ? TEXT("O") : TEXT("X"), Dice ? TEXT("O") : TEXT("X"));
		return FString();
	}

	int32 Moved = 0;
	for (int32 i = 0; i < Amount; ++i)
	{
		//	목록은 매 반복마다 다시 읽는다 — 앞 반복에서 하나가 빠졌다.
		const TArray<FBGCardInstance>& Source =
			bFromEquipment ? TargetPS->GetEquipment() : TargetPS->GetHand();

		if (Source.IsEmpty())
		{
			break;   // 더 가져갈 것이 없다. 실패가 아니라 정상 종료다.
		}

		//	덱 셔플과 분리된 판정 스트림을 쓴다 — 같은 시드에서 재현되고 덱 순서를 밀지 않는다.
		const int32 Index = Dice->RandomIndex(Source.Num());
		if (!Source.IsValidIndex(Index))
		{
			break;
		}

		//	제거 전에 값으로 복사 — 제거하면 위 참조가 흔들린다.
		const FBGCardInstance Picked = Source[Index];

		//	장비는 UnequipCard 로 뺀다. 배열에서 지우는 것과 Infinite GE 를 걷어내는 것을
		//	함께 처리하기 때문이다 — 배열만 지우면 장비는 사라졌는데 확률 보정만 영구히 남는다.
		FBGCardInstance Removed;
		const bool bRemoved = bFromEquipment
			? TargetPS->UnequipCard(Picked.CardId, Removed)
			: TargetPS->RemoveCardFromHand(Picked.InstanceId, Removed);

		if (!bRemoved)
		{
			//	방금 목록에서 고른 것이 안 빠진다면 목록과 제거 경로가 어긋난 것이다.
			UE_LOG(LogBGCard, Warning,
				TEXT("[탈취] %s 제거 실패 — CardId %s / InstanceId %d"),
				bFromEquipment ? TEXT("장비") : TEXT("손패"),
				*Picked.CardId.ToString(), Picked.InstanceId);
			break;
		}

		if (bSteal && SelfPS)
		{
			//	장비여도 장비 칸이 아니라 손패로 들어간다(헤더 주석 참고).
			SelfPS->AddCardToHand(Removed);
		}
		else
		{
			GS->PushToDiscard(Removed);
		}
		++Moved;
	}

	if (Moved == 0)
	{
		return FString();
	}

	return bFromEquipment
		? FString::Printf(TEXT("상대 장비 %d개 %s"), Moved, bSteal ? TEXT("훔침") : TEXT("버림"))
		: FString::Printf(TEXT("상대 %d장 %s"),      Moved, bSteal ? TEXT("훔침") : TEXT("버림"));
}

FString UGA_BGCardEffects::ApplyRestoreCardUse(int32 Amount, ABGPlayerState* SelfPS)
{
	if (!SelfPS)
	{
		return FString();
	}

	//	이 카드 자신의 사용 횟수는 GA 발동 전에 이미 계상돼 있다(HandlePlayCard).
	//	그래서 1 회복하면 "이 카드는 사용 횟수를 쓰지 않은 셈" 이 된다.
	const int32 Restored = SelfPS->DecrementCardsUsedThisTurn(Amount);
	return (Restored > 0) ? FString::Printf(TEXT("사용한도 %d 회복"), Restored) : FString();
}

FString UGA_BGCardEffects::ApplyStatusEffect(const FBGCardEffect& Effect,
	ABGPlayerState* SelfPS, ABGPlayerState* TargetPS)
{
	if (!Effect.StatusTag.IsValid())
	{
		UE_LOG(LogBGCard, Warning, TEXT("[BG] ApplyStatus: StatusTag 가 비어 있습니다(DT 확인)."));
		return FString();
	}

	ABGPlayerState* Recipient = Effect.bToTarget ? TargetPS : SelfPS;
	if (!Recipient)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[BG] ApplyStatus: %s 를 받을 대상이 없습니다."),
			*Effect.StatusTag.ToString());
		return FString();
	}

	//	중첩은 AddPendingStatus 가 거부한다. 여기까지 왔다면 HandlePlayCard 의 사전 검사를
	//	통과했다는 뜻이므로 정상적으로는 실패하지 않는다.
	if (!Recipient->AddPendingStatus(Effect.StatusTag, Effect.Amount))
	{
		return FString();
	}

	return FString::Printf(TEXT("%s%s 부여"),
		Effect.bToTarget ? TEXT("상대에게 ") : TEXT(""),
		*Effect.StatusTag.GetTagName().ToString());
}
