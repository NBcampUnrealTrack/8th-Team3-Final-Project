#include "UI/Widget/BGHeartSlotWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"

void UBGHeartSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyTint();
}

void UBGHeartSlotWidget::SetFilled(bool bInFilled, bool bAnimate)
{
	if (bFilled == bInFilled)
	{
		//	같은 상태여도 다시 칠한다 — 애니메이션이 끝나며 시작 전 색으로 되돌려 놓으면(When Finished:
		//	Restore State) 코드가 기억하는 상태와 화면이 어긋난다. 갱신이 올 때마다 화면을 상태에 맞춘다.
		if (!(Anim_Lose && IsAnimationPlaying(Anim_Lose)))
		{
			ApplyTint();
		}
		return;
	}
	bFilled = bInFilled;

	//	회복으로 다시 찼는데 꺼지는 연출이 돌고 있으면 끊는다. 안 그러면 찬 칸이 어두워진 채 끝난다.
	if (Anim_Lose && IsAnimationPlaying(Anim_Lose))
	{
		StopAnimation(Anim_Lose);
	}

	if (!bFilled && bAnimate && Anim_Lose)
	{
		//	색은 애니메이션이 끝난 뒤에 맞춘다 — 먼저 어둡게 칠하면 번쩍임 트랙이 없는 애니메이션에서
		//	칸이 이미 꺼진 채로 튄다.
		PlayAnimation(Anim_Lose);
	}
	else
	{
		ApplyTint();

		if (bFilled && bAnimate && Anim_Gain)
		{
			PlayAnimation(Anim_Gain);
		}
	}

	if (bAnimate)
	{
		if (bFilled) { OnHeartGained(); }
		else         { OnHeartLost(); }
	}
}

void UBGHeartSlotWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	//	연출의 마지막 키가 어떤 색이든 끝 상태는 코드가 정한다 — 틴트 값 하나로 두 곳이 맞춰진다.
	//	⚠️ 지금 칠하는 것만으로는 부족하다. 이 통지 뒤에 애니메이션이 마지막 프레임을 적용하거나
	//	   시작 전 색으로 되돌리면 방금 칠한 색이 덮인다. 다음 틱에 한 번 더 칠한다.
	if (Animation == Anim_Lose)
	{
		ApplyTint();
		bReapplyTintNextTick = true;
	}
}

void UBGHeartSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bReapplyTintNextTick)
	{
		bReapplyTintNextTick = false;
		ApplyTint();
	}
}

void UBGHeartSlotWidget::ApplyTint()
{
	if (Img_Heart)
	{
		Img_Heart->SetColorAndOpacity(bFilled ? FullTint : EmptyTint);
	}
}

void UBGHeartSlotWidget::RefreshRow(TConstArrayView<UBGHeartSlotWidget*> Slots, int32 Health, int32 MaxHealth,
	FBGHeartRowState& State)
{
	//	판 전에는 최대 체력이 0 이다 — 줄째로 접힌다.
	const int32 ShownMax    = FMath::Clamp(MaxHealth, 0, Slots.Num());
	const int32 ShownHealth = FMath::Clamp(Health, 0, ShownMax);

	//	⚠️ 첫 값(위젯 생성)이나 최대 체력이 바뀐 갱신(직업 배정·장비)은 잃은 것이 아니다 — 연출하지 않는다.
	//	   위젯 갱신은 체력과 무관한 이유로도 자주 오지만, 값이 같으면 SetFilled 가 아무것도 하지 않는다.
	const bool bAnimate = State.LastHealth != INDEX_NONE && ShownMax > 0 && ShownMax == State.LastMaxHealth;

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		UBGHeartSlotWidget* HeartSlot = Slots[Index];
		if (!HeartSlot)
		{
			continue;
		}

		//	최대 체력을 넘는 칸은 자리까지 접는다. 숨기기만 하면 줄이 가운데로 모이지 않는다.
		if (Index >= ShownMax)
		{
			HeartSlot->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		HeartSlot->SetVisibility(ESlateVisibility::HitTestInvisible);
		HeartSlot->SetFilled(Index < ShownHealth, bAnimate);
	}

	State.LastHealth    = ShownHealth;
	State.LastMaxHealth = ShownMax;
}
