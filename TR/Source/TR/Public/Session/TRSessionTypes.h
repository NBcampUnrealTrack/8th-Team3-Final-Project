// TR 세션 공용 타입 — UTRGameInstance(OSS 호출부)와 UTRSessionFlow(UI 계층)가 공유한다.

#pragma once

#include "CoreMinimal.h"
#include "TRSessionTypes.generated.h"

// 세션 진행 단계 — UI 로딩 표시/에러 토스트용.
UENUM(BlueprintType)
enum class ETRSessionPhase : uint8
{
	Idle,
	Creating,   // 방 생성 중
	Hosting,    // 호스트 성공(리슨 시작)
	Searching,  // 세션 검색 중
	Joining,    // 조인 시도 중
	Joined,     // 조인 성공(클라 합류)
	Failed      // 실패(메시지 동반)
};

// 방찾기 검색 결과 한 건 — ResultIndex 는 직전 검색 캐시 기준(새 검색 시 무효).
USTRUCT(BlueprintType)
struct FTRSessionSearchResult
{
	GENERATED_BODY()

	// 광고된 방 이름(미광고 시 호스트 표시명 폴백).
	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	FString HostDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	int32 MaxPlayers = 0;

	// 검색 응답 왕복 핑(ms).
	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	int32 PingMs = 0;

	// 검색 캐시 인덱스 — JoinSessionByIndex 에 넘긴다.
	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	int32 ResultIndex = INDEX_NONE;

	// 빈자리 있고 난입이 허용된 방.
	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	bool bCanJoin = false;
};

// 로비 현황 — UI 가 인원 표시에 쓴다.
USTRUCT(BlueprintType)
struct FTRLobbyStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	int32 CurrentPlayers = 0;

	//	아래는 판 시작 규칙이 정해져야 채울 수 있다. 규칙이 나오면 주석을 풀고
	//	UTRSessionFlow::GetLobbyStatus 에서 GameState 값을 읽어 넣는다.
	//
	//	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	//	int32 ReadyPlayers = 0;
	//
	//	// 시작에 필요한 최소 인원(서버 설정). 호스트가 아니면 0.
	//	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	//	int32 RequiredPlayers = 0;
	//
	//	// 호스트 기준 판 진행 여부. 클라에서는 항상 false.
	//	UPROPERTY(BlueprintReadOnly, Category = "TR|Session")
	//	bool bMatchStarted = false;
};

// 방찾기 검색 필터 — 문자열 필터는 결과 수신 후 클라 측 적용(OSS 쿼리는 부분검색 미지원).
USTRUCT(BlueprintType)
struct FTRSessionSearchFilter
{
	GENERATED_BODY()

	// 방 이름 부분 문자열(대소문자 무시, 비우면 전체).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Session")
	FString RoomNameQuery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Session")
	bool bHideFull = false;

	// 필터 적용 후 최대 결과 수(0 이하 = 무제한).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Session")
	int32 MaxResults = 20;
};

// ── 세션 이벤트 델리게이트 ──

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTRSessionBoolEvent, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTRFindSessionsEvent, bool, bSuccess, int32, NumFound);

// 세션 단계 변화 통지(로딩 스피너/에러 표시용).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTRSessionPhaseChanged, ETRSessionPhase, Phase, const FString&, Message);

// 방찾기 목록 갱신 완료 통지(필터 적용 결과, 실패 시 빈 배열).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTRSessionListReady, const TArray<FTRSessionSearchResult>&, Results);
