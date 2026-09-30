// 연출 큐가 주고받는 타입.
//
// ⚠️ Kind 는 닫힌 열거형으로 둔다. 무타입 이벤트 버스(FName 키)로 열어 두면
//    빠진 처리를 컴파일러가 못 잡고, 몇 달 뒤 누가 무엇을 듣는지 추적이 안 된다.

#pragma once

#include "CoreMinimal.h"
#include "UI/View/BGCardFXTypes.h"
#include "BGPresentationTypes.generated.h"

/** 연출의 종류. 1단계는 카드 사용만 — 드로우·사망은 다음 단계에서 붙는다. */
UENUM(BlueprintType)
enum class EBGPresentationKind : uint8
{
	None     UMETA(DisplayName = "없음"),
	CardPlay UMETA(DisplayName = "카드 사용"),
};

/**
 * 연출을 이루는 갈래. 렌더 틱 / 물리 틱처럼 **책임이 다른 축**이라 따로 센다.
 *
 * 디렉터는 어느 트랙이 몇 개 도는지만 알고 내용은 모른다. 트랙을 나눈 이득은 두 가지다:
 *   ① 끝내는 주체가 트랙마다 하나로 정해진다 — "누가 끝내는가" 를 찾을 곳이 한 곳이다.
 *   ② 멈췄을 때 어느 축이 안 끝났는지 로그에 이름으로 찍힌다.
 */
UENUM(BlueprintType)
enum class EBGPresentationTrack : uint8
{
	/**
	 * 판정 UI 대기(주사위·결과 표시). 끝내는 주체: ABGPlayerController(복제된 대기 플래그).
	 * ⚠️ 월드 연출은 판정 UI 종료 통지 뒤에 시작한다. ResolutionPresentationSeconds는 안전망이다.
	 *    이 트랙이 없으면 그 사이 잡힌 트랙이 하나도 없어 연출이 곧바로 끝난다.
	 */
	Resolution UMETA(DisplayName = "판정 대기"),

	/** 시네마틱 단계. 끝내는 주체: ABGPlayerController(단계 None). */
	Cinematic UMETA(DisplayName = "시네마틱"),

	/** GameplayCue 의 파티클·사운드 수명. 끝내는 주체: ABGCardGameplayCueNotify(Cue 제거). */
	WorldFX   UMETA(DisplayName = "월드 이펙트"),

	/** 캐릭터 애님 몽타주. 끝내는 주체: ABGCardGameplayCueNotify(몽타주 종료). */
	Montage   UMETA(DisplayName = "몽타주"),

	/** 위젯 연출. 끝내는 주체: BP(UMG 애니메이션 Finished). */
	UI        UMETA(DisplayName = "UI"),
};

/**
 * 연출이 끝나기를 기다리는 표. 참여자가 Hold 로 받아 두었다가 Release 로 놓는다.
 * 전부 놓여야 디렉터가 다음 연출로 넘어간다.
 */
USTRUCT(BlueprintType)
struct FBGPresentationHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BG|Presentation")
	int32 Id = INDEX_NONE;

	/** 어느 갈래의 표인가. 진단용이며 동작에는 쓰이지 않는다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Presentation")
	EBGPresentationTrack Track = EBGPresentationTrack::UI;

	bool IsValid() const { return Id != INDEX_NONE; }
};

/**
 * 트랙 표 한 장. 쥐기와 놓기를 짝지어 둔다 — 참여자마다 같은 세 줄을 되풀이하지 않게.
 *
 * ⚠️ 디렉터를 들고 있지 않는다. 쓸 때 넘겨받는다 — 표를 쥐는 쪽(Cue 액터)이
 *    디렉터보다 짧게 살기 때문에, 참조를 들면 죽은 포인터를 보게 된다.
 */
struct FBGPresentationTrackHold
{
	/** 이미 쥐고 있으면 아무 일도 하지 않는다. */
	void Begin(class UBGPresentationDirector* Director, EBGPresentationTrack Track);

	/** 쥔 적이 없으면 아무 일도 하지 않는다. 여러 번 불러도 안전하다. */
	void End(class UBGPresentationDirector* Director);

	bool IsHeld() const { return Handle.IsValid(); }

private:
	FBGPresentationHandle Handle;
};

/** 연출 하나의 요청. */
USTRUCT(BlueprintType)
struct FBGPresentationRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BG|Presentation")
	EBGPresentationKind Kind = EBGPresentationKind::None;

	/** 카드 사용 연출의 맥락. 등급·카테고리·효과 목록이 여기 실린다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Presentation")
	FBGCardPlayContext Context;

	/**
	 * 이 연출의 상한(초).
	 * ⚠️ 구독자가 Release 를 빠뜨리면 큐가 영영 안 흐르는데 **에러가 나지 않는다.**
	 *    상한이 그것을 드러내고 판을 계속 굴린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Presentation", meta = (ClampMin = "0.1"))
	float TimeoutSeconds = 8.f;

	/**
	 * 이 연출의 **바닥**(초). 트랙이 다 끝나도 이 시간까지는 붙잡는다.
	 *
	 * ⚠️ 없으면 짧은 몽타주가 카드보다 먼저 끝났을 때 잡힌 트랙이 하나도 없는 채로
	 *    연출이 시작돼 그 프레임에 끝난다 — HUD 가 켜지자마자 꺼져 깜빡인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Presentation", meta = (ClampMin = "0.0"))
	float MinDurationSeconds = 0.f;
};
