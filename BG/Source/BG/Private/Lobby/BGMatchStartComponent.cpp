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
	//	사람 수 기준이다. 좌석 총원(PvETargetSeats)을 쓰면 혼자서는 영영 시작할 수 없다.
	const ABGGameMode* GM = GetGameMode();
	if (GM && GM->IsPvE())
	{
		return FMath::Clamp(PvEMinStartPlayers, 1, ABGGameMode::MaxPlayers);
	}

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

	//	PvE 는 기다릴 사람이 없다 - 조건이 차면 바로 연다.
	//	준비 상태가 바뀔 때도 불리므로 재시도가 된다. PostLogin 에 직접 넣지 말 것.
	if (GM->IsPvE())
	{
		FString Error;
		if (CanStartMatch(Error))
		{
			UE_LOG(LogBGNet, Log, TEXT("[PvE] 시작 조건 충족 — 자동 시작"));
			StartMatch();
		}
		else
		{
			UE_LOG(LogBGNet, Verbose, TEXT("[PvE] 자동 시작 보류 — %s"), *Error);
		}
	}
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

	//	PvE: 빈 좌석을 봇으로 채운다. 아래 RandomizeSeats·AssignRoles 가 인원수를 보고
	//	돌기 때문에 반드시 그보다 먼저다.
	if (GM->IsPvE())
	{
		const int32 Needed = FMath::Max(0, PvETargetSeats - Players.Num());
		const int32 Added  = GM->SpawnAIPlayers(Needed);

		if (Added > 0)
		{
			//	봇을 포함해 다시 센다. 아래 로그와 AssignRoles 가 이 목록을 쓴다.
			GM->CollectPlayers(Players);
			UE_LOG(LogBGNet, Log, TEXT("[PvE] 봇 %d 명 충원 (목표 좌석 %d) — 총 %d 명"),
				Added, PvETargetSeats, Players.Num());
		}
		else if (Needed > 0)
		{
			UE_LOG(LogBGNet, Warning, TEXT("[PvE] 봇 충원 실패 — %d 명 필요했으나 0 명 스폰됨."), Needed);
		}
	}

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
	if (GM->IsPvE())
	{
		GM->AssignRolesPvE();
	}
	else
	{
		GM->AssignRoles();
	}

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

