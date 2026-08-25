// TRMenuLeave.cpp

#include "UI/TRMenuLeave.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Session/Interface_TRSession.h"
#include "Session/TRSessionFlow.h"

bool TRMenuLeave::LeaveSessionAndReturnToMenu(const UObject* WorldContextObject, bool bReloadWhenStandalone)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (World == nullptr)
	{
		return false;
	}

	// 세션을 남긴 채 나가면 다음 실행에서 유령 방이 남는다.
	UGameInstance* GI = World->GetGameInstance();
	if (IInterface_TRSession* Session = GI ? GI->GetSubsystem<UTRSessionFlow>() : nullptr)
	{
		Session->LeaveSession();
	}

	// LeaveSession 이 이미 트래블을 걸었으면 여기서 또 열면 둘이 겹친다.
	const FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
	if (Context != nullptr && !Context->TravelURL.IsEmpty())
	{
		return true;
	}

	if (World->GetNetMode() == NM_Standalone && !bReloadWhenStandalone)
	{
		return false;
	}

	// PIE 에서는 패키지 이름에 UEDPIE_N_ 이 붙으므로 벗겨야 열린다.
	UGameplayStatics::OpenLevel(World, FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())));
	return true;
}
