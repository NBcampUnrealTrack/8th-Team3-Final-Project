// TR 게임 인스턴스 — OnlineSubsystem 세션 호출부. 방 생성/검색/조인/파기 파이프라인을 담당한다.
// 서브시스템별 옵션 분기는 FTROssPolicy 가 맡고, UI 는 UTRSessionFlow 를 통해서만 접근한다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Session/TRSessionTypes.h"
#include "TRGameInstance.generated.h"

class UNetDriver;

UCLASS(Config = Game)
class TR_API UTRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// ── 세션 ──

	// 방 생성 → 성공 시 게임 맵에서 리슨 시작. RoomName 이 비면 호스트 표시명 기반 기본값.
	// bLAN=false 여도 LAN 전용 서브시스템(NULL)에서는 LAN 으로 내려간다.
	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	void HostSession(int32 MaxPlayers = 4, bool bLAN = false, const FString& RoomName = TEXT(""));

	// 주변 TR 세션 검색 — 결과는 OnFindSessionsComplete 로 통지.
	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	void FindSessions(bool bLAN = false, int32 MaxSearchResults = 20);

	// 직전 검색 결과의 index 세션에 접속(접속 후 자동 ClientTravel).
	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	void JoinFoundSession(int32 SearchResultIndex);

	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	void DestroyCurrentSession();

	// 방을 나간 뒤 메인메뉴 맵으로 복귀. 이미 메인메뉴면 아무것도 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	void ReturnToMainMenu();

	// 스팀 오버레이의 친구 초대 창을 연다. 스팀이 아니거나 세션이 없으면 false.
	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	bool OpenInviteOverlay();

	// 이 설치본을 식별하는 열쇠. 접속 URL 에 ?TRKey= 로 실려 서버가 돌아온 사람을 알아본다.
	// NULL 서브시스템은 접속마다 새 UniqueNetId 를 만들어 쓸 수 없어서 따로 둔다.
	// 한 PC 에서 여러 클라이언트를 띄우는 검증에서는 -TRKey=... 로 다르게 준다.
	UFUNCTION(BlueprintPure, Category = "TR|Session")
	FString GetPlayerKey() const;

	static const TCHAR* PlayerKeyOption;

	//	판 시작 규칙이 정해지면 주석을 푼다 — 시작 시 false 로 바꿔 난입을 막는 자리다.
	//	지금은 참여까지만 열어 두므로 세션은 계속 난입 허용 상태다.
	//
	//	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	//	void SetAllowJoinInProgress(bool bAllow);

	// ── 조회 ──

	UFUNCTION(BlueprintPure, Category = "TR|Session")
	FString GetHostRoomCode() const { return HostRoomCode; }

	// 검색 결과 세션에 광고된 방 코드(클라이언트 코드 매칭용). 없으면 빈 문자열.
	UFUNCTION(BlueprintPure, Category = "TR|Session")
	FString GetFoundSessionCode(int32 SearchResultIndex) const;

	UFUNCTION(BlueprintPure, Category = "TR|Session")
	int32 GetFoundSessionCount() const;

	// 직전 검색 결과 전체 스냅샷(필터 미적용, ResultIndex=검색 캐시 인덱스).
	UFUNCTION(BlueprintPure, Category = "TR|Session")
	TArray<FTRSessionSearchResult> GetFoundSessionResults() const;

	// ── 결과 이벤트 (UTRSessionFlow 가 구독) ──

	UPROPERTY(BlueprintAssignable, Category = "TR|Session")
	FTRSessionBoolEvent OnCreateSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "TR|Session")
	FTRFindSessionsEvent OnFindSessionsComplete;

	UPROPERTY(BlueprintAssignable, Category = "TR|Session")
	FTRSessionBoolEvent OnJoinSessionComplete;

private:
	IOnlineSessionPtr GetSessionInterface() const;

	// 호스트가 이번 세션에 광고한 방 코드(6자 A-Z2-9). 호스트 전용.
	FString HostRoomCode;

	// 6자 방 코드 생성(혼동 문자 0/O/1/I 제외).
	static FString GenerateRoomCode();

	// 직전 검색 결과 보관(Join 시 참조).
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	// 기존 세션 파기 완료를 기다리는 방 생성 요청(파기는 비동기 — 즉시 생성하면 거부된다).
	struct FPendingHostRequest
	{
		FString RoomName;
		int32   MaxPlayers = 4;
		bool    bLAN = false;
		bool    bValid = false;
	};
	FPendingHostRequest PendingHostRequest;

	// 파기 완료를 기다리는 조인 요청. 결과는 값으로 — 재검색하면 SessionSearch 가 갈린다.
	struct FPendingJoinRequest
	{
		FOnlineSessionSearchResult Result;
		bool bValid = false;
	};
	FPendingJoinRequest PendingJoinRequest;

	void StartHostSession(int32 MaxPlayers, bool bLAN, const FString& RoomName);
	void StartJoinSession(const FOnlineSessionSearchResult& Result);

	// 리슨 시작. 이미 게임 맵이면 그 자리에서 열고, 메인메뉴면 게임 맵으로 한 번 이동한다.
	bool StartListen();
	bool StartListenInPlace();

	// 세션이 열리는 게임 맵. 메인메뉴에서 호스트할 때 이 맵으로 이동한다.
	// 맵이 정해지면 DefaultGame.ini [/Script/TR.TRGameInstance] 에서 덮어쓴다.
	UPROPERTY(Config)
	FString GameMapPath = TEXT("/Game/Developers/rkdxh/TestMap");

	UPROPERTY(Config)
	FString MainMenuMapPath = TEXT("/Game/Developers/rkdxh/TestMap");

	// 재접속 식별용 열쇠. 처음 한 번 만들어 사용자 설정에 저장하고 계속 쓴다.
	UPROPERTY(Config)
	mutable FString PlayerKey;

	// 접속 끊김 시 남은 로컬 세션 파기 — 없으면 이후 방 생성이 계속 거부된다.
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType, const FString& ErrorString);
	FDelegateHandle NetworkFailureHandle;

	// 친구 초대 수락 — 오버레이에서 "게임 참가" 를 누르면 여기로 온다.
	void HandleSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId,
		FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	FDelegateHandle InviteAcceptedHandle;

	// OSS 세션 콜백.
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	// 델리게이트 핸들(콜백 1회 후 해제).
	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
};
