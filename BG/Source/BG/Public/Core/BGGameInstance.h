// BANG! 게임 인스턴스 — OnlineSubsystem 세션 호출부. 방 생성/검색/조인/파기 파이프라인을 담당한다.
// 서브시스템별 옵션 분기는 FBGOssPolicy 가 맡는다. 맵 이동은 메인메뉴 → 게임 맵 1 회뿐이고,
// 로비 → 판 전환은 여전히 이동 없이 페이즈 태그로 처리한다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Session/BGSessionTypes.h"
#include "BGGameInstance.generated.h"

class UNetDriver;

UCLASS(Config = Game)
class BG_API UBGGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// ── 세션 ──

	// 방 생성 → 성공 시 현재 월드에서 리슨 시작(맵 이동 없음). RoomName 이 비면 호스트 표시명 기반 기본값.
	// bLAN=false 여도 LAN 전용 서브시스템(NULL)에서는 LAN 으로 내려간다.
	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	void HostSession(int32 MaxPlayers = 7, bool bLAN = false, const FString& RoomName = TEXT(""));

	// 주변 BG 세션 검색 — 결과는 OnFindSessionsComplete 로 통지.
	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	void FindSessions(bool bLAN = false, int32 MaxSearchResults = 20);

	// 직전 검색 결과의 index 세션에 접속(접속 후 자동 ClientTravel).
	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	void JoinFoundSession(int32 SearchResultIndex);

	// 현재 세션 파기(나가기/호스트 종료).
	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	void DestroyCurrentSession();

	// 방을 나간 뒤 메인메뉴 맵으로 복귀. 이미 메인메뉴면 아무것도 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	void ReturnToMainMenu();

	// 판이 시작되면 false 로 — 진행 중인 판에 난입을 막는다.
	UFUNCTION(BlueprintCallable, Category = "BG|Session")
	void SetAllowJoinInProgress(bool bAllow);

	// ── 조회 ──

	// 호스트가 광고 중인 방 코드. 호스트가 아니면 빈 문자열.
	UFUNCTION(BlueprintPure, Category = "BG|Session")
	FString GetHostRoomCode() const { return HostRoomCode; }

	// 검색 결과 세션에 광고된 방 코드(클라이언트 코드 매칭용). 없으면 빈 문자열.
	UFUNCTION(BlueprintPure, Category = "BG|Session")
	FString GetFoundSessionCode(int32 SearchResultIndex) const;

	UFUNCTION(BlueprintPure, Category = "BG|Session")
	int32 GetFoundSessionCount() const;

	// 직전 검색 결과 전체 스냅샷(필터 미적용, ResultIndex=검색 캐시 인덱스).
	UFUNCTION(BlueprintPure, Category = "BG|Session")
	TArray<FBGSessionSearchResult> GetFoundSessionResults() const;

	// ── 결과 이벤트 (UBGSessionFlow 가 구독) ──
	UPROPERTY(BlueprintAssignable, Category = "BG|Session")
	FBGSessionBoolEvent OnCreateSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "BG|Session")
	FBGFindSessionsEvent OnFindSessionsComplete;

	UPROPERTY(BlueprintAssignable, Category = "BG|Session")
	FBGSessionBoolEvent OnJoinSessionComplete;

	// ── 콘솔 테스트 트리거 (구현: BGSessionExec.cpp, Shipping 에서는 no-op) ──
	UFUNCTION(Exec)
	void BG_Host();
	UFUNCTION(Exec)
	void BG_Find();
	UFUNCTION(Exec)
	void BG_Join();
	// 검색 → 코드 매칭 → 조인까지 한 번에. 프로세스 2개로 왕복을 검증할 때 쓴다.
	UFUNCTION(Exec)
	void BG_JoinCode(const FString& Code);
	UFUNCTION(Exec)
	void BG_Leave();
	// 준비 토글(1=준비, 0=해제). 호스트는 항상 준비 상태라 필요 없다.
	UFUNCTION(Exec)
	void BG_Ready(int32 bReady);
	UFUNCTION(Exec)
	void BG_Start();
	UFUNCTION(Exec)
	void BG_RoomCode();
	// 세션 러프 UI 토글(뷰포트에 올리고 마우스 입력을 연다).
	UFUNCTION(Exec)
	void BG_UI();

	//	덱 관련 exec 는 ABGGameMode 로

private:
	// 세션 인터페이스 핸들(없으면 유효하지 않은 포인터).
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
		int32   MaxPlayers = 7;
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

	// 실제 CreateSession 실행부 — 기존 세션이 없는 상태에서만 호출된다.
	void StartHostSession(int32 MaxPlayers, bool bLAN, const FString& RoomName);

	// 실제 JoinSession 실행부 — 기존 세션이 없는 상태에서만 호출된다.
	void StartJoinSession(const FOnlineSessionSearchResult& Result);

	// 리슨 시작. 이미 게임 맵이면 그 자리에서 열고, 메인메뉴처럼 다른 맵이면
	// 게임 맵으로 한 번 이동한다(로비→판 전환은 여전히 이동 없음).
	bool StartListen();

	// 맵을 다시 로드하지 않고 현재 월드를 리슨 서버로 전환한다.
	bool StartListenInPlace();

	// 세션이 열리는 게임 맵. 메인메뉴에서 호스트할 때 이 맵으로 이동한다.
	UPROPERTY(Config)
	FString GameMapPath = TEXT("/Game/GameSystem/TestMap");

	// 방을 나가면 돌아갈 메인메뉴 맵.
	UPROPERTY(Config)
	FString MainMenuMapPath = TEXT("/Game/GameSystem/L_MainMenu");

	// 접속 끊김 시 남은 로컬 세션 파기 — 없으면 이후 방 생성이 계속 거부된다.
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	FDelegateHandle NetworkFailureHandle;

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
