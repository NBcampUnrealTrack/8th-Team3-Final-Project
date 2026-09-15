// TRVoiceComponent.h
// 무전 입력만 담당한다. 어떤 값을 쓸지는 UTRVoiceSubsystem 이 정한다.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Components/ActorComponent.h"
#include "TRVoiceComponent.generated.h"

class APlayerController;

/** 로컬 PlayerController 에 붙는 무전 입력 컴포넌트. UTRVoiceSubsystem 이 붙인다. */
UCLASS(Config = Game, ClassGroup = (TR), meta = (BlueprintSpawnableComponent))
class TR_API UTRVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTRVoiceComponent();

	/** 소유 컨트롤러의 입력 컴포넌트에 키를 건다. 이미 걸려 있으면 건너뛰고, 입력 컴포넌트가 없으면 false. */
	bool BindKeys();

	UFUNCTION(BlueprintCallable, Category = "TR|Voice")
	void StartPushToTalk();

	UFUNCTION(BlueprintCallable, Category = "TR|Voice")
	void StopPushToTalk();

	/** 오픈마이크로 바뀌면 눌린 상태와 무관하게 송신을 연다. 서브시스템이 부른다. */
	void ApplyTransmitMode(bool bPushToTalk);

	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	bool IsPushToTalkHeld() const { return bPushToTalkHeld; }

	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	bool IsBound() const { return bBound; }

	FKey GetPushToTalkKey() const { return PushToTalkKey; }

	FKey GetSettingsUIKey() const { return SettingsUIKey; }

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 서버 보이스 핸드셰이크가 끝나는 순간을 잡아 송신 모드를 다시 적용한다. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void ToggleSettingsUI();

	/** 서브시스템의 현재 모드를 다시 적용한다. 핸드셰이크 뒤 서버가 보낸 송신 끄기를 덮는다. */
	void ReapplyTransmitMode();

	APlayerController* GetOwningController() const;

	/** 무전 키. DefaultGame.ini 의 [/Script/TR.TRVoiceComponent] 에서 바꾼다. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "TR|Voice")
	FKey PushToTalkKey = EKeys::V;

	/** 임시 음성 설정 패널 토글 키. 엔진 디버그 바인딩(F1~F5, F9, F11)과 겹치지 않는 키를 쓴다. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "TR|Voice")
	FKey SettingsUIKey = EKeys::F7;

	FTimerHandle ReapplyTimer;

	bool bBound = false;

	bool bPushToTalkHeld = false;

	bool bHandshakeSeen = false;
};
