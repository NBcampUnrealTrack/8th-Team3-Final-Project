#include "UI/View/BGCardViewLibrary.h"

#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"   // ArtTexture 동기 로드
#include "Engine/World.h"
#include "Debug/BGCardLog.h"
#include "Card/BGCardType.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGDiceComponent.h"
#include "Card/BGGameDataTypes.h"
#include "BGGameplayTags.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "AbilitySystemGlobals.h"

namespace
{
	const UBGDataSubsystem* GetDataSubsystem(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
			: nullptr;
		const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	}

	//	비율 → 확률(%). 정규화 규칙이 두 곳에 생기지 않도록 판정 컴포넌트의 변환을 그대로 쓴다.
	FBGCardChanceView ToChanceView(const FBGOutcomeWeights& Weights)
	{
		FBGCardChanceView Out;
		UBGDiceComponent::GetOutcomeChances(Weights,
			Out.CriticalFailure, Out.Failure, Out.Success, Out.CriticalSuccess);
		return Out;
	}

	//	보정이 실제로 확률을 바꿨는가. 표시 단위가 % 라 0.05%p 미만은 같은 값으로 본다
	//	(정규화 부동소수 오차로 "보정됨" 표시가 깜빡이는 것을 막는다).
	bool ChancesDiffer(const FBGCardChanceView& A, const FBGCardChanceView& B)
	{
		constexpr float Epsilon = 0.05f;
		return !FMath::IsNearlyEqual(A.CriticalFailure, B.CriticalFailure, Epsilon)
			|| !FMath::IsNearlyEqual(A.Failure,         B.Failure,         Epsilon)
			|| !FMath::IsNearlyEqual(A.Success,         B.Success,         Epsilon)
			|| !FMath::IsNearlyEqual(A.CriticalSuccess, B.CriticalSuccess, Epsilon);
	}

	//	장비(파란 카드)인가. 판정도 대상도 없어 연출 유도가 통째로 갈라지는 지점이라
	//	판별을 한 곳에 둔다.
	bool IsEquipmentRow(const FBGCardRow& Row)
	{
		return Row.TypeTag == BG::Card::Type::Blue.GetTag();
	}

	//	가장 인상이 강한 원형. 이 값이 나오면 더 볼 것 없이 확정이다.
	constexpr int32 StrongestArchetypePriority = 0;

	//	연출로 표현할 것이 없는 효과. 어떤 후보보다도 뒤에 온다.
	constexpr int32 NoArchetypePriority = TNumericLimits<int32>::Max();

	/**
	 * 효과 하나를 연출 원형으로. 우선순위는 "화면에서 가장 크게 보이는 것" 순이다.
	 * 낮은 값일수록 인상이 강하다 — 여러 효과가 섞이면 가장 강한 것이 카드의 성격이 된다.
	 */
	int32 ArchetypePriority(EBGCardFXArchetype Archetype)
	{
		switch (Archetype)
		{
		case EBGCardFXArchetype::Damage:  return StrongestArchetypePriority;   // 피해가 있으면 그게 이 카드의 정체다
		case EBGCardFXArchetype::Steal:   return 1;
		case EBGCardFXArchetype::Discard: return 2;
		case EBGCardFXArchetype::Heal:    return 3;
		case EBGCardFXArchetype::Status:  return 4;
		case EBGCardFXArchetype::Draw:    return 5;   // 부수적으로 딸려오는 경우가 많다
		default:                          return NoArchetypePriority;
		}
	}

	EBGCardFXArchetype OpToArchetype(EBGCardEffectOp Op)
	{
		switch (Op)
		{
		case EBGCardEffectOp::DamageTarget:        return EBGCardFXArchetype::Damage;
		case EBGCardEffectOp::HealSelf:            return EBGCardFXArchetype::Heal;
		case EBGCardEffectOp::DrawSelf:            return EBGCardFXArchetype::Draw;
		case EBGCardEffectOp::RestoreCardUse:      return EBGCardFXArchetype::Draw;
		case EBGCardEffectOp::StealTargetRandom:   return EBGCardFXArchetype::Steal;
		case EBGCardEffectOp::DiscardTargetRandom: return EBGCardFXArchetype::Discard;
		case EBGCardEffectOp::ApplyStatus:         return EBGCardFXArchetype::Status;

		//	장비를 겨냥한 것도 손패를 겨냥한 것과 같은 원형으로 본다.
		//	"뭘 훔쳤나" 는 대상 자리의 연출이 말해 주고, 카드 쪽 연출은 같아도 읽힌다.
		case EBGCardEffectOp::StealTargetEquipRandom:   return EBGCardFXArchetype::Steal;
		case EBGCardEffectOp::DiscardTargetEquipRandom: return EBGCardFXArchetype::Discard;

		default:                                   return EBGCardFXArchetype::None;
		}
	}

	/**
	 * 연출이 어디서 터지는가. 피해는 맞는 쪽, 회복은 자기 자리다 —
	 * 전부 카드 자리에서 터지면 무엇이 누구에게 일어났는지 읽히지 않는다.
	 */
	EBGCardFXTargetKind OpToTargetKind(const FBGCardEffect& Effect)
	{
		switch (Effect.Op)
		{
		case EBGCardEffectOp::DamageTarget:
		case EBGCardEffectOp::StealTargetRandom:
		case EBGCardEffectOp::DiscardTargetRandom:
		case EBGCardEffectOp::StealTargetEquipRandom:
		case EBGCardEffectOp::DiscardTargetEquipRandom:
			return EBGCardFXTargetKind::Target;

		case EBGCardEffectOp::DrawSelf:
			return EBGCardFXTargetKind::Deck;   // 덱에서 온다

		case EBGCardEffectOp::ApplyStatus:
			//	상태는 자기 강화일 수도, 상대 방해(함정)일 수도 있다.
			//	DT 에 이미 방향이 적혀 있으므로 추측하지 않는다.
			return Effect.bToTarget ? EBGCardFXTargetKind::Target : EBGCardFXTargetKind::Caster;

		default:
			return EBGCardFXTargetKind::Caster;
		}
	}

	//	UI 등급 → 게임 로직 등급. 변환이 두 곳에 생기지 않도록 여기서만 한다.
	EBGDiceOutcome TierToOutcome(EBGResolutionTier Tier)
	{
		switch (Tier)
		{
		case EBGResolutionTier::CriticalSuccess: return EBGDiceOutcome::CriticalSuccess;
		case EBGResolutionTier::Success:         return EBGDiceOutcome::Success;
		case EBGResolutionTier::Failure:         return EBGDiceOutcome::Failure;
		default:                                 return EBGDiceOutcome::CriticalFailure;
		}
	}

	//	⚠️ EBGDiceOutcome 에 등급을 추가하면 여기도 넣을 것 — 빠뜨리면 그 등급의 효과가
	//	   원형 유도에서 통째로 누락된다(컴파일은 통과한다).
	constexpr EBGDiceOutcome AllOutcomes[] = {
		EBGDiceOutcome::CriticalFailure, EBGDiceOutcome::Failure,
		EBGDiceOutcome::Success,         EBGDiceOutcome::CriticalSuccess };

	/**
	 * 카드 데이터에서 연출 원형을 유도한다.
	 *
	 * DT 에 연출용 컬럼을 따로 두지 않는 이유: 두 곳에 적으면 카드 효과를 바꿨을 때
	 * 연출만 옛것으로 남는다(피해를 없앤 카드가 계속 공격 연출로 나가는 식).
	 * 효과 목록이 곧 성격이므로 거기서 뽑아 쓴다.
	 *
	 * 여러 효과가 섞이면 가장 인상이 강한 것을 고른다 — 피해 > 탈취 > 파괴 > 회복 > 상태 > 수급.
	 */
	EBGCardFXArchetype DeriveFXArchetype(const FBGCardRow& Row)
	{
		//	장비는 판정도 대상도 없다 — 효과 목록을 볼 것 없이 장착 연출이다.
		if (IsEquipmentRow(Row))
		{
			return EBGCardFXArchetype::Equip;
		}

		//	등급마다 효과가 다르므로 네 등급을 전부 훑는다. 대성공에만 피해가 붙는 카드도
		//	"공격 카드" 로 읽혀야 한다 — 성공했을 때만 공격처럼 보이면 일관성이 깨진다.
		EBGCardFXArchetype Best = EBGCardFXArchetype::None;
		int32 BestPriority = NoArchetypePriority;

		for (EBGDiceOutcome Outcome : AllOutcomes)
		{
			for (const FBGCardEffect& Effect : Row.OutcomeEffects.ForOutcome(Outcome))
			{
				const EBGCardFXArchetype Candidate = OpToArchetype(Effect.Op);
				const int32 Priority = ArchetypePriority(Candidate);
				if (Priority < BestPriority)
				{
					BestPriority = Priority;
					Best = Candidate;

					if (BestPriority == StrongestArchetypePriority)
					{
						return Best;   // 이보다 강한 후보는 없다
					}
				}
			}
		}

		return Best;
	}

	//	연출로 표현할 값이 있는 효과인가.
	//	수치 0 은 "이 등급에서는 효과 없음" 이다 — 실행 쪽(GA_BGCardEffects)과 같은 규칙을 쓴다.
	//	ApplyStatus 만 예외: 강제 판정 상태는 수치를 쓰지 않으므로 0 이 정상이다.
	bool HasVisibleAmount(const FBGCardEffect& Effect)
	{
		return Effect.Amount > 0 || Effect.Op == EBGCardEffectOp::ApplyStatus;
	}
}

TArray<FBGCardFXStep> UBGCardViewLibrary::MakeFXSteps(const FBGCardRow& Row, EBGResolutionTier Tier)
{
	TArray<FBGCardFXStep> Steps;

	//	장비는 판정을 거치지 않는다. 등급으로 목록을 고를 수 없으므로 장착 연출 하나로 끝낸다.
	if (IsEquipmentRow(Row))
	{
		FBGCardFXStep& Equip = Steps.AddDefaulted_GetRef();
		Equip.Op         = EBGCardFXArchetype::Equip;
		Equip.Amount     = 1;
		Equip.TargetKind = EBGCardFXTargetKind::Caster;
		return Steps;
	}

	//	등급이 없으면(None) 고를 목록이 없다. 빈 배열이 정상이다 — BP 는 아무것도 재생하지 않는다.
	if (Tier == EBGResolutionTier::None)
	{
		return Steps;
	}

	//	⚠️ DT 에 적힌 순서를 그대로 지킨다. 순서가 연출의 인과를 만든다
	//	   ("뽑고 나서 때린다" 와 "때리고 나서 뽑는다" 는 다르게 읽힌다).
	//	   게임플레이 실행 순서(GA_BGCardEffects::ExecuteEffects)와도 같아야 한다.
	const TArray<FBGCardEffect>& Fired = Row.OutcomeEffects.ForOutcome(TierToOutcome(Tier));
	Steps.Reserve(Fired.Num());

	for (const FBGCardEffect& Effect : Fired)
	{
		const EBGCardFXArchetype Op = OpToArchetype(Effect.Op);
		if (Op == EBGCardFXArchetype::None || !HasVisibleAmount(Effect))
		{
			continue;   // 연출로 표현할 것이 없는 효과
		}

		FBGCardFXStep& Step = Steps.AddDefaulted_GetRef();
		Step.Op         = Op;
		Step.Amount     = Effect.Amount;
		Step.TargetKind = OpToTargetKind(Effect);
	}

	return Steps;
}

FBGCardView UBGCardViewLibrary::MakeCardView(const UObject* WorldContextObject, const FBGCardInstance& Card,
	const UAbilitySystemComponent* SourceASC)
{
	FBGCardView View;
	View.InstanceId = Card.InstanceId;
	View.CardId     = Card.CardId;

	const UBGDataSubsystem* Data = GetDataSubsystem(WorldContextObject);
	const FBGCardRow* Row = Data ? Data->GetCardRow(Card.CardId) : nullptr;

	if (!Row)
	{
		// 조용히 빈 카드가 되면 "왜 이름이 안 보이지" 로 시간을 버린다. 화면에 원인을 띄운다.
		View.DisplayName = FText::FromString(FString::Printf(TEXT("%s (Row 없음)"), *Card.CardId.ToString()));
		View.Description = FText::FromString(TEXT("DT_BGCard 에 이 CardId 의 Row 가 없습니다."));
		View.bPlayable   = false;
		UE_LOG(LogBGCard, Warning, TEXT("MakeCardView: '%s' Row 를 찾지 못했습니다."), *Card.CardId.ToString());
		return View;
	}

	View.DisplayName  = Row->DisplayName;
	View.Description  = Row->Description;
	View.bNeedsTarget = Row->TraitTags.HasTag(BG::Card::Trait::NeedsTargetSeat.GetTag());
	View.FXArchetype  = DeriveFXArchetype(*Row);

	// ── 카드 그림 ────────────────────────────────────────────────────────
	//	여기서 동기 로드한다. 뷰는 손패에 실제로 들어온 카드에 대해서만 만들어지므로
	//	한 번에 로드되는 것은 많아야 손패 장수(상한 10장)다 — DT 를 하드 참조로 바꿔
	//	전체 카드 그림을 상시 물고 있는 것보다 이쪽이 싸다.
	//
	//	⚠️ 손패가 바뀔 때만 호출되는 경로다(MakeHandViews). 매 프레임 도는 곳에서 부르면
	//	   첫 로드마다 히치가 생긴다 — 그때는 비동기 로드로 바꿀 것.
	if (!Row->ArtTexture.IsNull())
	{
		View.ArtTexture = Row->ArtTexture.LoadSynchronous();

		if (!View.ArtTexture)
		{
			//	경로는 있는데 로드가 실패했다 — 애셋이 삭제·이동됐다는 뜻이다.
			UE_LOG(LogBGCard, Warning,
				TEXT("MakeCardView: '%s' 의 ArtTexture 로드 실패 (%s). 애셋이 지워졌거나 옮겨졌습니다."),
				*Card.CardId.ToString(), *Row->ArtTexture.ToString());
		}
	}
	else
	{
		//	DT 에 아직 그림을 안 넣은 카드. 위젯이 폴백을 그리므로 게임은 정상 진행된다.
		//	Verbose 로 두는 이유: 아트 작업 중에는 빈 카드가 정상 상태라 Warning 이면 로그가 묻힌다.
		UE_LOG(LogBGCard, Verbose,
			TEXT("MakeCardView: '%s' 에 ArtTexture 가 비어 있습니다 — 위젯 폴백으로 그립니다."),
			*Card.CardId.ToString());
	}

	// TODO(5.x): 사거리·턴당 BANG 한도·현재 페이즈를 반영한 실제 판정으로 교체.
	//            어디까지나 표시용 힌트이고, 최종 판정은 언제나 서버가 한다.
	View.bPlayable = true;

	// ── 4단계 확률 (GDD §4.4 / §6.1 / §8 / §10) ──────────────────────────
	//	파란(장비) 카드는 판정 없이 장착 경로로 빠진다 — 판별자는 카드를 실제로 내는 쪽
	//	(ABGPlayerController::ServerRequestPlayCard)과 같은 TypeTag 를 쓴다. 두 곳이 갈리면
	//	확률이 표시되는데 판정은 일어나지 않는 카드가 생긴다.
	const bool bIsEquip = (Row->TypeTag == BG::Card::Type::Blue.GetTag());

	if (!bIsEquip && Row->OutcomeWeights.Sum() > 0)
	{
		View.bHasChances = true;
		View.BaseChance  = ToChanceView(Row->OutcomeWeights);

		//	보정 합산은 순수 계산이라 클라에서 돌려도 안전하다.
		//	GDD §9.2 — 확률 "표시" 는 클라, 확률 "판정" 은 서버. 여기는 표시다.
		//	⚠️ 공격 여부는 판정하는 쪽(ABGPlayerController)과 **반드시 같은 방법으로** 구해야 한다.
		//	   한쪽만 어긋나면 손패에 뜬 확률과 실제 판정이 달라지고, 그 차이는 용병처럼
		//	   공격 한정 보정을 가진 직업에서만 나타나 재현이 어렵다.
		const bool bIsAttack = Row->TraitTags.HasTag(BG::Card::Trait::Attack.GetTag());

		const UBGDiceComponent* Dice = UBGDiceComponent::Get(WorldContextObject);
		View.FinalChance = (Dice && SourceASC)
			? ToChanceView(Dice->ApplyModifiers(Row->OutcomeWeights, SourceASC, bIsAttack))
			: View.BaseChance;

		View.bChanceModified = ChancesDiffer(View.BaseChance, View.FinalChance);
	}

	return View;
}

TArray<FBGCardView> UBGCardViewLibrary::MakeHandViews(const ABGPlayerState* PlayerState)
{
	TArray<FBGCardView> Views;
	if (!PlayerState)
	{
		return Views;
	}

	const TArray<FBGCardInstance>& Hand = PlayerState->GetHand();
	Views.Reserve(Hand.Num());

	//	내 손패이므로 시전자는 나다 — 장비·직업 보정을 반영한 최종 확률을 채울 수 있다.
	//	ASC 는 폰에 붙어 있다(ABGGameState::GetSeatHealth 와 같은 경로).
	//	⚠️ 보정이 바뀌면 이 뷰는 낡는다. 손패가 바뀔 때만 다시 만들어지기 때문이다.
	//	   장착 직후 갱신은 ABGPlayerController::RefreshHandWidget 호출로 보장한다.
	const UAbilitySystemComponent* SourceASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState->GetPawn());

	//	⚠️ 여기서 턴/페이즈를 반영하지 말 것.
	//	   bPlayable 은 SetHand 시점에 뷰에 박히는 스냅샷이라, 손패 "내용" 이 바뀔 때만 갱신된다.
	//	   턴 진행은 Draw 페이즈에서 카드를 뽑은 뒤 Play 로 넘어가므로, 마지막 갱신이 Draw 시점에
	//	   일어나 "내 차례인데 카드가 안 나가는" 교착이 생겼다(카드를 내야 손패가 바뀌는데
	//	   카드를 낼 수 없으므로 영구히 풀리지 않는다).
	//
	//	   턴/페이즈 잠금은 UBGHandWidget::SetInteractionLocked 가 담당한다 —
	//	   그쪽은 카드 위젯을 재생성하지 않고 상태만 바꾸므로 주기 갱신해도 드래그가 끊기지 않는다.
	//	   (ABGPlayerController::UpdateHandInteractionLock)
	for (const FBGCardInstance& Card : Hand)
	{
		Views.Add(MakeCardView(PlayerState, Card, SourceASC));
	}

	return Views;
}
