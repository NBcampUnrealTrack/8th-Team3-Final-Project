// BANG! 판 시작 진행 — 서버 권위. 맵 이동 없이 제자리에서 판을 연다.
// 호스트가 시작을 누르면 세션 계층이 StartMatch 를 부르고, 이 컴포넌트가 역할 배정을 시킨다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BGMatchStartComponent.generated.h"

class ABGGameMode;

UCLASS(ClassGroup = (BG), Config = Game, meta = (BlueprintSpawnableComponent))
class BG_API UBGMatchStartComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGMatchStartComponent();

	// 현재 월드의 게임모드에 붙은 컴포넌트. 서버가 아니면 nullptr.
	static UBGMatchStartComponent* Find(const UWorld* World);

	// 게임모드의 PostLogin 과 준비 상태 변경에서 불린다. 로비 현황만 갱신한다.
	void NotifyPlayerJoined();

	// 서버 전용. 역할을 배정하고 판을 연다. 조건이 안 맞으면 false.
	UFUNCTION(BlueprintCallable, Category = "BG|Match")
	bool StartMatch();

	// 지금 시작해도 되는가(인원 4~7 + 전원 준비). 사유는 OutError 로 돌려준다.
	bool CanStartMatch(FString& OutError) const;

	// 지금 새 플레이어를 받아도 되는가. 거절 사유는 OutError 로 돌려준다.
	// 클라의 bAllowJoinInProgress 검사는 검색 스냅샷이라 못 믿는다 — 서버 판정이 최종.
	bool CanAcceptPlayer(FString& OutError) const;

	UFUNCTION(BlueprintPure, Category = "BG|Match")
	bool IsMatchStarted() const { return bMatchStarted; }

	UFUNCTION(BlueprintPure, Category = "BG|Match")
	int32 GetMinStartPlayers() const;

protected:
	// 시작에 필요한 최소 인원. 4~7 로 클램프된다.
	// DefaultGame.ini [/Script/BG.BGMatchStartComponent] 로 덮어쓴다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "BG|Match")
	int32 MinStartPlayers = 4;

	// 시작 손패 장수. 0 이하면 BG_DealHand 의 기본값(5장)을 쓴다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "BG|Match")
	int32 StartingHandCards = 0;

	// PvE: 조우 하나의 총 좌석 수(사람 + 봇). 사람 수를 뺀 나머지를 봇으로 채운다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "BG|PvE")
	int32 PvETargetSeats = 2;

	// PvE: 시작에 필요한 사람 수. 봇은 그 뒤에 스폰되므로 여기 안 센다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "BG|PvE")
	int32 PvEMinStartPlayers = 1;

private:
	ABGGameMode* GetGameMode() const;

	bool bMatchStarted = false;
};
