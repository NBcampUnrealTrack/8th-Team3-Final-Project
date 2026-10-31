#include "Tutorial/BGTutorialComponent.h"

#include "AI/BGAIPlayerController.h"
#include "BGGameplayTags.h"
#include "Core/BGGameInstance.h"
#include "Core/BGGameMode.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "Core/BGPlayerState.h"
#include "Debug/BGCardLog.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Tutorial/BGTutorialStepRow.h"

UBGTutorialComponent::UBGTutorialComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBGTutorialComponent::BeginPlay()
{
	Super::BeginPlay();

	//	튜토리얼 판의 서버에서만 일한다. 게임모드는 서버에만 있어 클라이언트에서는 이 컴포넌트가 없다.
	const ABGGameMode* GM = Cast<ABGGameMode>(GetOwner());
	if (!IsValid(GM) || !GM->HasAuthority() || !GM->IsTutorial())
	{
		return;
	}

	const UDataTable* Table = GetStepTable();
	if (!IsValid(Table))
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[Tutorial] 단계 표가 없다 - BP_BGGameInstance 의 BG|Tutorial 에서 DT_BGTutorialSteps 를 고를 것."));
		return;
	}

	TArray<TPair<int32, FName>> Sorted;
	Table->ForeachRow<FBGTutorialStepRow>(TEXT("TutorialComponent"),
		[&Sorted](const FName& Key, const FBGTutorialStepRow& Row)
		{
			Sorted.Emplace(Row.Order, Key);
		});
	Sorted.Sort([](const TPair<int32, FName>& A, const TPair<int32, FName>& B)
	{
		return A.Key < B.Key;
	});

	for (const TPair<int32, FName>& Entry : Sorted)
	{
		StepIds.Add(Entry.Value);
	}

	if (ABGGameState* GS = GetBGGameState())
	{
		GS->OnTurnStateChanged.AddDynamic(this, &UBGTutorialComponent::HandleTurnStateChanged);
		GS->OnIntroStageChanged.AddUObject(this, &UBGTutorialComponent::HandleIntroStageChanged);
	}
	else
	{
		UE_LOG(LogBGCard, Error, TEXT("[Tutorial] GameState 가 없어 진행기를 시작하지 못했다."));
	}

	UE_LOG(LogBGCard, Log, TEXT("[Tutorial] 진행기 시작 - 단계 %d개"), StepIds.Num());
}

void UBGTutorialComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ABGGameState* GS = GetBGGameState())
	{
		GS->OnTurnStateChanged.RemoveDynamic(this, &UBGTutorialComponent::HandleTurnStateChanged);
		GS->OnIntroStageChanged.RemoveAll(this);
	}

	Super::EndPlay(Reason);
}

void UBGTutorialComponent::HandleTurnStateChanged()
{
	Evaluate();
}

void UBGTutorialComponent::HandleIntroStageChanged(EBGIntroStage /*Stage*/)
{
	Evaluate();
}

void UBGTutorialComponent::Evaluate()
{
	//	GameState 를 바꾸면 통지가 다시 여기로 들어온다. 처리 중이면 표시만 해 두고 끝난 뒤 한 번 더 돈다.
	if (bEvaluating)
	{
		bReevaluate = true;
		return;
	}

	bEvaluating = true;
	do
	{
		bReevaluate = false;
		EvaluateOnce();
	}
	while (bReevaluate);
	bEvaluating = false;
}

void UBGTutorialComponent::EvaluateOnce()
{
	ABGGameState* GS = GetBGGameState();
	const UDataTable* Table = GetStepTable();
	if (!IsValid(GS) || !IsValid(Table))
	{
		return;
	}

	//	판이 끝났으면 떠 있는 팝업과 정지를 걷는다. 결과 화면 위에 설명이 남으면 안 된다.
	if (!GS->IsMatchRunning())
	{
		if (ActiveIndex != INDEX_NONE)
		{
			CompleteStep();
		}
		return;
	}

	if (ActiveIndex != INDEX_NONE)
	{
		//	행동형 단계는 그 행동이 일어났는지 본다. 읽기형은 다음 버튼(HandleNextPressed)만 끝낸다.
		const FBGTutorialStepRow* Active = Table->FindRow<FBGTutorialStepRow>(StepIds[ActiveIndex], TEXT("TutorialComponent"));
		if (!Active || (Active->CompleteOn != EBGTutorialComplete::NextButton && IsCompleteMet(*Active)))
		{
			CompleteStep();
		}
		return;
	}

	if (!StepIds.IsValidIndex(NextIndex))
	{
		return;   //	단계를 다 마쳤다.
	}

	const FBGTutorialStepRow* Next = Table->FindRow<FBGTutorialStepRow>(StepIds[NextIndex], TEXT("TutorialComponent"));
	if (!Next)
	{
		++NextIndex;
		bReevaluate = true;
		return;
	}

	if (IsIntroTrigger(*Next))
	{
		//	도입 연출이 이미 끝났다(건너뛰었다). 이 행을 붙들고 있으면 뒤 단계가 영영 막힌다.
		//	첫 턴 좌석이 정해지기 전에는 연출이 아직 시작 전일 수 있어 건너뛰지 않는다.
		if (!GS->IsIntroPlaying() && GS->GetCurrentSeat() != INDEX_NONE)
		{
			++NextIndex;
			bReevaluate = true;
			return;
		}
	}
	else if (GS->IsPresentationBusy() || GS->IsIntroPlaying() || GS->GetCurrentSeat() == INDEX_NONE)
	{
		//	연출이 끝나기를 기다린다. 연출 중에 팝업을 끼우면 화면이 겹친다.
		//	첫 턴 좌석이 없으면 판은 열렸어도 도입 연출이 아직 시작 전이다 - 그때 뜨면 연출을 가로막는다.
		return;
	}

	if (IsTriggerMet(*Next))
	{
		ActivateStep(NextIndex, *Next);
	}
}

bool UBGTutorialComponent::IsIntroTrigger(const FBGTutorialStepRow& Row) const
{
	return Row.Trigger == EBGTutorialTrigger::IntroRoleReveal
		|| Row.Trigger == EBGTutorialTrigger::IntroHeroReveal
		|| Row.Trigger == EBGTutorialTrigger::IntroFirstTurn;
}

bool UBGTutorialComponent::IsTriggerMet(const FBGTutorialStepRow& Row) const
{
	const ABGGameState* GS = GetBGGameState();
	if (!IsValid(GS))
	{
		return false;
	}

	const int32 HumanSeat = FindHumanSeat();
	const int32 CurrentSeat = GS->GetCurrentSeat();
	const bool bPlayPhase = GS->GetPhaseTag() == BG::Phase::Play.GetTag();

	switch (Row.Trigger)
	{
	case EBGTutorialTrigger::AfterPrevious:
		{
			return true;
		}
	case EBGTutorialTrigger::MatchStart:
		{
			return CurrentSeat != INDEX_NONE && bPlayPhase;
		}
	case EBGTutorialTrigger::MyTurnStart:
		{
			return HumanSeat != INDEX_NONE && CurrentSeat == HumanSeat && bPlayPhase;
		}
	case EBGTutorialTrigger::EnemyTurnStart:
		{
			return HumanSeat != INDEX_NONE && CurrentSeat != INDEX_NONE && CurrentSeat != HumanSeat && bPlayPhase;
		}
	case EBGTutorialTrigger::AfterMyCard:
		{
			const ABGPlayerState* PS = GS->GetPlayerStateBySeat(HumanSeat);
			return IsValid(PS) && CurrentSeat == HumanSeat && PS->GetCardsUsedThisTurn() > 0;
		}
	case EBGTutorialTrigger::IntroRoleReveal:
		{
			return GS->GetIntroStage() == EBGIntroStage::RoleReveal;
		}
	case EBGTutorialTrigger::IntroHeroReveal:
		{
			return GS->GetIntroStage() == EBGIntroStage::HeroReveal;
		}
	case EBGTutorialTrigger::IntroFirstTurn:
		{
			return GS->GetIntroStage() == EBGIntroStage::FirstTurn;
		}
	}

	return false;
}

bool UBGTutorialComponent::IsCompleteMet(const FBGTutorialStepRow& Row) const
{
	const ABGGameState* GS = GetBGGameState();
	if (!IsValid(GS))
	{
		return false;
	}

	switch (Row.CompleteOn)
	{
	case EBGTutorialComplete::NextButton:
		{
			return false;
		}
	case EBGTutorialComplete::CardPlayed:
		{
			const ABGPlayerState* PS = GS->GetPlayerStateBySeat(FindHumanSeat());
			return IsValid(PS) && PS->GetCardsUsedThisTurn() > BaselineCardsUsed;
		}
	case EBGTutorialComplete::TurnEnded:
		{
			return GS->GetCurrentSeat() != BaselineSeat;
		}
	}

	return false;
}

void UBGTutorialComponent::ActivateStep(int32 Index, const FBGTutorialStepRow& Row)
{
	ABGGameState* GS = GetBGGameState();
	if (!IsValid(GS))
	{
		return;
	}

	ActiveIndex = Index;
	NextIndex = Index + 1;

	//	행동형 완료 판정의 기준을 잡는다. 이 순간 이후에 늘어난 것만 "해냈다"로 본다.
	const ABGPlayerState* PS = GS->GetPlayerStateBySeat(FindHumanSeat());
	BaselineCardsUsed = IsValid(PS) ? PS->GetCardsUsedThisTurn() : 0;
	BaselineSeat = GS->GetCurrentSeat();

	//	정지를 먼저 건다. 팝업이 뜨는 프레임에 AI 가 한 수 더 두는 틈을 없애기 위해서다.
	if (Row.CompleteOn == EBGTutorialComplete::NextButton)
	{
		GS->SetTutorialHold(true);
	}
	GS->SetActiveTutorialStep(StepIds[Index]);

	UE_LOG(LogBGCard, Log, TEXT("[Tutorial] 단계 시작 - %s"), *StepIds[Index].ToString());
}

void UBGTutorialComponent::CompleteStep()
{
	ABGGameState* GS = GetBGGameState();
	if (IsValid(GS))
	{
		GS->SetTutorialHold(false);
		GS->SetActiveTutorialStep(NAME_None);
	}

	if (StepIds.IsValidIndex(ActiveIndex))
	{
		UE_LOG(LogBGCard, Log, TEXT("[Tutorial] 단계 끝 - %s"), *StepIds[ActiveIndex].ToString());
	}

	ActiveIndex = INDEX_NONE;
	bReevaluate = true;   //	다음 단계가 이미 조건을 채웠을 수 있다.
}

void UBGTutorialComponent::HandleNextPressed()
{
	const ABGGameMode* GM = Cast<ABGGameMode>(GetOwner());
	const UDataTable* Table = GetStepTable();
	if (!IsValid(GM) || !GM->HasAuthority() || !GM->IsTutorial() || !IsValid(Table)
		|| bEvaluating || !StepIds.IsValidIndex(ActiveIndex))
	{
		return;
	}

	//	클라이언트가 보낸 요청이라 지금 단계가 정말 읽기형인지 서버가 다시 본다.
	const FBGTutorialStepRow* Active = Table->FindRow<FBGTutorialStepRow>(StepIds[ActiveIndex], TEXT("TutorialComponent"));
	if (!Active || Active->CompleteOn != EBGTutorialComplete::NextButton)
	{
		return;
	}

	//	CompleteStep 이 GameState 를 두 번 바꾸는 사이에 통지가 끼어들면 다음 단계가 먼저 활성화돼 버린다.
	bEvaluating = true;
	CompleteStep();
	bEvaluating = false;

	Evaluate();
}

int32 UBGTutorialComponent::FindHumanSeat() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return INDEX_NONE;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const ABGPlayerController* PC = Cast<ABGPlayerController>(It->Get());
		if (IsValid(PC) && !PC->IsA<ABGAIPlayerController>())
		{
			if (const ABGPlayerState* PS = PC->GetPlayerState<ABGPlayerState>(); IsValid(PS))
			{
				return PS->GetSeatIndex();
			}
		}
	}

	return INDEX_NONE;
}

ABGGameState* UBGTutorialComponent::GetBGGameState() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetGameState<ABGGameState>() : nullptr;
}

const UDataTable* UBGTutorialComponent::GetStepTable() const
{
	const UWorld* World = GetWorld();
	const UBGGameInstance* GI = IsValid(World) ? Cast<UBGGameInstance>(World->GetGameInstance()) : nullptr;
	return IsValid(GI) ? GI->GetTutorialStepTable() : nullptr;
}
