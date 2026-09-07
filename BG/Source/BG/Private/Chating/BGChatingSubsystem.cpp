// Fill out your copyright notice in the Description page of Project Settings.


#include "BG/Public/Chating/BGChatingSubsystem.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "UObject/UObjectGlobals.h"

#include "BG/Public/Core/BGGameState.h"
#include "BG/Public/Core/BGPlayerController.h"

void UBGChatingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	//	GameInstanceSubsystem 은 한 번만 초기화된다. 그때 월드는 메인메뉴다.
	//	맵을 갈아타면 월드 객체가 통째로 새로 생기므로, 처음 월드에만 걸어 두면
	//	게임 맵의 GameState 를 영영 못 잡는다. 트래블마다 다시 건다.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UBGChatingSubsystem::HandlePostLoadMap);

	if (UGameInstance* GI = GetGameInstance())
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

void UBGChatingSubsystem::HandleChatFromGameState(const FString& SenderName, const FString& Message)
{
	FBGChatEntry Entry;
	Entry.SenderName = SenderName;
	Entry.Message = Message;
	Entry.Timestamp = FDateTime::Now();

	History.Add(Entry);

	OnMessageReceived.Broadcast(Entry);
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
}
