// BANG! 좌석 정보 → UI 뷰모델 변환. UBGCardViewLibrary 와 같은 역할을 좌석에 대해 한다.
//
// ⚠️ 여기서 만드는 FBGSeatView 에는 "전원에게 공개되어도 되는 것"만 담아야 한다.
//    손패 내용은 절대 넣지 말 것 — HandCount 만 넣는다.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/View/BGSeatView.h"
#include "BGSeatViewLibrary.generated.h"

class ABGPlayerState;
class ABGCharacter;
class APlayerController;

UCLASS()
class BG_API UBGSeatViewLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 모든 좌석의 공개 정보를 좌석 번호 순으로 만든다.
	 * Viewer 기준으로 bIsLocalPlayer / Distance 가 채워진다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	static TArray<FBGSeatView> MakeSeatViews(const APlayerController* Viewer);

	/**
	 * 대상으로 고를 수 있는 좌석 목록(살아 있고 자기 자신이 아닌 좌석).
	 *
	 * ⚠️ 사거리(WeaponRange vs Distance) 판정은 아직 들어 있지 않다 — STEP 7/8 에서 추가한다.
	 *    지금은 "너무 넓게" 고를 수 있으므로, 최종 판정은 서버가 다시 해야 한다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	static TArray<int32> GetSelectableSeats(const APlayerController* Viewer, bool bExcludeSelf = true);

	/**
	 * 살아 있는 좌석들로 이루어진 원에서의 최단 걸음 수. 자기 자신이거나 계산 불가면 INDEX_NONE.
	 *
	 * ⚠️ Scope / Mustang 등 어트리뷰트 보정은 아직 반영하지 않는다(STEP 7).
	 *    해당 카드가 구현되기 전까지는 이 값이 곧 최종 거리와 같다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	static int32 GetRingDistance(const APlayerController* Viewer, int32 FromSeat, int32 ToSeat);

	/** 좌석 번호로 PlayerState 를 찾는다. 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	static ABGPlayerState* FindPlayerStateBySeat(const APlayerController* Viewer, int32 SeatIndex);

	/**
	 * 좌석 번호로 그 좌석의 캐릭터(폰)를 찾는다. 없거나 폰이 ABGCharacter 가 아니면 nullptr.
	 * 조준(UBGWorldTargetingComponent)과 턴 강조(ABGPlayerController) 양쪽이 같은 로직을
	 * 각자 들고 있던 것을 여기 하나로 모았다 — 나중에 대상 해석 방식이 바뀌면 여기만 고치면 된다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	static ABGCharacter* FindCharacterBySeat(const APlayerController* Viewer, int32 SeatIndex);
};
