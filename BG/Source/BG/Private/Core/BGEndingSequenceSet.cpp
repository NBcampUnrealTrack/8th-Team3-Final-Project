#include "Core/BGEndingSequenceSet.h"

#include "Debug/BGCardLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ABGEndingSequenceSet::ABGEndingSequenceSet()
{
	//	값만 들고 있는 액터다. 틱도 복제도 필요 없다.
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

ULevelSequence* ABGEndingSequenceSet::FindSequence(FGameplayTag WinningRole) const
{
	if (!WinningRole.IsValid())
	{
		return nullptr;
	}

	const TObjectPtr<ULevelSequence>* Found = Sequences.Find(WinningRole);
	return Found ? Found->Get() : nullptr;
}

ABGEndingSequenceSet* ABGEndingSequenceSet::Find(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	if (!World)
	{
		return nullptr;
	}

	ABGEndingSequenceSet* First = nullptr;
	for (TActorIterator<ABGEndingSequenceSet> It(World); It; ++It)
	{
		if (!First)
		{
			First = *It;
			continue;
		}

		//	둘을 두면 어느 쪽이 쓰였는지 화면만 보고는 알 수 없다 — 그 자리에서 드러낸다.
		UE_LOG(LogBGCard, Warning,
			TEXT("[Ending] 엔딩 시퀀스 목록이 맵에 둘 이상 있습니다(%s, %s) — 첫 번째만 씁니다."),
			*First->GetName(), *(*It)->GetName());
		break;
	}

	return First;
}
