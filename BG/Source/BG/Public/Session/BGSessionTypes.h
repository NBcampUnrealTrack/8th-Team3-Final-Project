// BANG! 세션 공용 타입 — UBGGameInstance(OSS 호출부)와 UBGSessionFlow(UI 계층)가 공유한다.

#pragma once

#include "CoreMinimal.h"
#include "BGSessionTypes.generated.h"

// 세션 진행 단계 — UI 로딩 표시/에러 토스트용.
UENUM(BlueprintType)
enum class EBGSessionPhase : uint8
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
struct FBGSessionSearchResult
{
	GENERATED_BODY()

	// 광고된 방 이름(미광고 시 호스트 표시명 폴백).
	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	FString RoomName;

	// 호스트 표시명(온라인 서비스 계정 표시명).
	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	FString HostDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	int32 MaxPlayers = 0;

	// 검색 응답 왕복 핑(ms).
	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	int32 PingMs = 0;

	// 검색 캐시 인덱스 — JoinSessionByIndex 에 넘긴다.
	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	int32 ResultIndex = INDEX_NONE;

	// 빈자리 있고 판이 진행 중이 아님.
	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	bool bCanJoin = false;
};

// 로비 현황 — UI 가 인원/준비 표시와 시작 버튼 활성에 쓴다.
USTRUCT(BlueprintType)
struct FBGLobbyStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	int32 ReadyPlayers = 0;

	// 시작에 필요한 최소 인원(서버 설정). 호스트가 아니면 0.
	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	int32 RequiredPlayers = 0;

	// 호스트 기준 판 진행 여부. 클라에서는 항상 false.
	UPROPERTY(BlueprintReadOnly, Category = "BG|Session")
	bool bMatchStarted = false;
};

// 방찾기 검색 필터 — 문자열 필터는 결과 수신 후 클라 측 적용(OSS 쿼리는 부분검색 미지원).
USTRUCT(BlueprintType)
struct FBGSessionSearchFilter
{
	GENERATED_BODY()

	// 방 이름 부분 문자열(대소문자 무시, 비우면 전체).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Session")
	FString RoomNameQuery;

	// true 면 빈자리 있는 방만.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Session")
	bool bHideFull = false;

	// 필터 적용 후 최대 결과 수(0 이하 = 무제한).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Session")
	int32 MaxResults = 20;
};

// ── 세션 이벤트 델리게이트 ──

// 세션 비동기 결과 통지(호스트/조인 버튼 UI 갱신용).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBGSessionBoolEvent, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBGFindSessionsEvent, bool, bSuccess, int32, NumFound);

// 세션 단계 변화 통지(로딩 스피너/에러 표시용).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBGSessionPhaseChanged, EBGSessionPhase, Phase, const FString&, Message);

// 방찾기 목록 갱신 완료 통지(필터 적용 결과, 실패 시 빈 배열).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGSessionListReady, const TArray<FBGSessionSearchResult>&, Results);
