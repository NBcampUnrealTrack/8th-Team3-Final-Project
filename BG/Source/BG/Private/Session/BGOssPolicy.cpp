#include "Session/BGOssPolicy.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"

FBGOssPolicy FBGOssPolicy::Resolve(const UWorld* World)
{
	FBGOssPolicy Policy;

	// IOnlineSubsystem::Get() 대신 이 헬퍼를 쓴다 — PIE 는 인스턴스마다 별도 서브시스템을 갖는다.
	IOnlineSubsystem* OSS = Online::GetSubsystem(World);
	if (!OSS)
	{
		Policy.bLanOnly = true;
		return Policy;
	}

	Policy.SubsystemName = OSS->GetSubsystemName();

	if (Policy.SubsystemName == STEAM_SUBSYSTEM)
	{
		Policy.bUseLobbies = true;
		Policy.bUsePresence = true;
	}
	else if (Policy.SubsystemName == NULL_SUBSYSTEM)
	{
		// Steam 미실행·-nosteam 폴백. LAN 질의로만 서로를 찾는다.
		Policy.bLanOnly = true;
	}
	// 모르는 서브시스템은 표준 경로만 — 미지원 옵션을 켜면 조용히 실패한다.

	return Policy;
}

FString FBGOssPolicy::ToString() const
{
	return FString::Printf(TEXT("OSS=%s 로비=%d 프레즌스=%d LAN전용=%d"),
		*SubsystemName.ToString(), bUseLobbies, bUsePresence, bLanOnly);
}
