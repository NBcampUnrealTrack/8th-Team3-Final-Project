// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BG/Public/Chating/BGChatEntry.h"
#include "BGChatingSubsystem.generated.h"

class AGameStateBase;
class ABGGameState;

/**
 * 
 */
UCLASS()
class BG_API UBGChatingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	UFUNCTION(BlueprintCallable, Category = "BG|Chat")
	void SendMessage(const FString& Message);
	
	UPROPERTY(BlueprintAssignable, Category = "BG|Chat")
	FOnBGChatEntryReceived OnMessageReceived;
	
	UFUNCTION(BlueprintPure, Category = "BG|Chat")
	const TArray<FBGChatEntry>& GetHistory() const { return History; }
	
	UFUNCTION(BlueprintCallable, Category = "BG|Chat")
	void AddSystemMessage(const FString& Message);
	
private:
	
	void HandleGameStateSet(AGameStateBase* GS);
	
	UFUNCTION()
	void HandleChatFromGameState(const FString& SenderName, const FString& Message);
	
	/** 트래블로 새 월드가 로드됐다. 거기 GameState 를 다시 잡는다. */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** 이 월드의 GameState 통지를 구독한다. 이미 와 있으면 바로 잡는다. */
	void BindToWorld(UWorld* World);

	FDelegateHandle PostLoadMapHandle;
	
	TWeakObjectPtr<ABGGameState> BoundGameState;
	
	TArray<FBGChatEntry> History;
	
	
};
