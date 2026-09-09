#include "UI/Presentation/BGPresentationDirector.h"

#include "Debug/BGCardLog.h"
#include "Engine/World.h"
#include "TimerManager.h"

UBGPresentationDirector::UBGPresentationDirector()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBGPresentationDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//	⚠️ 파괴 뒤에 발동하면 댕글링 접근이다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeoutTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UBGPresentationDirector::Enqueue(const FBGPresentationRequest& Request)
{
	if (Request.Kind == EBGPresentationKind::None)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[연출] 종류가 None 인 요청은 무시한다."));
		return;
	}

	Queue.Add(Request);
	StartNext();
}

void UBGPresentationDirector::EnqueueCardPlay(const FBGCardPlayContext& Context)
{
	FBGPresentationRequest Request;
	Request.Kind    = EBGPresentationKind::CardPlay;
	Request.Context = Context;

	Enqueue(Request);
}

void UBGPresentationDirector::StartNext()
{
	if (bPresenting || Queue.IsEmpty())
	{
		return;
	}

	Active = Queue[0];
	Queue.RemoveAt(0);

	bPresenting = true;
	ActiveHolds.Reset();

	//	월드 연출(시네마틱·Cue)이 끝났다고 알려 줄 때까지 잡아 두는 기본 표.
	//	이게 없으면 구독자가 하나도 Hold 하지 않은 연출이 시작하자마자 끝난다.
	WorldFXHold = Hold();

	//	⚠️ 상한이 없으면 Release 하나만 빠져도 큐가 영영 멈춘다(에러 없이).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimeoutTimer, this,
			&UBGPresentationDirector::HandleTimeout,
			FMath::Max(0.1f, Active.TimeoutSeconds), /*bLoop=*/false);
	}

	OnPresentationStarted.Broadcast(Active);
}

FBGPresentationHandle UBGPresentationDirector::Hold()
{
	if (!bPresenting)
	{
		//	진행 중인 연출이 없다. 무효 표를 주면 Release 가 조용히 넘어간다 —
		//	몽타주 종료처럼 연출과 무관한 때에도 오는 신호가 있기 때문이다.
		return FBGPresentationHandle();
	}

	FBGPresentationHandle Handle;
	Handle.Id = NextHandleId++;
	ActiveHolds.Add(Handle.Id);

	return Handle;
}

void UBGPresentationDirector::Release(FBGPresentationHandle Handle)
{
	if (!bPresenting || !Handle.IsValid())
	{
		return;
	}

	//	이미 놓았거나 앞 연출의 표다. 중복 호출은 흔하므로 조용히 넘어간다.
	if (ActiveHolds.Remove(Handle.Id) == 0)
	{
		return;
	}

	if (ActiveHolds.IsEmpty())
	{
		FinishActive(/*bTimedOut=*/false);
	}
}

void UBGPresentationDirector::NotifyWorldFXFinished()
{
	Release(WorldFXHold);
	WorldFXHold = FBGPresentationHandle();
}

void UBGPresentationDirector::HandleTimeout()
{
	//	⚠️ 여기까지 왔다는 것은 구독자가 Release 를 빠뜨렸다는 뜻이다. 원인을 남긴다 —
	//	   남기지 않으면 "가끔 연출이 오래 걸린다" 로만 보이고 추적할 실마리가 없다.
	UE_LOG(LogBGCard, Warning,
		TEXT("[연출] 상한 %.1f초 초과 — 표 %d개가 안 놓였다. 종류 %d. "
		     "Hold 를 잡고 Release 를 빠뜨린 구독자를 확인할 것."),
		Active.TimeoutSeconds, ActiveHolds.Num(), static_cast<int32>(Active.Kind));

	FinishActive(/*bTimedOut=*/true);
}

void UBGPresentationDirector::FinishActive(bool bTimedOut)
{
	if (!bPresenting)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeoutTimer);
	}

	const FBGPresentationRequest Finished = Active;

	bPresenting = false;
	ActiveHolds.Reset();
	WorldFXHold = FBGPresentationHandle();
	Active      = FBGPresentationRequest();

	//	⚠️ 상태를 먼저 비우고 통지한다. 구독자가 통지를 받고 곧바로 다음 연출을
	//	   Enqueue 하는 것이 정상 흐름이라, 상태가 남아 있으면 그 요청이 밀린다.
	OnPresentationFinished.Broadcast(Finished);

	StartNext();
}
