// TR 세션 플로우 — UI 와 UTRGameInstance 사이의 단일 바인딩 계층.
// UI 는 ITRSessionInterface 로만 접근한다. 레벨 트래블은 다루지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Session/TRSessionTypes.h"
#include "Session/TRSessionInterface.h"
#include "TRSessionFlow.generated.h"

class UTRGameInstance;

UCLASS(Config = Game)
class TR_API UTRSessionFlow : public UGameInstanceSubsystem, public ITRSessionInterface
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── ITRSessionInterface ──

	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	virtual void HostCreateRoom(const FString& RoomName) override;

	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	virtual void JoinRoomByCode(const FString& RoomCode) override;

	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	virtual void RefreshSessionList(const FTRSessionSearchFilter& Filter) override;

	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	virtual void JoinSessionByIndex(int32 ResultIndex) override;

	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	virtual void LeaveSession() override;

	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	virtual void OpenInviteOverlay() override;

	UFUNCTION(BlueprintPure, Category = "TR|Session")
	virtual bool IsHost() const override;

	UFUNCTION(BlueprintPure, Category = "TR|Session")
	virtual FTRLobbyStatus GetLobbyStatus() const override;

	UFUNCTION(BlueprintPure, Category = "TR|Session")
	virtual FString GetRoomCode() const override;

	virtual FOnTRSessionPhaseChanged& GetSessionPhaseChangedEvent() override { return OnSessionPhaseChanged; }
	virtual FOnTRSessionListReady& GetSessionListReadyEvent() override { return OnSessionListReady; }

	// ── UI 단계 통지(BP 바인딩용 — C++ 은 인터페이스의 Get*Event 로 접근) ──

	UPROPERTY(BlueprintAssignable, Category = "TR|Session")
	FOnTRSessionPhaseChanged OnSessionPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "TR|Session")
	FOnTRSessionListReady OnSessionListReady;

protected:
	// 방 정원. DefaultGame.ini [/Script/TR.TRSessionFlow] 로 덮어쓸 수 있다.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "TR|Session")
	int32 MaxPlayers = 4;

private:
	UTRGameInstance* GetTRGameInstance() const;

	void BindGameInstanceEvents();
	void UnbindGameInstanceEvents();

	// UTRGameInstance 세션 콜백.
	UFUNCTION()
	void HandleCreateSessionComplete(bool bSuccess);

	UFUNCTION()
	void HandleFindSessionsComplete(bool bSuccess, int32 NumFound);

	UFUNCTION()
	void HandleJoinSessionComplete(bool bSuccess);

	void SetPhase(ETRSessionPhase Phase, const FString& Message = FString());

	// 검색 완료 → PendingListFilter 적용 → OnSessionListReady 브로드캐스트.
	void BroadcastFilteredSessionList(bool bSearchSuccess);

	// 검색 후행 의도 두 갈래(코드 조인 / 목록 갱신) — 겹치면 마지막 의도만 유지한다.
	FString PendingJoinCode;
	bool bWantsJoinAfterSearch = false;

	FTRSessionSearchFilter PendingListFilter;
	bool bWantsListAfterSearch = false;
};
