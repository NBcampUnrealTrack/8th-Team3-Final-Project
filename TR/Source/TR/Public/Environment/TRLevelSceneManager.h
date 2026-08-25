// TRLevelSceneManager.h

#pragma once

#include "CoreMinimal.h"
#include "Environment/TRLevelSceneTypes.h"
#include "GameFramework/Actor.h"
// 바이옴 종류(ETRBiomeType).
#include "TRTypes.h"
#include "TRLevelSceneManager.generated.h"

class ATRLevelSceneChunk;
class ATRLevelSceneFloor;
class ATRLevelSceneRailSegment;
class UMaterialInterface;
class UMaterialParameterCollection;
class USceneComponent;
class UTRBiomeEnvironment;
class UTREnvironmentBlenderComponent;
class UStaticMesh;

/**
 * 레벨 환경 자동생성 관리자
 * 할당된 메시들을 규칙에 따라 랜덤배치하며 이동시켜 속도감, 자연스러움 구현
 * ISM 활용하여 제한적인 메시로 재활용하면서 반복 사용
 */

UCLASS(Blueprintable)
class TR_API ATRLevelSceneManager : public AActor
{
	GENERATED_BODY()

public:
	ATRLevelSceneManager();

	virtual void Tick(float DeltaSeconds) override;

	//---변수 Setter---
	UFUNCTION(BlueprintCallable, Category = "Level Scene")
	void SetVisualSpeed(float NewVisualSpeed);

	UFUNCTION(BlueprintCallable, Category = "Level Scene")
	void SetRouteSeed(int32 NewRouteSeed, bool bRegenerate = true);

	UFUNCTION(BlueprintCallable, Category = "Level Scene")
	void RegenerateAllChunks();

	/** 지금 새로 만들어지는 청크가 쓰는 풍경. */
	UFUNCTION(BlueprintPure, Category = "Level Scene|Biome")
	ETRBiomeType GetCurrentBiome() const { return CurrentBiome; }

	/**
	 * 바이옴을 즉시 지정한다. 이미 앞에 깔린 청크는 그대로 두고, 앞으로 만들어지는
	 * 청크부터 적용된다 — 즉 전환이 자연스럽게 흘러온다.
	 *
	 * 평소에는 웨이브 진행이 알아서 부르므로 쓸 일이 없다. 연출이나 디버깅용으로 연다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Scene|Biome")
	void SetCurrentBiome(ETRBiomeType InBiome);

	/**
	 * 지금 배경 상태를 로그에 한 번 찍는다. 콘솔 명령 `TR.Scene.Dump` 가 부른다.
	 *
	 * 화면 표시(`TR.Scene.Debug 1`)와 **같은 문장을 쓴다.** 둘이 따로 놀면 화면에서
	 * 본 것과 로그에 남은 것이 달라져서, 나중에 로그만 보고는 판단을 못 하게 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Scene|Debug")
	void DumpSceneDebug() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	//--- 레벨 초기 설정 함수 --- 
	void CreateInitialChunks();

	/**
	 * GameState 가 알려주는 BiomeId 를 따라간다. 매 틱 불린다.
	 *
	 * 이름을 정하는 것은 서버의 웨이브 테이블이고, 그 결과가 복제되어 여기까지 온다.
	 * 클라이언트가 테이블을 직접 읽지 않는 이유는 행 승계 규칙을 두 곳에서 구현하지
	 * 않기 위해서다 — 규칙이 갈라지면 클라이언트만 다른 풍경을 보게 된다.
	 */
	void UpdateBiomeForWave();

	/** 그 풍경에 실제로 깔릴 소품 세트(공통 + 전용). 미리 합쳐 둔 것을 돌려준다. */
	const TArray<FTRLevelSceneMeshDefinition>& GetBiomeMeshDefinitions(ETRBiomeType InBiome) const;

	/**
	 * 바이옴마다 `공통 + 전용` 을 미리 합쳐 둔다. BeginPlay 에서 한 번 돈다.
	 *
	 * **청크를 재활용할 때마다 합치지 않는 이유**는 두 가지다. 하나는 재활용이 잦아서
	 * 매번 배열을 새로 만들면 낭비라는 것이고, 다른 하나는 합친 결과를 참조로 넘겨야
	 * 하는데 임시 배열의 참조는 넘길 수 없다는 것이다.
	 */
	void RebuildResolvedMeshDefinitions();

	/**
	 * 지면 텍스처를 소품 전환에 맞춰 따라가게 한다. 매 틱 불린다.
	 *
	 * **소품이 도착한 뒤에 시작하는 것이 요점이다.** 소품은 화면 앞에서 생성되어 시간을
	 * 두고 다가오는데, 지면은 발밑까지 한 번에 바뀐다. 동시에 시작하면 기차 밑은 이미
	 * 눈밭인데 주변에는 아직 사막 바위가 서 있는 그림이 몇 초 동안 나온다.
	 */
	void UpdateFloorBiome(float DeltaSeconds);

	/** 새 풍경을 실은 청크가 기차에 닿았는지. 지면 전환을 시작할 시점을 여기서 잡는다. */
	bool HasBiomeReachedTrain(ETRBiomeType InBiome) const;

	/**
	 * 지면 A 슬롯을 이 풍경으로 즉시 맞추고 경계를 화면 밖으로 치운다.
	 *
	 * @param bSettleSurface true 면 날씨도 목표치로 즉시 끝낸다. 레벨을 처음 만들 때는
	 *                       눈이 쌓이는 과정을 보여 줄 이유가 없으므로 그렇게 시작한다.
	 *                       전환 끝에서는 false — B 에서 차오르던 진행도를 A 가 이어받는다.
	 */
	void CommitFloorBiome(ETRBiomeType InBiome, bool bSettleSurface = false);

	/**
	 * 눈/젖음을 목표치까지 서서히 채운다. 매 틱 불린다.
	 *
	 * **경계 전환과 별개로 돈다.** 땅은 선을 그으며 갈리지만 눈은 쌓이는 데 시간이
	 * 걸린다. 둘을 한 타이머로 묶으면 눈이 쌓이는 것이 아니라 경계선에 칠해진 것으로
	 * 보인다. 그래서 진행도를 따로 들고, 지금 쓰는 슬롯에만 밀어 넣는다.
	 *
	 * @param bSlotB 지금 차오르는 중인 슬롯. 전환 중이면 B, 아니면 A 다.
	 */
	void UpdateFloorSurfaceEffect(float DeltaSeconds, bool bSlotB);

	/**
	 * 새 목표 날씨로 차오르기를 시작한다.
	 *
	 * @param InBiome    목표 풍경. 정의가 없으면 맑음(0,0)으로 본다.
	 * @param Start      출발값. 새 슬롯(B)은 0 에서, 제자리 변화는 지금 값에서 출발한다.
	 * @param bImmediate true 면 시간을 두지 않고 목표치로 끝낸다(레벨 시작 등).
	 * @param bSlotB     첫 값을 밀어 넣을 슬롯.
	 */
	void StartFloorSurfaceEffect(ETRBiomeType InBiome, FVector2D Start, bool bImmediate, bool bSlotB);

	/**
	 * 지금 진행도의 날씨값을 슬롯에 밀어 넣는다.
	 *
	 * **슬롯을 인자로 받는다.** 예전에는 bFloorBlending 을 직접 읽었는데, 그러면 이
	 * 함수의 동작이 호출 순서에 딸려 붙는다 — 전환이 끝나는 프레임에는 플래그를 내리기
	 * 전에 부르느냐 뒤에 부르느냐로 A 와 B 가 갈렸다.
	 *
	 * @param bSlotB true 면 다음 풍경(B), false 면 지금 풍경(A).
	 */
	void PushFloorSurfaceEffect(bool bSlotB) const;

	/** 배경 상태를 사람이 읽을 수 있는 줄로 만든다. 화면 표시와 로그가 이걸 같이 쓴다. */
	void BuildDebugLines(TArray<FString>& OutLines) const;

	/** TR.Scene.Debug 가 켜져 있으면 화면에 찍는다. 매 틱 불린다. */
	void DrawSceneDebug() const;

	/** 지금 이 순간의 날씨값. X=눈, Y=젖음. */
	FVector2D GetCurrentFloorSurfaceEffect() const
	{
		return FMath::Lerp(FloorSurfaceStart, FloorSurfaceTarget, FloorSurfaceProgress);
	}

	/** 지금 경계 위치를 지면과 소품 양쪽에 밀어 넣는다. 축과 폭도 함께 보낸다. */
	void PushFloorBoundary(float TravelBoundary) const;

	/** MPC 스칼라 하나를 밀어 넣는다. EnvironmentCollection 이 비어 있으면 아무것도 안 한다. */
	void PushEnvironmentScalar(FName ParameterName, float Value) const;

	/**
	 * 안개·해·하늘빛·포스트프로세스를 지면 전환에 맞춰 따라가게 한다. 매 틱 불린다.
	 *
	 * **하늘은 발밑 지면과 같은 근거로 움직인다.** 지면 머테리얼은 픽셀의 월드 좌표에서
	 * 경계 마스크를 계산하는데, 그 식을 기차 위치에서 풀면 전역 진행도가 그냥 나온다
	 * (GetEnvironmentBlendAlpha 참고). 전역 값을 따로 타이머로 굴리면 땅은 이미 눈밭인데
	 * 하늘만 아직 사막인 구간이 몇 초 동안 나온다.
	 */
	void UpdateEnvironment(float DeltaSeconds);

	/**
	 * 환경 전환 진행도(0~1).
	 *
	 * 경계 전환 중에는 **지면 마스크를 기차 위치에서 평가한 값**이다. 마스크가
	 * saturate((dot(P,Axis) - Boundary) / Width) 이고 기차는 매니저 원점에 고정이므로,
	 * 투영값이 소거되고 saturate(-FloorBoundaryTravel / FloorBlendWidth) 만 남는다.
	 * 즉 "지금 발밑 땅이 얼마나 새 풍경인가"와 글자 그대로 같은 값이다.
	 *
	 * 경계가 없는 구간에서는 따라갈 선이 없으므로 프리셋의 BlendSeconds 로 간다.
	 */
	float GetEnvironmentBlendAlpha() const;

	/**
	 * 실제로 쓰는 경계 흐림 폭(cm). 0 이 되지 않게 막은 값이다.
	 *
	 * **지면·소품·환경이 모두 같은 폭을 써야 한다.** 하나만 어긋나면 땅과 바위와 안개가
	 * 서로 다른 자리에서 갈린다. 0 으로 나누면 마스크가 NaN 이 되어 화면이 통째로
	 * 사라지므로, 막는 자리도 한 군데여야 한다.
	 */
	float GetSafeFloorBlendWidth() const { return FMath::Max(FloorBlendWidth, 1.f); }

	/** 그 풍경의 환경 프리셋. 정의가 없으면 nullptr — 환경을 안 건드린다는 뜻이다. */
	const UTRBiomeEnvironment* GetBiomeEnvironment(ETRBiomeType InBiome) const;
	void CreateInitialRailSegments();
	void CreateFloor();
	void ValidateGenerationSettings() const;

	/**
	 * 소품 세트 하나를 검사한다. 레거시 세트와 바이옴 세트가 같은 검사를 받는다.
	 *
	 * @param Context 로그에 찍을 이름. 어느 바이옴의 몇 번째 항목인지 짚어 준다.
	 */
	void ValidateMeshDefinitions(const TArray<FTRLevelSceneMeshDefinition>& InMeshDefinitions,
		const FString& Context) const;

	//--- 이동 효과 관련 함수 ---
	//지금 배경을 흘려도 되는가. bAutoMove 와 판 시작 여부를 함께 본다.
	bool IsSceneMovementAllowed() const;

	/**
	 * 이번 프레임에 지형이 실제로 흐르는 속도(cm/s). 멈춰 있으면 0 이다.
	 *
	 * 지면 경계·머테리얼 스크롤·Mass 지면 속도가 **모두 같은 값을 써야 한다.** 하나만
	 * 어긋나면 증상이 제각각으로 나온다 — 경계선이 소품과 따로 놀거나, 지면 텍스처가
	 * 지형과 다른 속도로 흐르거나, 적의 발이 미끄러진다. 그래서 식을 한 군데에만 둔다.
	 */
	float GetEffectiveVisualSpeed() const;
	void MoveChunks(float DeltaSeconds);
	void MoveRailSegments(float DeltaSeconds);
	void RecyclePassedChunks();
	void RecyclePassedRailSegments();
	void RecycleChunk(ATRLevelSceneChunk* Chunk);
	void RecycleRailSegment(ATRLevelSceneRailSegment* RailSegment);
	void UpdateFloorVelocity(bool bForceUpdate = false);

	//지면 스크롤 속도를 UTRGroundMotionSubsystem에 매긴다. 적 애니메이션의 발 미끄러짐 보정에 쓰인다
	void PublishGroundVelocity() const;
	float GetTravelDistance(const ATRLevelSceneChunk* Chunk) const;
	float GetRailTravelDistance(const ATRLevelSceneRailSegment* RailSegment) const;
	float GetFrontmostTravelDistance(const ATRLevelSceneChunk* IgnoredChunk = nullptr) const;
	float GetFrontmostRailTravelDistance(const ATRLevelSceneRailSegment* IgnoredRailSegment = nullptr) const;

	//랜덤 설정/주입 함수
	int32 MakeChunkSeed(int32 SegmentIndex) const;

	//--- 메시 관리용 정보 ---
	UPROPERTY(VisibleAnywhere, Category = "Level Scene")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, Category = "Level Scene|Generation")
	TSubclassOf<ATRLevelSceneChunk> ChunkClass;

	/**
	 * **모든 바이옴에 공통으로 깔리는 소품.**
	 *
	 * 실제로 배치되는 세트는 `공통 + 그 바이옴 전용`이다. 어디에나 있는 바위나 잔해를
	 * 바이옴마다 복사해 넣지 않으려는 것이다 — 복사해 두면 하나 바꿀 때 전부 손봐야 하고,
	 * 한 군데 빠뜨린 것을 알아채기도 어렵다.
	 *
	 * 특정 바이옴만 공통을 빼고 싶으면 그 바이옴의 bIgnoreSharedMeshes 를 켠다.
	 *
	 * (예전에는 "바이옴을 안 쓸 때의 폴백"이었다. Biomes 에 항목이 하나라도 있으면
	 * 통째로 무시돼서, 바이옴 키만 만들어 두면 소품이 조용히 전부 사라졌다.)
	 */
	UPROPERTY(EditAnywhere, Category = "Level Scene|Biome")
	TArray<FTRLevelSceneMeshDefinition> SharedMeshDefinitions;

	/**
	 * 풍경 종류별 정의. 비워 두면 SharedMeshDefinitions 만 깔린다.
	 *
	 * **맵으로 둔 이유**는 같은 종류를 두 번 정의하는 사고를 막기 위해서다. 배열이면
	 * 같은 종류가 두 줄 들어가도 에디터가 막지 않고, 그중 어느 쪽이 쓰이는지는 순서에
	 * 달리게 된다.
	 *
	 * 전환 시점은 **휴식 단계**다. 그때 다음 웨이브의 풍경으로 바꾸면, 새 소품이
	 * 화면 앞쪽에서 생성되어 휴식이 끝나기 전에 기차 주변까지 도착한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Level Scene|Biome")
	TMap<ETRBiomeType, FTRBiomeDefinition> Biomes;

	/**
	 * 판이 시작되기 전에 쓸 풍경.
	 *
	 * 웨이브 테이블 1번 행에 풍경이 적혀 있으면 판이 시작될 때 그 값으로 바뀐다.
	 * 여기 값은 로비에서 보이는 배경을 정한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Level Scene|Biome")
	ETRBiomeType DefaultBiome = ETRBiomeType::None;

	/**
	 * 지면 경계가 흐려지는 폭(cm).
	 *
	 * 이 폭 안에서만 두 지형이 겹친다. 작게 두면 칼로 자른 듯한 경계가 되고,
	 * 크게 두면 넓게 스며든다. 화면 전체를 섞는 것이 아니므로 크게 줘도 탁해지지 않는다.
	 */
	/**
	 * 소품이 환경 값을 읽어 가는 곳(MPC_TREnvironment).
	 *
	 * **지면과 소품이 값을 받는 길이 다르다.** 지면은 액터가 하나라 MID 에 직접 밀어
	 * 넣지만, 소품은 청크마다 수백 개가 깔리고 머테리얼을 공유한다. 인스턴스마다 MID 를
	 * 만들면 배칭이 깨지므로, 소품은 월드에 하나뿐인 이 MPC 에서 읽는다.
	 *
	 * 비워 두면 소품 쪽 갱신만 조용히 건너뛴다. 지면은 그대로 동작한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Level Scene|Biome")
	TObjectPtr<UMaterialParameterCollection> EnvironmentCollection;

	UPROPERTY(EditAnywhere, Category = "Level Scene|Biome", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float FloorBlendWidth = 2500.f;

	/**
	 * 경계가 기차보다 얼마나 더 뒤까지 간 뒤에 전환을 마칠지(cm).
	 *
	 * 경계가 기차를 지나쳐도 카메라는 뒤쪽 지면을 조금 본다. 0 으로 두면 아직 보이는
	 * 자리에서 A 슬롯이 새 텍스처로 바뀌어 그 부분만 툭 변한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Level Scene|Biome", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FloorBoundaryOvershoot = 20000.f;

	//--- 메시 생성/업데이트용 수치 ---
	//배치에 사용될 난수열 생성용 시드값
	UPROPERTY(EditAnywhere, Category = "Level Scene|Generation")
	int32 RouteSeed = 1337;

	//반복에 사용할 청크 액터 갯수
	UPROPERTY(EditAnywhere, Category = "Level Scene|Chunk", meta = (ClampMin = "1"))
	int32 ChunkCount = 10;

	//청크 액터의 가로새로 평면 사이즈
	UPROPERTY(EditAnywhere, Category = "Level Scene|Chunk", meta = (ClampMin = "1.0"))
	FVector2D ChunkSize = FVector2D(3000.f, 30000.f);

	//시작시 뒤쪽으로 미리 생성할 청크 갯수
	UPROPERTY(EditAnywhere, Category = "Level Scene|Chunk", meta = (ClampMin = "0"))
	int32 InitialBehindChunkCount = 5;

	//청크가 뒤로 멀어졌을 때 회수할 기준값, 더 멀어지면 앞쪽으로 초기화하여 재배치
	UPROPERTY(EditAnywhere, Category = "Level Scene|Chunk", meta = (ClampMin = "0.0"))
	float RecycleBehindDistance = 15000.f;

	//이동속도, 기차의 이동속도와 연동하여 속도감 동기화
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Scene|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float VisualSpeed = 1000.f;

	/**
	 * 바이옴별로 합쳐 둔 최종 소품 세트. RebuildResolvedMeshDefinitions 가 채운다.
	 *
	 * **BeginPlay 이후로는 항목을 추가하지 않는다.** 여기 원소의 참조를 밖으로
	 * 넘기는데, 나중에 다른 키를 넣으면 그 참조가 끊어질 수 있다.
	 */
	UPROPERTY(Transient)
	TMap<ETRBiomeType, FTRLevelSceneMeshDefinitionSet> ResolvedMeshDefinitions;

	/** 새로 만들어지는 청크가 쓸 풍경. */
	ETRBiomeType CurrentBiome = ETRBiomeType::None;

	/** 마지막으로 본 값. 그대로면 다시 판단하지 않는다(없는 종류 경고가 매 틱 나가는 것도 막는다). */
	ETRBiomeType LastEvaluatedBiome = ETRBiomeType::MAX;

	/**
	 * 지면이 **지금 보여주고 있는** 바이옴(A 슬롯).
	 *
	 * CurrentBiomeIndex 와 따로 두는 이유는 둘의 시점이 다르기 때문이다. 소품은 바뀌자마자
	 * 앞에서 생성되기 시작하지만, 지면은 그 소품이 도착한 뒤에야 섞이기 시작한다.
	 * 이 값이 CurrentBiomeIndex 를 따라잡으면 전환이 끝난 것이다.
	 */
	ETRBiomeType FloorBiome = ETRBiomeType::MAX;

	/** 지금 섞고 있는 목표 풍경. bFloorBlending 이 꺼져 있으면 의미 없다. */
	ETRBiomeType FloorBlendTarget = ETRBiomeType::None;

	/**
	 * 지면을 섞는 중인가.
	 *
	 * 목표를 None 과 비교해 판단하지 않는 이유는 None 도 유효한 풍경이기 때문이다.
	 * 상태를 값에 겹쳐 담으면 "지정 안 됨"과 "None 으로 전환 중"을 구별할 수 없다.
	 */
	bool bFloorBlending = false;

	/**
	 * 경계의 현재 위치. 매니저 기준 진행 거리(cm)다.
	 *
	 * 앞쪽(양수)에서 시작해 지형 스크롤과 **같은 속도로** 줄어든다. 그래서 경계선이
	 * 소품과 정확히 같은 속도로 다가오고, 둘이 어긋나 보이지 않는다.
	 */
	float FloorBoundaryTravel = 0.f;

	/**
	 * 전환 중이 아닐 때 경계를 치워 두는 거리(cm).
	 *
	 * 화면 밖으로 충분히 밀어내면 마스크가 어디서나 0 이 되어 A 슬롯만 보인다.
	 * 0 으로 두면 매니저 앞쪽 절반이 B 슬롯으로 보이는 사고가 난다.
	 */
	static constexpr float NoTransitionBoundaryTravel = 1.0e8f;

	/**
	 * 안개·해·하늘빛을 섞어 레벨에 적용하는 컴포넌트.
	 *
	 * 매니저가 직접 라이트를 만지지 않고 한 겹 두는 이유는, 이 클래스가 이미 청크 풀링과
	 * 레일과 지면 전환을 들고 있어서다. 라이팅까지 여기 들어오면 아무도 못 읽는다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Level Scene|Environment")
	TObjectPtr<UTREnvironmentBlenderComponent> EnvironmentBlender;

	/**
	 * 환경이 출발한 풍경과 목표 풍경.
	 *
	 * 지면 슬롯(FloorBiome / FloorBlendTarget)을 그대로 따라가지만, **따로 들고 있어야
	 * 한다.** 전환이 끝나면 지면은 A 슬롯 하나만 남기고 목표를 잊는데, 환경은 그 뒤로도
	 * "어디서 출발했는지"를 알아야 경계 없는 전환을 이어서 섞을 수 있다.
	 */
	ETRBiomeType EnvironmentFromBiome = ETRBiomeType::MAX;
	ETRBiomeType EnvironmentToBiome = ETRBiomeType::MAX;

	/** 지금 진행도(0~1). 경계 전환 중에는 경계 위치가, 아닐 때는 시간이 정한다. */
	float EnvironmentAlpha = 1.f;

	/**
	 * 날씨 효과의 출발값과 목표값. X=눈, Y=젖음.
	 *
	 * 진행도 하나로 둘 사이를 오가게 해서, 눈과 물이 따로 놀지 않고 같은 리듬으로
	 * 차오른다. 값 두 벌을 각자 보간하면 이 동기화를 매번 손으로 맞춰야 한다.
	 */
	FVector2D FloorSurfaceStart = FVector2D::ZeroVector;
	FVector2D FloorSurfaceTarget = FVector2D::ZeroVector;

	/** 0 이면 출발값, 1 이면 목표값. 다 차면 더 밀어 넣지 않는다. */
	float FloorSurfaceProgress = 1.f;

	/** 출발값에서 목표값까지 걸리는 시간(초). 0 이면 즉시. */
	float FloorSurfaceDuration = 0.f;

	//자동 이동 여부 설정
	UPROPERTY(EditAnywhere, Category = "Level Scene|Movement")
	bool bAutoMove = true;

	//판이 시작된 뒤에만 움직일지 여부. 끄면 로비에서도 배경이 흐른다.
	UPROPERTY(EditAnywhere, Category = "Level Scene|Movement")
	bool bMoveOnlyAfterMatchStart = true;

	//레일 생성 여부 결정
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail")
	bool bGenerateRail = true;

	//레일 메시 생성용 액터 클래스
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail")
	TSubclassOf<ATRLevelSceneRailSegment> RailSegmentClass;
	
	//레일 메시
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail")
	TObjectPtr<UStaticMesh> RailMesh;

	//생성할 레일 갯수
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail", meta = (ClampMin = "1"))
	int32 RailSegmentCount = 24;

	//시작시 뒤쪽으로 미리 생성할 레일 조각 갯수
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail", meta = (ClampMin = "0"))
	int32 InitialBehindRailSegmentCount = 15;

	//레일 배치 간격
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail", meta = (ClampMin = "1.0"))
	float RailSegmentLength = 1000.f;

	//레일이 뒤로 멀어졌을 때 회수할 기준값, 더 멀어지면 앞쪽으로 재배치
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail", meta = (ClampMin = "0.0"))
	float RailRecycleBehindDistance = 15000.f;

	//레일 배치 오프셋
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail")
	FVector RailOffset = FVector::ZeroVector;

	//레일 주변 위치로 인식할 범위. 레일이 생성될 때 이 폭 안에는 Random 배치하지 않음.
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail|Placement", meta = (ClampMin = "0.0", ToolTip = "Half-width of the no-placement strip centered on RailOffset.Y."))
	float RailPlacementClearanceHalfWidth = 750.f;

	//레일 액터 컬링 거리
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail", meta = (ClampMin = "0"))
	int32 RailEndCullDistance = 30000;

	//레일 그림자 생성 여부
	UPROPERTY(EditAnywhere, Category = "Level Scene|Rail")
	bool bRailCastShadow = true;

	//지면 사용할지 여부
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	bool bGenerateFloor = true;

	//기존 레벨에 있는 지면 사용할지, 혹은 새로 생성할지 여부
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	bool bUseExistingLevelFloor = true;

	//이미 배치되어있는 사용할 지면 액터
	UPROPERTY(EditInstanceOnly, Category = "Level Scene|Floor", meta = (EditCondition = "bUseExistingLevelFloor"))
	TObjectPtr<AActor> ExistingFloorActor;

	//지면 액터 클래스
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	TSubclassOf<ATRLevelSceneFloor> FloorClass;

	//지면 액터에 사용할 메시
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	TObjectPtr<UStaticMesh> FloorMesh;

	//지면 메시에 사용할 머티리얼
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	TObjectPtr<UMaterialInterface> FloorMaterial;

	//지면 액터 생성시 위치
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	FTransform FloorRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -20.f), FVector(200.f, 200.f, 1.f));

	//지면 머티리얼에 적용할 Panner이동효과 방향
	UPROPERTY(EditAnywhere, Category = "Level Scene|Floor")
	FVector2D FloorMovementDirection = FVector2D(1.f, 0.f);

	//---관리 대상 액터---
	//레벨에 배치된 청크 액터들
	UPROPERTY(Transient)
	TArray<TObjectPtr<ATRLevelSceneChunk>> ActiveChunks;

	//레벨에 배치된 레일 액터들
	UPROPERTY(Transient)
	TArray<TObjectPtr<ATRLevelSceneRailSegment>> ActiveRailSegments;

	//레벨에 배치된 지면 액터
	UPROPERTY(Transient)
	TObjectPtr<ATRLevelSceneFloor> ActiveFloor;

	//속도 변경시 이전값 저장용도
	float LastAppliedFloorVisualSpeed = -1.f;
	FVector2D LastAppliedFloorMovementDirection = FVector2D::ZeroVector;

	//청크 재활용시 재생성용 인덱스
	int32 NextSegmentIndex = 0;
};
