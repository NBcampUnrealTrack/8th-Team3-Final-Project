// TR OSS 정책 — 세션 설정 중 OnlineSubsystem 구현마다 갈리는 항목(로비·프레즌스·LAN 전용)만
// 모아 판정한다. 호출부가 서브시스템 이름을 직접 보지 않게 하는 것이 목적이다.

#pragma once

#include "CoreMinimal.h"

class UWorld;

struct TR_API FTROssPolicy
{
	// 현재 활성 서브시스템(STEAM / NULL / None).
	FName SubsystemName = NAME_None;

	// 세션을 로비 API 로 만들고 검색할 수 있다.
	bool bUseLobbies = false;

	// 프레즌스 기반 합류(친구 초대·오버레이)가 의미를 갖는다.
	bool bUsePresence = false;

	// 이 서브시스템은 LAN 질의로만 서로를 찾을 수 있다(NULL, 또는 서브시스템 부재).
	bool bLanOnly = false;

	// World 필수 — PIE 는 인스턴스마다 별도 서브시스템(null 이면 부재로 취급).
	static FTROssPolicy Resolve(const UWorld* World);

	FString ToString() const;
};
