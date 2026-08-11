#include "Lobby/BGMatchStartComponent.h"

#include "Core/BGGameMode.h"
#include "Lobby/BGReadyComponent.h"
#include "Lobby/BGSeatComponent.h"
#include "Core/BGGameState.h"
#include "BGGameplayTags.h"
#include "UI/Session/BGSessionMenuController.h"
#include "Debug/BGNetLog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Math/UnrealMathUtility.h"

UBGMatchStartComponent::UBGMatchStartComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UBGMatchStartComponent::GetMinStartPlayers() const
{
	return FMath::Clamp(MinStartPlayers, ABGGameMode::MinPlayers, ABGGameMode::MaxPlayers);
}

void UBGMatchStartComponent::NotifyPlayerJoined()
{
	const ABGGameMode* GM = GetGameMode();
	if (bMatchStarted || !GM || !GM->HasAuthority())
	{
		return;
	}

	int32 Total = 0;
	int32 Ready = 0;
	UBGReadyComponent::CountLobby(GetWorld(), Total, Ready);

	UE_LOG(LogBGNet, Log, TEXT("[MatchStart] 로비 %d/%d명 · 준비 %d명"),
		Total, GetMinStartPlayers(), Ready);
}

UBGMatchStartComponent* UBGMatchStartComponent::Find(const UWorld* World)
{
	// GetAuthGameMode 는 서버에서만 유효하다 — 클라에서는 자연히 nullptr.
	ABGGameMode* GM = World ? World->GetAuthGameMode<ABGGameMode>() : nullptr;
	return GM ? GM->FindComponentByClass<UBGMatchStartComponent>() : nullptr;
}

ABGGameMode* UBGMatchStartComponent::GetGameMode() const
{
	return Cast<ABGGameMode>(GetOwner());
}

bool UBGMatchStartComponent::CanAcceptPlayer(FString& OutError) const
{
	if (bMatchStarted)
	{
		OutError = TEXT("판이 이미 시작되었습니다");
		return false;
	}
	return true;
}

bool UBGMatchStartComponent::CanStartMatch(FString& OutError) const
{
	if (bMatchStarted)
	{
		OutError = TEXT("이미 시작된 판입니다");
		return false;
	}

	int32 Total = 0;
	int32 Ready = 0;
	UBGReadyComponent::CountLobby(GetWorld(), Total, Ready);

	const int32 Min = GetMinStartPlayers();
	if (Total < Min)
	{
		OutError = FString::Printf(TEXT("인원 부족 (%d/%d명)"), Total, Min);
		return false;
	}
	if (Total > ABGGameMode::MaxPlayers)
	{
		OutError = FString::Printf(TEXT("정원 초과 (%d명)"), Total);
		return false;
	}
	if (Ready < Total)
	{
		OutError = FString::Printf(TEXT("준비 대기 (%d/%d명)"), Ready, Total);
		return false;
	}
	return true;
}

bool UBGMatchStartComponent::StartMatch()
{
	ABGGameMode* GM = GetGameMode();
	if (!GM || !GM->HasAuthority())
	{
		return false;
	}

	FString Error;
	if (!CanStartMatch(Error))
	{
		UE_LOG(LogBGNet, Warning, TEXT("[MatchStart] 시작 불가 — %s"), *Error);
		return false;
	}

	bMatchStarted = true;

	//	세션은 참여 가능 상태로 둔다. 막으면 검색에서 빠져 복귀도 못 한다.
	//	난입 차단은 ABGGameMode::PreLogin 이 한다.

	TArray<APlayerController*> Players;
	GM->CollectPlayers(Players);

	UE_LOG(LogBGNet, Log, TEXT("[MatchStart] 판 시작(%d명) — 좌석·직업·역할 배정"), Players.Num());

	// 좌석을 먼저 섞는다. 로비 좌석(입장 순)을 0..N-1 랜덤 순열로 다시 채워야
	// 퇴장으로 생긴 빈 번호도 메워진다 — 역할 배정과 턴 진행이 이 좌석을 전제로 돈다.
	if (UBGSeatComponent* Seats = UBGSeatComponent::Find(GetWorld()))
	{
		Seats->RandomizeSeats();
	}
	else
	{
		UE_LOG(LogBGNet, Error, TEXT("[MatchStart] UBGSeatComponent 가 없어 좌석을 섞지 못했습니다."));
	}

	// AssignRoles 가 직업(Character.Job.*) 배정까지 함께 처리한다 — 직업 먼저, 역할 나중.
	GM->AssignRoles();

	// 호스트의 로비 UI 는 여기서 내린다. 클라는 각자 페이즈가 로비를 벗어난 것을 보고 닫는다.
	if (UGameInstance* GI = GM->GetGameInstance())
	{
		if (UBGSessionMenuController* Menu = GI->GetSubsystem<UBGSessionMenuController>())
		{
			Menu->Close();
		}
	}

	// 역할 배정으로 좌석과 스탯(Health)이 확정된 뒤에 판을 연다.
	// StartMatch 가 초기 손패를 Health 만큼 나눠주므로 순서가 뒤바뀌면 안 된다.
	if (ABGGameState* GS = GM->GetGameState<ABGGameState>())
	{
		GS->StartMatch();
	}
	else
	{
		UE_LOG(LogBGNet, Error, TEXT("[MatchStart] ABGGameState 가 없어 턴을 시작하지 못했습니다."));
	}

	return true;
}

