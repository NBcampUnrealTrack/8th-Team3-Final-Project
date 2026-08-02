#include "Card/BGDiceComponent.h"
#include "BGGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GAS/BGAttributeSet.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

UBGDiceComponent::UBGDiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UBGDiceComponent* UBGDiceComponent::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	return GS ? GS->FindComponentByClass<UBGDiceComponent>() : nullptr;
}

void UBGDiceComponent::InitializeStream(int32 MatchSeed)
{
	//	같은 매치 시드에서 파생시키되 셔플과는 다른 수열을 쓴다(재현 가능 + 상호 간섭 없음).
	//	곱하는 상수는 홀수 소수 — 시드가 1씩 달라져도 수열이 충분히 벌어진다.
	const int32 DiceSeed = MatchSeed * 2654435761u + 1013904223;

	RandomStream.Initialize(DiceSeed);
	bStreamInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("[BG] 주사위 스트림 초기화 — 매치시드 %d → 판정시드 %d"),
		MatchSeed, DiceSeed);
}

int32 UBGDiceComponent::RollDice(int32 Faces)
{
	//	굴리기는 서버 권위다. 클라에서 굴리면 서버와 결과가 갈라진다.
	const AActor* Owner = GetOwner();
	if (Owner && !Owner->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] RollDice 는 서버 전용이다 — 클라 호출 무시(1 반환)."));
		return 1;
	}

	const int32 N = (Faces > 0) ? Faces : DiceFaces;
	if (N < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] RollDice: 면 수 %d 는 2 미만 — 1 반환."), N);
		return 1;
	}

	if (!bStreamInitialized)
	{
		//	InitializeStream 전에 굴리면 매 판 같은 수열이 나온다 — 초기화 누락을 알린다.
		UE_LOG(LogTemp, Warning,
			TEXT("[BG] 주사위 스트림이 초기화되지 않았다 — GameState 의 덱 초기화가 먼저 돌아야 한다."));
	}

	const int32 Result = RandomStream.RandRange(1, N); // 1 ~ N 균등
	UE_LOG(LogTemp, Log, TEXT("[BG] 주사위 d%d → %d"), N, Result);
	return Result;
}

EBGDiceOutcome UBGDiceComponent::ClassifyRoll(int32 Roll, int32 Faces) const
{
	const int32 N = (Faces > 0) ? Faces : DiceFaces;
	if (N < 1)
	{
		return EBGDiceOutcome::Failure;
	}

	// 비율 정규화 (음수 방지, 합으로 나눔).
	const float CF = FMath::Max(0.f, CriticalFailureRatio);
	const float F  = FMath::Max(0.f, FailureRatio);
	const float S  = FMath::Max(0.f, SuccessRatio);
	const float CS = FMath::Max(0.f, CriticalSuccessRatio);
	const float Sum = CF + F + S + CS;
	if (Sum <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] ClassifyRoll: 비율 합이 0 — 성공으로 처리."));
		return EBGDiceOutcome::Success;
	}

	// 굴린 눈을 (0,1] 로 정규화. 낮을수록 대실패, 높을수록 대성공.
	const float T = static_cast<float>(FMath::Clamp(Roll, 1, N)) / static_cast<float>(N);

	// 누적 구간 경계.
	const float C1 = CF / Sum;
	const float C2 = C1 + F / Sum;
	const float C3 = C2 + S / Sum;

	if (T <= C1) return EBGDiceOutcome::CriticalFailure;
	if (T <= C2) return EBGDiceOutcome::Failure;
	if (T <= C3) return EBGDiceOutcome::Success;
	return EBGDiceOutcome::CriticalSuccess;
}

EBGDiceOutcome UBGDiceComponent::RollForOutcome(int32& OutRoll, int32 Faces, int32 RollBonus)
{
	OutRoll = RollDice(Faces);

	// 보정치는 판정에만 반영한다(원본 눈 OutRoll 은 로그/연출용으로 보존).
	// ClassifyRoll 이 내부에서 [1, N] 으로 클램프하므로 초과분은 자동으로 대성공에 수렴.
	const int32 EffectiveRoll = OutRoll + RollBonus;
	const EBGDiceOutcome Outcome = ClassifyRoll(EffectiveRoll, Faces);

	const int32 N = (Faces > 0) ? Faces : DiceFaces;
	if (RollBonus != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[BG] 판정 — d%d 눈 %d (보정 %+d → %d) → %s"),
			N, OutRoll, RollBonus, EffectiveRoll, *GetOutcomeText(Outcome).ToString());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[BG] 판정 — d%d 눈 %d → %s"),
			N, OutRoll, *GetOutcomeText(Outcome).ToString());
	}
	return Outcome;
}

FText UBGDiceComponent::GetOutcomeText(EBGDiceOutcome Outcome)
{
	switch (Outcome)
	{
	case EBGDiceOutcome::CriticalFailure: return NSLOCTEXT("BG", "DiceCritFail",    "대실패");
	case EBGDiceOutcome::Failure:         return NSLOCTEXT("BG", "DiceFail",        "실패");
	case EBGDiceOutcome::Success:         return NSLOCTEXT("BG", "DiceSuccess",     "성공");
	case EBGDiceOutcome::CriticalSuccess: return NSLOCTEXT("BG", "DiceCritSuccess", "대성공");
	default:                                return FText::GetEmpty();
	}
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  카드 데이터 기반 판정 (GDD §4.2)
//
//  카드 비율 + 보정 → 정규화 → 누적 구간(대성공 → 성공 → 실패 → 대실패) → 0~99 난수 1회.
//  판정을 여기 한곳에 모아두면 GA 10 개가 각자 굴리는 중복이 사라지고,
//  "같은 판정을 한 번만 적용" (GDD §9.2) 을 구조적으로 보장할 수 있다.
// ══════════════════════════════════════════════════════════════════════════════════════

FBGOutcomeWeights UBGDiceComponent::ApplyLuck(const FBGOutcomeWeights& Base, int32 Luck) const
{
	FBGOutcomeWeights Out = Base;
	if (Luck != 0)
	{
		Out.ApplyBonus(Luck * LuckSuccessWeightPerPoint, Luck * LuckCriticalWeightPerPoint);
	}
	return Out;
}

FBGOutcomeWeights UBGDiceComponent::ApplyModifiers(const FBGOutcomeWeights& Base,
	const UAbilitySystemComponent* SourceASC) const
{
	FBGOutcomeWeights Out = Base;
	if (!SourceASC)
	{
		return Out;
	}

	const UBGAttributeSet* AS = SourceASC->GetSet<UBGAttributeSet>();
	if (!AS)
	{
		return Out;
	}

	//	1) 승산 먼저 — 무효화(큰 음수 가산)가 배율에 흔들리지 않도록.
	//	   실패 등급에는 승산을 두지 않았다(필요한 카드가 없다 — GDD §7.2 최소 집합 권고).
	Out.ApplyMultiplier(
		AS->GetWeightMultCriticalFailure(),
		/*MultS=*/1.f,
		AS->GetWeightMultCriticalSuccess());

	//	2) 가산 — 비율 보정 어트리뷰트만.
	//	   ⚠️ 행운(Luck)은 여기에 넣지 않는다.
	//	      DT 비율이 곧 표시 확률이어야 §10 "최종 확률 표시" 와 §11 "설정 확률 vs 실측" 비교가
	//	      성립한다. 행운을 섞으면 카드 UI 의 확률이 플레이어마다 달라진다.
	//	      행운을 다시 쓰려면 ApplyLuck 을 명시적으로 호출하거나 WeightBonus* 로 환산할 것.
	Out.ApplyBonusAll(
		FMath::RoundToInt(AS->GetWeightBonusCriticalFailure()),
		FMath::RoundToInt(AS->GetWeightBonusFailure()),
		FMath::RoundToInt(AS->GetWeightBonusSuccess()),
		FMath::RoundToInt(AS->GetWeightBonusCriticalSuccess()));

	return Out;
}

void UBGDiceComponent::GetOutcomeChances(const FBGOutcomeWeights& Weights,
	float& OutCriticalFailure, float& OutFailure, float& OutSuccess, float& OutCriticalSuccess)
{
	OutCriticalFailure = OutFailure = OutSuccess = OutCriticalSuccess = 0.f;

	const int32 Total = Weights.Sum();
	if (Total <= 0)
	{
		return;
	}

	const float Inv = 100.f / static_cast<float>(Total);
	OutCriticalFailure = Weights.CriticalFailure * Inv;
	OutFailure         = Weights.Failure         * Inv;
	OutSuccess         = Weights.Success         * Inv;
	OutCriticalSuccess = Weights.CriticalSuccess * Inv;
}

EBGDiceOutcome UBGDiceComponent::RollOutcome(const FBGOutcomeWeights& Weights, int32& OutRoll)
{
	OutRoll = INDEX_NONE;

	const int32 Total = Weights.Sum();
	if (Total <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dice] 판정 비율 합이 0 — 실패로 처리합니다(카드 데이터 확인)."));
		return EBGDiceOutcome::Failure;
	}

	if (!bStreamInitialized)
	{
		// InitializeStream 전에 굴리면 매 판 같은 수열이 나온다 — 초기화 누락을 알린다.
		UE_LOG(LogTemp, Warning, TEXT("[Dice] RollOutcome: 스트림이 초기화되지 않았습니다."));
	}

	//	0 ~ Total-1 에서 뽑아 누적 구간에 대응시킨다.
	//	0~99 로 고정하지 않는 이유: Total 이 100 이 아닐 때 정규화 반올림으로 확률이 미세하게
	//	틀어지는 것을 피하기 위함이다. 비율 그대로가 곧 확률이 된다.
	const int32 Pick = RandomStream.RandRange(0, Total - 1);
	OutRoll = (Total == 100) ? Pick : FMath::FloorToInt(Pick * 100.f / Total);   // 표시용 0~99 환산

	int32 Cumulative = Weights.CriticalSuccess;
	if (Pick < Cumulative) { return EBGDiceOutcome::CriticalSuccess; }

	Cumulative += Weights.Success;
	if (Pick < Cumulative) { return EBGDiceOutcome::Success; }

	Cumulative += Weights.Failure;
	if (Pick < Cumulative) { return EBGDiceOutcome::Failure; }

	return EBGDiceOutcome::CriticalFailure;
}

int32 UBGDiceComponent::RandomIndex(int32 Max)
{
	if (Max <= 0)
	{
		return INDEX_NONE;
	}

	if (!bStreamInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dice] RandomIndex: 스트림이 초기화되지 않았습니다."));
	}

	return RandomStream.RandRange(0, Max - 1);
}

FGameplayTag UBGDiceComponent::OutcomeToTag(EBGDiceOutcome Outcome)
{
	switch (Outcome)
	{
	case EBGDiceOutcome::CriticalSuccess: return BG::Resolution::CriticalSuccess.GetTag();
	case EBGDiceOutcome::Success:         return BG::Resolution::Success.GetTag();
	case EBGDiceOutcome::Failure:         return BG::Resolution::Failure.GetTag();
	case EBGDiceOutcome::CriticalFailure: return BG::Resolution::CriticalFailure.GetTag();
	default:                                return FGameplayTag();
	}
}

EBGDiceOutcome UBGDiceComponent::TagsToOutcome(const FGameplayTagContainer& Tags, EBGDiceOutcome Fallback)
{
	if (Tags.HasTagExact(BG::Resolution::CriticalSuccess.GetTag())) { return EBGDiceOutcome::CriticalSuccess; }
	if (Tags.HasTagExact(BG::Resolution::Success.GetTag()))         { return EBGDiceOutcome::Success; }
	if (Tags.HasTagExact(BG::Resolution::Failure.GetTag()))         { return EBGDiceOutcome::Failure; }
	if (Tags.HasTagExact(BG::Resolution::CriticalFailure.GetTag())) { return EBGDiceOutcome::CriticalFailure; }
	return Fallback;
}
