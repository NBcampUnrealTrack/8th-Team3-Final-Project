#include "UI/Presentation/BGPresentationDirector.h"

#include "Debug/BGCardLog.h"
#include "Core/BGPlayerController.h"
#include "Core/BGGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/Class.h"   // StaticEnum — 멈춘 트랙을 이름으로 찍는다

namespace
{
	/** 진단용. 어느 트랙이 안 끝났는지 숫자 대신 이름으로 남긴다. */
	FString TrackNames(const TMap<int32, EBGPresentationTrack>& Holds)
	{
		TArray<FString> Names;
		for (const TPair<int32, EBGPresentationTrack>& Hold : Holds)
		{
			Names.AddUnique(StaticEnum<EBGPresentationTrack>()
				->GetDisplayNameTextByValue(static_cast<int64>(Hold.Value)).ToString());
		}
		return FString::Join(Names, TEXT(", "));
	}
}

void FBGPresentationTrackHold::Begin(UBGPresentationDirector* Director, EBGPresentationTrack Track)
{
	if (!Director || Handle.IsValid())
	{
		return;
	}

	Handle = Director->Hold(Track);
}

void FBGPresentationTrackHold::End(UBGPresentationDirector* Director)
{
	if (Director)
	{
		Director->Release(Handle);
	}

	//	⚠️ 디렉터가 없어도 비운다. 없다는 것은 판이 끝났다는 뜻이라, 들고 있어 봐야
	//	   다음 판의 표와 섞일 뿐이다.
	Handle = FBGPresentationHandle();
}

UBGPresentationDirector::UBGPresentationDirector()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UBGPresentationDirector* UBGPresentationDirector::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	ABGPlayerController* PC = World
		? Cast<ABGPlayerController>(World->GetFirstPlayerController())
		: nullptr;

	return PC ? PC->GetPresentationDirector() : nullptr;
}

void UBGPresentationDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//	⚠️ 파괴 뒤에 발동하면 댕글링 접근이다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeoutTimer);
		World->GetTimerManager().ClearTimer(MinDurationTimer);
		World->GetTimerManager().ClearTimer(GraceTimer);
	}

	Super::EndPlay(EndPlayReason);
}

// ── 들어오기 ──────────────────────────────────────────────────────────────────

void UBGPresentationDirector::Enqueue(const FBGPresentationRequest& Request)
{
	if (Request.Kind == EBGPresentationKind::None)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[연출] 종류가 None 인 요청은 무시한다."));
		return;
	}

	//	⚠️ 바닥이 상한보다 크면 모든 연출이 상한에 걸려 경고 로그를 뱉는다. 설정 실수다.
	if (Request.MinDurationSeconds >= Request.TimeoutSeconds)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[연출] 바닥(%.1f초)이 상한(%.1f초) 이상이다 — 바닥을 상한 아래로 낮출 것."),
			Request.MinDurationSeconds, Request.TimeoutSeconds);
	}

	Queue.Add(Request);
	StartNext();
}

void UBGPresentationDirector::EnqueueCardPlay(const FBGCardPlayContext& Context)
{
	FBGPresentationRequest Request;
	Request.Kind               = EBGPresentationKind::CardPlay;
	Request.Context            = Context;
	Request.MinDurationSeconds = CardPlayMinDuration;
	if (Context.Roll != INDEX_NONE)
	{
		const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
		Request.TimeoutSeconds = FMath::Max(Request.TimeoutSeconds,
			(GS ? GS->GetResolutionPresentationSeconds() : 12.f) + 8.f);
	}

	Enqueue(Request);
}

// ── 트랙 ──────────────────────────────────────────────────────────────────────

FBGPresentationHandle UBGPresentationDirector::Hold(EBGPresentationTrack Track)
{
	FBGPresentationHandle Handle;
	Handle.Id    = NextHandleId++;
	Handle.Track = Track;

	//	연출 전에 잡은 표는 다음 연출로 넘어간다 — 몽타주가 카드보다 먼저 시작하기 때문이다.
	//	그 사이에 Release 되면 PendingHolds 에서 빠지므로 다음 연출을 붙잡지 않는다.
	(bPresenting ? ActiveHolds : PendingHolds).Add(Handle.Id, Track);

	//	교대가 이뤄졌다 — 앞 트랙이 놓이고 이 트랙이 이어받았으므로 유예를 취소한다.
	if (bPresenting)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(GraceTimer);
		}
	}

	return Handle;
}

void UBGPresentationDirector::Release(FBGPresentationHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	//	아직 시작 전인 표. 인계 목록에서 빼는 것으로 끝이다.
	if (PendingHolds.Remove(Handle.Id) > 0)
	{
		return;
	}

	//	이미 놓았거나 앞 연출의 표다. 중복 호출은 흔하므로 조용히 넘어간다.
	if (!bPresenting || ActiveHolds.Remove(Handle.Id) == 0)
	{
		return;
	}

	if (ActiveHolds.IsEmpty())
	{
		ScheduleFinishAfterGrace();
	}
}

// ── 진행 ──────────────────────────────────────────────────────────────────────

void UBGPresentationDirector::StartNext()
{
	//	⚠️ 재진입 금지 + 반복문. 트랙을 아무도 잡지 않은 연출은 시작하자마자 끝나고,
	//	   그 FinishActive 가 다시 여기를 부른다 — 막지 않으면 줄 길이만큼 스택이 쌓인다.
	if (bAdvancing)
	{
		return;   // 위 프레임의 반복문이 이어서 처리한다.
	}
	TGuardValue<bool> Guard(bAdvancing, true);

	while (!bPresenting && !Queue.IsEmpty())
	{
		Active = Queue[0];
		Queue.RemoveAt(0);

		bPresenting = true;

		//	연출이 시작되기 **전에** 잡힌 표를 물려받는다. 몽타주는 카드가 중앙에 닿기 전에
		//	시작하므로(GameplayCue 가 판정 통지와 함께 온다) 이 인계가 없으면 그 표가 버려진다.
		ActiveHolds = MoveTemp(PendingHolds);
		PendingHolds.Reset();

		//	⚠️ 바닥을 먼저 건다. 통지 뒤로 미루면 그 사이 마지막 표가 놓여 연출이 끝나 버린다.
		if (Active.MinDurationSeconds > 0.f)
		{
			MinDurationHold = Hold(EBGPresentationTrack::UI);
		}

		if (UWorld* World = GetWorld())
		{
			//	⚠️ 상한이 없으면 Release 하나만 빠져도 큐가 영영 멈춘다(에러 없이).
			World->GetTimerManager().SetTimer(TimeoutTimer, this,
				&UBGPresentationDirector::HandleTimeout,
				FMath::Max(0.1f, Active.TimeoutSeconds), /*bLoop=*/false);

			if (MinDurationHold.IsValid())
			{
				World->GetTimerManager().SetTimer(MinDurationTimer, this,
					&UBGPresentationDirector::HandleMinDurationElapsed,
					Active.MinDurationSeconds, /*bLoop=*/false);
			}
		}

		//	⚠️ 통지 도중에는 끝내지 않는다. 구독자가 그 자리에서 Release 해 마지막 표가
		//	   놓이면, 아직 시작 통지를 못 받은 구독자가 종료 통지를 먼저 받는다.
		//	   그 요청은 아래에서 소화한다.
		bFinishRequestedDuringStart = false;
		{
			TGuardValue<bool> BroadcastGuard(bBroadcastingStart, true);
			OnPresentationStarted.Broadcast(Active);
		}

		//	참여하는 트랙이 없거나, 통지 도중에 다 놓였다. 유예를 주고 끝낸다.
		if (bFinishRequestedDuringStart || ActiveHolds.IsEmpty())
		{
			bFinishRequestedDuringStart = false;
			ScheduleFinishAfterGrace();
		}
	}
}

void UBGPresentationDirector::HandleTimeout()
{
	//	⚠️ 여기까지 왔다는 것은 어느 트랙이 Release 를 빠뜨렸다는 뜻이다. 트랙 이름을 남긴다 —
	//	   남기지 않으면 "가끔 연출이 오래 걸린다" 로만 보이고 추적할 실마리가 없다.
	UE_LOG(LogBGCard, Warning,
		TEXT("[연출] 상한 %.1f초 초과 — 안 끝난 트랙: %s (표 %d개). "
		     "그 트랙을 끝내는 주체를 확인할 것."),
		Active.TimeoutSeconds, *TrackNames(ActiveHolds), ActiveHolds.Num());

	FinishActive(/*bTimedOut=*/true);
}

void UBGPresentationDirector::ScheduleFinishAfterGrace()
{
	UWorld* World = GetWorld();
	if (!World || HandoffGraceSeconds <= 0.f)
	{
		FinishActive(/*bTimedOut=*/false);
		return;
	}

	//	⚠️ 곧장 끝내지 않는다. 다음 트랙이 이 사이에 표를 잡으면 Hold 가 이 타이머를 취소한다 —
	//	   판정 대기 → 월드 연출처럼 신호가 서로 다른 경로로 오는 교대를 이어 준다.
	World->GetTimerManager().SetTimer(GraceTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			//	유예가 끝났는데도 비어 있다 = 진짜 끝이다.
			if (ActiveHolds.IsEmpty())
			{
				FinishActive(/*bTimedOut=*/false);
			}
		}),
		HandoffGraceSeconds, /*bLoop=*/false);
}

void UBGPresentationDirector::HandleMinDurationElapsed()
{
	//	바닥을 채웠다. 다른 트랙이 아직 돌고 있으면 그쪽이 끝날 때까지 계속 간다.
	Release(MinDurationHold);
	MinDurationHold = FBGPresentationHandle();
}

void UBGPresentationDirector::FinishActive(bool bTimedOut)
{
	if (!bPresenting)
	{
		return;
	}

	//	⚠️ 시작 통지를 뿌리는 중이다. 여기서 끝내면 통지 순서가 뒤집히므로 미뤄 둔다 —
	//	   StartNext 가 통지를 마친 직후 소화한다.
	if (bBroadcastingStart)
	{
		bFinishRequestedDuringStart = true;
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeoutTimer);
		World->GetTimerManager().ClearTimer(MinDurationTimer);
		World->GetTimerManager().ClearTimer(GraceTimer);
	}

	const FBGPresentationRequest Finished = Active;

	bPresenting = false;
	ActiveHolds.Reset();
	MinDurationHold = FBGPresentationHandle();
	Active = FBGPresentationRequest();

	//	⚠️ 상태를 먼저 비우고 통지한다. 구독자가 통지를 받고 곧바로 다음 연출을
	//	   Enqueue 하는 것이 정상 흐름이라, 상태가 남아 있으면 그 요청이 밀린다.
	OnPresentationFinished.Broadcast(Finished);

	StartNext();
}
