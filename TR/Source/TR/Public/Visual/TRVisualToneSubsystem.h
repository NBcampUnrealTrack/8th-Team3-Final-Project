// TRVisualToneSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "TRTypes.h"
#include "TRVisualToneSubsystem.generated.h"

class ATRGameState;
class ATRTrain;
class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;

/**
 * 머테리얼이 읽을 스칼라 파라미터 이름.
 *
 * **MPC 애셋의 파라미터 이름과 글자 하나까지 같아야 한다.** 어긋나면 엔진이 경고 없이
 * 무시하고 화면만 그대로다 — 값이 안 변하는데 로그도 안 남는 종류의 실패라, 양쪽이
 * 같은 문자열을 본다는 사실을 여기 한 곳에 적어 둔다.
 */
namespace TRVisualToneParams
{
	/** 0 = 평온(로비·휴식), 1 = 전투 중. 채도·포그·림라이트를 여기에 묶는다. */
	inline const FName WaveTension(TEXT("WaveTension"));

	/** 0 = 선체 멀쩡, 1 = 파괴 직전. 화면 가장자리 붉은 비네트용. */
	inline const FName HullDanger(TEXT("HullDanger"));

	/** 웨이브가 시작되는 순간 1로 튀었다가 0으로 감쇠. 짧은 붉은 섬광용. */
	inline const FName WavePulse(TEXT("WavePulse"));

	/**
	 * 지면이 흐르는 속도. 0 = 정지(로비), 1 = 기준 속도.
	 *
	 * **트레드밀 구조에서 속도감을 만드는 유일한 근거다.** 기차는 월드 좌표상 멈춰 있고
	 * 지형이 흐르므로, 카메라도 기차도 "빠르다"는 신호를 내지 않는다. 엔진 모션블러도
	 * 기대할 수 없다 — 청크가 재활용될 때 순간이동해서 그 프레임의 모션벡터가 튄다.
	 * 속도선·UV 스트레치처럼 머테리얼이 직접 그리는 수밖에 없고, 그 재료가 이 값이다.
	 */
	inline const FName GroundSpeed(TEXT("GroundSpeed"));
}

/**
 * 톤 전환의 수치. 프로젝트 세팅 > Game > TR Visual Tone 에서 조정한다.
 *
 * 서브시스템에 UPROPERTY 로 두지 않는 이유는 월드 서브시스템에 에디터가 여는 기본값
 * 인스턴스가 없기 때문이다 — 레벨마다 액터를 놓게 하면 빠뜨린 레벨에서만 조용히
 * 톤이 안 바뀐다. 프로젝트 단위 설정이 이 값들의 실제 성격이기도 하다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "TR Visual Tone"))
class TR_API UTRVisualToneSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/**
	 * 값을 밀어 넣을 파라미터 컬렉션. 비어 있으면 서브시스템이 아무것도 하지 않는다.
	 * (경고는 한 번만 남긴다 — 매 틱 남기면 로그가 못 쓰게 된다.)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Visual Tone")
	TSoftObjectPtr<UMaterialParameterCollection> ToneCollection;

	/** 전투 -> 휴식 전환에 걸리는 시간(초). 즉시 바뀌면 조명이 튄 것처럼 보인다. */
	UPROPERTY(Config, EditAnywhere, Category = "Visual Tone", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float TensionTransitionTime = 2.5f;

	/**
	 * 선체 체력이 이 비율 **아래로** 내려가면 위험 표시가 켜지기 시작한다.
	 * 1.0 으로 두면 첫 피격부터 화면이 붉어져 위험 신호의 의미가 사라진다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Visual Tone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HullDangerThreshold = 0.35f;

	/** 웨이브 시작 섬광이 사라지기까지의 시간(초). 0이면 섬광을 쓰지 않는다. */
	UPROPERTY(Config, EditAnywhere, Category = "Visual Tone", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float WavePulseDuration = 0.8f;

	/**
	 * GroundSpeed 가 1.0 이 되는 지면 속도(cm/s).
	 *
	 * ATRLevelSceneManager::VisualSpeed 의 기본값과 맞춰 둔다. 여기를 기준으로 정규화하는
	 * 이유는 머테리얼이 원시 cm/s 로는 아무것도 못 하기 때문이다 — Lerp 도 마스크도
	 * 0~1 을 전제한다. 원시 값이 필요한 게임플레이 코드는 UTRGroundMotionSubsystem 을
	 * 직접 읽으면 된다.
	 *
	 * 연출상 기차를 더 빠르게 굴리면 이 값도 같이 올린다. 안 올리면 1.0 에 붙어 버려
	 * 속도가 더 올라가도 화면이 반응하지 않는다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Visual Tone", meta = (ClampMin = "1.0", ForceUnits = "cm/s"))
	float SpeedReference = 1000.f;

	/**
	 * 지면 속도가 화면에 반영되기까지의 시간(초).
	 *
	 * 지면 속도는 판이 시작하는 순간 0에서 최고 속도로 **한 프레임 만에 뛴다**
	 * (ATRLevelSceneManager 가 판 시작 전에는 0을 낸다). 그대로 쓰면 속도선이
	 * 갑자기 튀어나와 출발한 것이 아니라 화면이 깨진 것처럼 보인다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Visual Tone", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SpeedTransitionTime = 1.2f;
};

/**
 * 웨이브 진행 상태와 선체 상태를 **머테리얼 파라미터 컬렉션 하나로** 옮긴다.
 *
 * 이게 없으면 톤을 바꾸려는 머테리얼마다 게임 상태를 직접 찾아야 하는데, 머테리얼은
 * 그럴 수단이 없어서 결국 블루프린트가 개별 다이내믹 인스턴스를 들고 돌아다니게 된다.
 * MPC 는 값 하나를 쓰면 그걸 참조하는 모든 머테리얼이 같은 프레임에 따라오므로,
 * "지금 판이 어떤 분위기인가"를 한 곳에서만 정하면 된다.
 *
 * **클라이언트 표현 전용이다.** 데디케이티드 서버에서는 틱하지 않는다.
 */
UCLASS()
class TR_API UTRVisualToneSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 지금 긴장도(0~1). 카메라 셰이크처럼 머테리얼 밖에서 같은 값을 쓰고 싶을 때 읽는다.
	 * MPC 를 되읽는 것보다 싸고, 보간 규칙이 갈라지지 않는다.
	 */
	UFUNCTION(BlueprintPure, Category = "TR|Visual")
	float GetWaveTension() const { return Tension; }

	UFUNCTION(BlueprintPure, Category = "TR|Visual")
	float GetHullDanger() const { return HullDanger; }

	/** 정규화된 지면 속도(0~1). 기준은 UTRVisualToneSettings::SpeedReference. */
	UFUNCTION(BlueprintPure, Category = "TR|Visual")
	float GetGroundSpeed() const { return GroundSpeed; }

	/**
	 * 웨이브 시작 섬광을 강제로 터뜨린다.
	 *
	 * 평소에는 웨이브 번호 변경이 알아서 터뜨리므로 부를 일이 없다. 보스 등장이나
	 * 선체 대파처럼 웨이브 경계가 아닌 순간에 같은 연출을 쓰고 싶을 때를 위해 연다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TR|Visual")
	void TriggerPulse() { Pulse = 1.f; }

	// UTickableWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/**
	 * 게임 상태를 아직 못 찾았으면 찾아 델리게이트를 건다.
	 *
	 * Initialize 시점에는 GameState 가 아직 복제돼 오지 않았을 수 있다. 그때 한 번
	 * 실패하고 마는 대신 붙을 때까지 매 틱 다시 시도한다 — 클라이언트에서 이 액터가
	 * 언제 도착하는지는 접속 상황에 달려 있어 미리 정할 수 없다.
	 *
	 * @return 게임 상태에 연결돼 있으면 true.
	 */
	bool EnsureGameStateBound();

	/** 선체 비율(0~1)을 읽는다. 기차를 못 찾으면 1(멀쩡)로 본다. */
	float ResolveHullRatio();

	/** 세 값을 MPC 에 쓴다. 값이 거의 안 바뀌었으면 건너뛴다. */
	void PushToCollection();

	UFUNCTION()
	void HandleWaveNumberChanged(int32 WaveNumber);

	UFUNCTION()
	void HandleWavePhaseChanged(ETRWavePhase Phase);

	UPROPERTY()
	TWeakObjectPtr<ATRGameState> CachedGameState;

	/** 선체 체력은 복제되지만 알림이 없다. 값 하나라 매 틱 읽는 편이 델리게이트보다 싸다. */
	UPROPERTY()
	TWeakObjectPtr<ATRTrain> CachedTrain;

	float Tension = 0.f;
	float TargetTension = 0.f;
	float HullDanger = 0.f;
	float Pulse = 0.f;
	float GroundSpeed = 0.f;

	/** MPC 에 마지막으로 쓴 값. 셋 다 문턱 안이면 쓰기를 통째로 건너뛴다. */
	float LastPushedTension = -1.f;
	float LastPushedDanger = -1.f;
	float LastPushedPulse = -1.f;
	float LastPushedSpeed = -1.f;

	/** 다음으로 기차를 찾아볼 월드 시각(초). 기차가 없는 레벨에서 매 틱 순회하지 않게 한다. */
	float NextTrainSearchTime = 0.f;

	/** 기차 찾기 재시도 간격(초). 기차는 레벨 로드와 함께 있거나 아예 없다. */
	static constexpr float TrainSearchInterval = 1.f;

	/**
	 * 해석해 둔 컬렉션 인스턴스. 소프트 참조를 매 틱 푸는 것을 피한다.
	 *
	 * 약참조인 이유는 인스턴스의 수명을 월드가 쥐고 있기 때문이다 — 레벨 전환에서
	 * 서브시스템보다 먼저 사라질 수 있다.
	 */
	UPROPERTY()
	TWeakObjectPtr<UMaterialParameterCollectionInstance> CachedInstance;

	/** 컬렉션이 비었거나 인스턴스를 못 얻었다고 이미 알렸는지. 매 틱 로그를 막는다. */
	bool bWarnedMissingCollection = false;

	/**
	 * 이만큼 안 바뀌면 쓰지 않는다. MPC 쓰기는 그 컬렉션을 참조하는 머테리얼 전부의
	 * 유니폼 버퍼를 갱신시키므로, 휴식 중처럼 값이 멈춰 있을 때 매 프레임 쓸 이유가 없다.
	 */
	static constexpr float PushThreshold = 0.002f;
};
