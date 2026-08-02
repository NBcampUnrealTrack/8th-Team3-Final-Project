// BANG! 세션 플로우 — UI 와 UBGGameInstance 사이의 단일 바인딩 계층.
// UI 는 IBGSessionInterface 로만 접근한다. 레벨 트래블은 다루지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Session/BGSessionTypes.h"
#include "Session/BGSessionInterface.h"
#include "BGSessionFlow.generated.h"

class UBGGameInstance;
class UBGReadyComponent;

UCLASS(Config = Game)
class BG_API UBGSessionFlow : public UGameInstanceSubsystem, public IBGSessionInterface
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── IBGSessionInterface ──
	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	virtual void HostCreateRoom(const FString& RoomName) override;

	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	virtual void JoinRoomByCode(const FString& RoomCode) override;

	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	virtual void RefreshSessionList(const FBGSessionSearchFilter& Filter) override;

	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	virtual void JoinSessionByIndex(int32 ResultIndex) override;

	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	virtual void LeaveSession() override;

	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	virtual void SetLocalReady(bool bReady) override;

	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	virtual void StartGame() override;

	UFUNCTION(BlueprintPure, Category = "BG|Session")
	virtual bool IsHost() const override;

	UFUNCTION(BlueprintPure, Category = "BG|Session")
	virtual bool IsLocalReady() const override;

	virtual bool CanStartGame(FString& OutReason) const override;

	UFUNCTION(BlueprintPure, Category = "BG|Session")
	virtual FBGLobbyStatus GetLobbyStatus() const override;

	UFUNCTION(BlueprintPure, Category = "BG|Session")
	virtual FString GetRoomCode() const override;

	virtual FOnBGSessionPhaseChanged& GetSessionPhaseChangedEvent() override { return OnSessionPhaseChanged; }
	virtual FOnBGSessionListReady& GetSessionListReadyEvent() override { return OnSessionListReady; }

	// ── UI 단계 통지(BP 바인딩용 — C++ 은 인터페이스의 Get*Event 로 접근) ──
	UPROPERTY(BlueprintAssignable, Category = "BG|Session")
	FOnBGSessionPhaseChanged OnSessionPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "BG|Session")
	FOnBGSessionListReady OnSessionListReady;

protected:
	// 방 정원(BANG! 기본판 4~7인). DefaultGame.ini [/Script/BG.BGSessionFlow] 로 덮어쓸 수 있다.
	// 실제 클램프는 GameInstance 에서 ABGGameMode::Min/MaxPlayers 로 한 번 더.
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "BG|Session")
	int32 MaxPlayers = 7;

private:
	UBGGameInstance* GetBGGameInstance() const;

	// 로컬 플레이어의 준비 컴포넌트. 방에 들어가기 전이면 nullptr.
	UBGReadyComponent* GetLocalReadyComponent() const;

	void BindGameInstanceEvents();
	void UnbindGameInstanceEvents();

	// UBGGameInstance 세션 콜백.
	UFUNCTION()
	void HandleCreateSessionComplete(bool bSuccess);
	UFUNCTION()
	void HandleFindSessionsComplete(bool bSuccess, int32 NumFound);
	UFUNCTION()
	void HandleJoinSessionComplete(bool bSuccess);

	// 단계 통지 헬퍼.
	void SetPhase(EBGSessionPhase Phase, const FString& Message = FString());

	// 검색 완료 → PendingListFilter 적용 → OnSessionListReady 브로드캐스트.
	void BroadcastFilteredSessionList(bool bSearchSuccess);

	// 검색 후행 의도 두 갈래(코드 조인 / 목록 갱신) — 겹치면 마지막 의도만 유지한다.
	// 조인 대기 중 코드(검색 완료 후 매칭에 사용).
	FString PendingJoinCode;
	bool bWantsJoinAfterSearch = false;

	// 방찾기 목록 갱신 대기 필터(검색 완료 후 클라 측 적용).
	FBGSessionSearchFilter PendingListFilter;
	bool bWantsListAfterSearch = false;
};
