// Fill out your copyright notice in the Description page of Project Settings.


#include "BG/Public/Chating/BGChatingSubsystem.h"

#include "Ping/BGPingTypes.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "UObject/UObjectGlobals.h"

#include "BG/Public/Core/BGGameState.h"
#include "BG/Public/Core/BGPlayerController.h"
#include "Audio/BGAudioSubsystem.h"
#include "BGGameplayTags.h"

void UBGChatingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	//	GameInstanceSubsystem 은 한 번만 초기화된다. 그때 월드는 메인메뉴다.
	//	맵을 갈아타면 월드 객체가 통째로 새로 생기므로, 처음 월드에만 걸어 두면
	//	게임 맵의 GameState 를 영영 못 잡는다. 트래블마다 다시 건다.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UBGChatingSubsystem::HandlePostLoadMap);

	if (UGameInstance* GI = GetGameInstance(); GI && GI->GetWorld()
		&& GI->GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		BindToWorld(GI->GetWorld());
	}
}

void UBGChatingSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GameStateSetEvent.RemoveAll(this);
		}
	}

	if (ABGGameState* GS = BoundGameState.Get())
	{
		GS->OnChatMessageReceived.RemoveAll(this);
	}

	Super::Deinitialize();
}

void UBGChatingSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	//	PostLoadMap 은 프로세스 전역 통지다. PIE 처럼 한 프로세스에 게임 인스턴스가 여럿이면
	//	남의 월드까지 오므로, 내 월드만 잡는다 — 아니면 전원이 마지막에 로드된 GameState 에 붙는다.
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	BindToWorld(LoadedWorld);
}

void UBGChatingSubsystem::BindToWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	//	중복 구독을 막고 새로 건다.
	World->GameStateSetEvent.RemoveAll(this);
	World->GameStateSetEvent.AddUObject(this, &UBGChatingSubsystem::HandleGameStateSet);

	//	리슨서버 호스트는 GameState 가 이미 와 있다 — 통지를 기다릴 필요가 없다.
	//	클라이언트는 보통 아직 없고, 나중에 GameStateSetEvent 로 잡힌다.
	if (AGameStateBase* GS = World->GetGameState())
	{
		HandleGameStateSet(GS);
	}
}

void UBGChatingSubsystem::HandleGameStateSet(AGameStateBase* GS)
{
	ABGGameState* BGGS = Cast<ABGGameState>(GS);
	if (!BGGS || BoundGameState == BGGS)
	{
		return;
	}

	if (ABGGameState* Prev = BoundGameState.Get())
	{
		Prev->OnChatMessageReceived.RemoveAll(this);
	}

	BGGS->OnChatMessageReceived.AddUObject(this, &UBGChatingSubsystem::HandleChatFromGameState);
	BoundGameState = BGGS;

	UE_LOG(LogTemp, Warning, TEXT("[Chat] GameState 바인딩 성공: %s"), *GetNameSafe(BGGS));
}

void UBGChatingSubsystem::HandleChatFromGameState(const FString& SenderName, const FString& Message, bool bSpectator)
{
	FBGChatEntry Entry;
	Entry.SenderName = SenderName;
	Entry.Message = Message;
	Entry.Timestamp = FDateTime::Now();
	Entry.bSpectator = bSpectator;

	History.Add(Entry);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UBGAudioSubsystem* Audio = GI->GetSubsystem<UBGAudioSubsystem>())
		{
			Audio->Play(BG::Audio::UI::Chat::Message.GetTag());
		}
	}

	OnMessageReceived.Broadcast(Entry);
	
	UE_LOG(LogTemp, Warning, TEXT("[Chat] GameState 로부터 수신: %s"), *Message);
}

void UBGChatingSubsystem::SendMessage(const FString& Message)
{
	if (Message.IsEmpty())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetPrimaryPlayerController() : nullptr;

	if (ABGPlayerController* BGPC = Cast<ABGPlayerController>(PC))
	{
		BGPC->ServerSendChatMessage(Message);
	}
}

void UBGChatingSubsystem::AddSystemMessage(const FString& Message)
{
	if (Message.IsEmpty())
	{
		return;
	}
	
	FBGChatEntry Entry;
	Entry.Message = Message;
	Entry.Timestamp = FDateTime::Now();
	Entry.bSystem = true;
	
	History.Add(Entry);
	
	OnMessageReceived.Broadcast(Entry);
	
	UE_LOG(LogTemp, Warning, TEXT("[Chat] AddSystemMessage 호출: %s"), *Message);
}

void UBGChatingSubsystem::AddPingEntry(const FString& Message, FGameplayTag PingTag, int32 SenderSeat, int32 TargetSeat,
	FLinearColor SenderColor)
{
	if (Message.IsEmpty())
	{
		return;
	}

	FBGChatEntry Entry;
	Entry.Message = Message;
	Entry.Timestamp = FDateTime::Now();
	Entry.bSystem = true;
	Entry.bPing = true;
	Entry.PingTag = PingTag;
	Entry.SenderSeat = SenderSeat;
	Entry.TargetSeat = TargetSeat;
	//	색을 안 넘기면 좌석 팔레트에서 고른다 — 마커 배지와 같은 표.
	Entry.SenderColor = (SenderColor.A > 0.f) ? SenderColor : BGPing::GetSeatColor(SenderSeat);

	History.Add(Entry);

	OnMessageReceived.Broadcast(Entry);
}
