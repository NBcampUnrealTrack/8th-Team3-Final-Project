// UBGCardWidget — 카드 데이터를 화면에 올리는 부분 (뷰 갱신 · 드래그/잠금 상태 표시).
//
// ── 이 클래스는 네 개의 .cpp 로 나뉘어 있다 ──────────────────────────────────
//   BGCardWidget.cpp         (여기) 생성 · 뷰 갱신 · 드래그/잠금 표시
//   BGCardWidget_Motion.cpp        모션 상태 기계 (등장 · 이동 · 중앙 · 퇴장)
//   BGCardWidget_FX.cpp            머테리얼 연출 (등급 글로우 · 퇴장 디졸브)
//   BGCardWidget_Input.cpp         마우스 입력 (호버 판정 · 드래그 시작)
//
// 한 클래스를 나눈 것이지 네 클래스가 아니다. 헤더는 하나이고 멤버도 공유한다 —
// 1300 줄짜리 한 파일에서 "지금 고치는 것이 어느 책임인지" 가 안 보였기 때문에 가른 것뿐이다.
//
// ⚠️ 새 함수를 넣을 때 파일을 고르는 기준: **그 함수가 무엇을 만지는가**.
//    위젯 슬레이트 속성이면 여기, MotionState/좌표면 Motion, DMI 면 FX, 커서면 Input.
//    애매하면 Motion 이다 — 대부분의 연출은 시간축을 타기 때문이다.
// ──────────────────────────────────────────────────────────────────────────

#include "UI/Widget/BGCardWidget.h"
#include "UI/Widget/BGHandWidget.h"   // SetOwningHand 의 TWeakObjectPtr 대입에 완전한 타입이 필요하다

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Debug/BGCardLog.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

void UBGCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshVisual();
}

void UBGCardWidget::NativeDestruct()
{
	//	⚠️ 안전망이다. 퇴장 없이 걷어내진 카드(판 종료·손패 초기화)를 여기서 잡지 않으면
	//	   HUD 가 켜진 채로 남고 아무 에러도 나지 않는다.
	NotifyPlayFXFinished();

	Super::NativeDestruct();
}

void UBGCardWidget::SetOwningHand(UBGHandWidget* InHand)
{
	OwningHand = InHand;
}

void UBGCardWidget::SetCardView(const FBGCardView& InCardView)
{
	CardView = InCardView;
	RefreshVisual();
}

void UBGCardWidget::SetBeingDragged(bool bInBeingDragged)
{
	if (bBeingDragged == bInBeingDragged)
	{
		return;
	}
	bBeingDragged = bInBeingDragged;

	if (bBeingDragged)
	{
		//	커서를 따라다니는 복제본이 진짜 카드다. 원본은 실루엣으로 남기되 입력은 끊는다 —
		//	실루엣이 입력을 받으면 드래그 중 자기 자신이 드롭 대상이 된다.
		PreDragVisibility = GetVisibility();
		PreDragOpacity    = GetRenderOpacity();

		SetVisibility(ESlateVisibility::HitTestInvisible);
		SetRenderOpacity(DragGhostOpacity);
	}
	else
	{
		//	퇴장 중이어도 불투명도는 되돌린다. 퇴장 페이드는 TickExiting 이 다시 정한다.
		SetRenderOpacity(PreDragOpacity);

		if (IsLeavingHand())
		{
			//	⚠️ 가시성은 퇴장 로직의 것이라 건드리지 않는다. 다만 BeginLeaveHand 가 갈무리한 값은
			//	   '드래그 중의 HitTestInvisible' 이라, 그대로 두면 손패 복귀 시 입력을 못 받는다.
			PreLeaveVisibility = PreDragVisibility;
		}
		else
		{
			SetVisibility(PreDragVisibility);
		}
	}

	RefreshVisual();
}

void UBGCardWidget::SetDragLocked(bool bInLocked)
{
	if (bDragLocked == bInLocked)
	{
		return;
	}
	bDragLocked = bInLocked;
	RefreshVisual();
}

void UBGCardWidget::RefreshChanceText()
{
	if (!Text_ChanceCriticalFailure && !Text_ChanceFailure && !Text_ChanceSuccess && !Text_ChanceCriticalSuccess)
	{
		return;   // WBP 가 확률 칸을 두지 않았다. 전부 선택 사항이다.
	}

	//	판정하지 않는 카드(장비)에 확률을 띄우면 "장비도 실패할 수 있다" 는 오해를 준다.
	//	Collapsed 로 자리까지 회수해 빈 줄을 남기지 않는다.
	const ESlateVisibility ChanceVisibility =
		CardView.bHasChances ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	//	보정 후 값을 보여준다 — GDD §8 이 요구하는 것은 "최종 확률" 이다.
	const FBGCardChanceView& Shown = CardView.FinalChance;

	//	반올림 때문에 합이 100 이 안 될 수 있다(등급별 값이 정확한 쪽을 택했다).
	//	태그 이름은 DT_BGChanceTextStyle 의 행 이름과 정확히 같아야 한다.
	const auto ApplyTierChance = [bHasChances = CardView.bHasChances, ChanceVisibility](URichTextBlock* Block, const FText& Format, float Value)
	{
		if (!Block)
		{
			return;   // 이 등급 칸을 두지 않은 WBP.
		}

		Block->SetVisibility(ChanceVisibility);
		if (bHasChances)
		{
			Block->SetText(FText::Format(Format, FText::AsNumber(FMath::RoundToInt(Value))));
		}
	};

	ApplyTierChance(Text_ChanceCriticalFailure, NSLOCTEXT("BG", "CardChanceCriticalFailure", "<CriticalFailure>{0}</>"), Shown.CriticalFailure);
	ApplyTierChance(Text_ChanceFailure, NSLOCTEXT("BG", "CardChanceFailure", "<Failure>{0}</>"), Shown.Failure);
	ApplyTierChance(Text_ChanceSuccess, NSLOCTEXT("BG", "CardChanceSuccess", "<Success>{0}</>"), Shown.Success);
	ApplyTierChance(Text_ChanceCriticalSuccess, NSLOCTEXT("BG", "CardChanceCriticalSuccess", "<CriticalSuccess>{0}</>"), Shown.CriticalSuccess);
}

void UBGCardWidget::RefreshChanceModifiedBanner()
{
	if (!Text_ChanceModified)
	{
		return;
	}

	//	호버 중일 때만 띄운다.
	const bool bShow = CardView.bHasChances && CardView.bChanceModified && IsHoverActive();

	Text_ChanceModified->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShow)
	{
		Text_ChanceModified->SetColorAndOpacity(FSlateColor(ModifiedChanceColor));
	}
}

void UBGCardWidget::RefreshVisual()
{
	if (Text_CardName)
	{
		Text_CardName->SetText(CardView.DisplayName);
	}
	if (Text_CardDescription)
	{
		Text_CardDescription->SetText(CardView.Description);
	}

	RefreshChanceText();
	RefreshChanceModifiedBanner();
	RefreshArt();
	RefreshFrame();

	if (Border_Root)
	{
		//	잠금이 가장 우선 — 잠긴 동안에는 사용 가능 여부와 무관하게 손댈 수 없다.
		const FLinearColor Tint =
			bDragLocked          ? LockedTint :
			bBeingDragged        ? DraggedTint :
			!CardView.bPlayable  ? UnplayableTint :
			                       NormalTint;
		Border_Root->SetBrushColor(Tint);
	}
}

void UBGCardWidget::RefreshArt()
{
	if (!Img_Art)
	{
		return;   // WBP 가 그림 칸을 두지 않았다. 선택 사항이므로 조용히 넘어간다.
	}

	//	뷰의 그림 → 폴백 → 칸 접기 순. null 브러시를 넣으면 카드 한복판이 투명해져
	//	"카드가 사라졌다" 로 보이므로 브러시는 건드리지 않는다.
	UTexture2D* Art = CardView.ArtTexture ? CardView.ArtTexture.Get() : FallbackArtTexture.Get();

	if (!Art)
	{
		Img_Art->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Img_Art->SetVisibility(ESlateVisibility::HitTestInvisible);
	Img_Art->SetBrushFromTexture(Art, /*bMatchSize=*/false);
}

void UBGCardWidget::RefreshFrame()
{
	if (!Border_Root)
	{
		return;
	}

	UTexture2D* Frame = nullptr;
	switch (CardView.Category)
	{
		case EBGCardCategory::Equipment:
			Frame = EquipmentFrameTexture;
			break;
		case EBGCardCategory::Attack:
			Frame = AttackFrameTexture;
			break;
		case EBGCardCategory::Magic:
			Frame = MagicFrameTexture;
			break;
	}

	if (!Frame)
	{
		return;   // null 로 덮으면 카드가 통째로 투명해진다(RefreshArt 와 같은 이유).
	}

	//	⚠️ **브러시를 갈아 끼우지 않는다.** SetBrushFromTexture 를 부르면 물려 둔 MI 가 날아가고,
	//	   이 함수는 NativePreConstruct 에서도 불리므로 디자인 타임에 그 일이 벌어진다 —
	//	   그 상태로 WBP 를 저장하면 애셋에 텍스처가 박힌다. 실제로 한 번 날려 먹었다.
	if (IsDesignTime())
	{
		//	디자인 타임에는 GetDynamicMaterial 조차 부르지 않는다 — 브러시의 ResourceObject 를
		//	DMI 로 바꿔 놓아 미리보기를 위해 애셋을 더럽히는 꼴이 된다.
		return;
	}

	//	⚠️ 자기 DMI 를 따로 만들어 캐시하지 않는다. UMG 애니메이션도 이 브러시의 DMI 를 잡아
	//	   HoverAmount 를 모는데, 코드가 다른 DMI 를 들면 두 쪽이 갈라져 한쪽 값이 안 먹는다.
	//	   GetDynamicMaterial 은 이미 DMI 면 그대로 돌려주므로 매번 불러도 새로 만들지 않는다.
	UMaterialInstanceDynamic* FrameMID = Border_Root->GetDynamicMaterial();
	if (!FrameMID)
	{
		//	머테리얼이 없다. 텍스처로 덮으면 나중에 MI 를 물려도 첫 갱신에 또 날아가므로
		//	고칠 곳만 알려 주고 그냥 둔다. RefreshVisual 은 자주 불리므로 한 번만 남긴다.
		if (!bWarnedFrameBrushNotMaterial)
		{
			bWarnedFrameBrushNotMaterial = true;
			UE_LOG(LogBGCard, Warning,
				TEXT("[Card] Border_Root 브러시에 머테리얼이 없어 대분류별 테두리를 못 바꾼다. "
				     "WBP 의 Border_Root ▸ Appearance ▸ Brush ▸ Image 에 "
				     "MI_BGCardFrame_Default 를 물릴 것."));
		}
		return;
	}

	if (!FrameTexParamName.IsNone())
	{
		FrameMID->SetTextureParameterValue(FrameTexParamName, Frame);
	}
}
