#include "UI/Widget/BGTurnPanelWidget.h"

#include "Audio/BGAudioSubsystem.h"
#include "UI/View/BGSeatViewLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Debug/BGCardLog.h"
#include "Lobby/BGRopeComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/View/BGInfoTooltipRow.h"
#include "UI/Widget/BGInfoTooltipWidget.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

void UBGTurnPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	//	툴팁 델리게이트는 슬레이트 위젯을 만들 때 한 번만 읽힌다.
	//	NativeConstruct 는 그 뒤라 거기서 붙이면 늦는다.
	struct FInfoRow { UBorder* Border; const TCHAR* FuncName; };
	const FInfoRow InfoRows[] = {
		{ Border_Job,       TEXT("MakeJobTooltip")       },
		{ Border_Equipment, TEXT("MakeEquipmentTooltip") },
		{ Border_HandLimit, TEXT("MakeHandLimitTooltip") },
		{ Border_Role,      TEXT("MakeRoleTooltip")      },
	};

	for (const FInfoRow& Row : InfoRows)
	{
		if (Row.Border)
		{
			Row.Border->ToolTipWidgetDelegate.BindUFunction(this, Row.FuncName);
		}
	}
}

void UBGTurnPanelWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshVisual();
}

void UBGTurnPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->BindDefaultButtonsInWidget(this);
		Audio->BindButtonSoundGroup(Button_EndTurn, EBGUIButtonSoundGroup::Confirm);
	}

	if (Button_EndTurn)
	{
		//	중복 구독 방지 — NativeConstruct 는 위젯이 다시 뷰포트에 붙을 때마다 불릴 수 있다.
		Button_EndTurn->OnClicked.RemoveDynamic(this, &UBGTurnPanelWidget::HandleEndTurnClicked);
		Button_EndTurn->OnClicked.AddDynamic(this, &UBGTurnPanelWidget::HandleEndTurnClicked);
	}

	RefreshVisual();

	RefreshHealthHearts();
	RefreshIcons();
}

namespace
{
	//	표 Row 가 없을 수 있다 — 그때는 빈 텍스트로 접힌다.
	FText InfoTitle(const FBGInfoTooltipRow* Row) { return Row ? Row->Title : FText::GetEmpty(); }
	FText InfoBody (const FBGInfoTooltipRow* Row) { return Row ? Row->Body  : FText::GetEmpty(); }

	//	살아 있는 값이 있으면 그쪽을, 없으면 표의 문구를 쓴다.
	FText PreferLive(const FText& Live, const FText& Fallback) { return Live.IsEmpty() ? Fallback : Live; }
}

UWidget* UBGTurnPanelWidget::MakeInfoTooltip(const FText& InTitle, const FText& InBody)
{
	if (!InfoTooltipClass)
	{
		return nullptr;   //	그릴 위젯이 없다.
	}

	//	줄마다 새로 만든다. 슬레이트 위젯은 부모가 하나뿐이라, 하나를 네 Border 가
	//	돌려 쓰면 툴팁이 겹치는 순간 먼저 뜬 쪽이 빈 채로 남는다.
	UBGInfoTooltipWidget* Tooltip = CreateWidget<UBGInfoTooltipWidget>(this, InfoTooltipClass);
	if (Tooltip)
	{
		Tooltip->SetInfo(InTitle, InBody);
	}

	return Tooltip;
}

const FBGInfoTooltipRow* UBGTurnPanelWidget::FindInfoRow(FName RowName) const
{
	if (!InfoTable)
	{
		return nullptr;
	}

	//	없는 Row 는 정상이다(역할 미배정 등) — 경고를 끄고 호출부가 폴백을 고른다.
	return InfoTable->FindRow<FBGInfoTooltipRow>(RowName, TEXT("BGTurnPanel Info"), /*bWarnIfMissing=*/ false);
}

FText UBGTurnPanelWidget::MakeEquipmentLines() const
{
	//	장비 한 장이 "이름 / 설명" 한 덩이가 되고, 덩이 사이만 빈 줄로 띄운다.
	//	이름과 설명 사이까지 빈 줄을 두면 한 장비가 둘로 갈라져 보인다.
	//	장비가 없으면 빈 텍스트다 — 호출부가 표의 문구로 넘어간다.
	TArray<FText> Blocks;
	Blocks.Reserve(LocalSeatView.Equipment.Num());
	for (const FBGEquipmentView& Item : LocalSeatView.Equipment)
	{
		Blocks.Add(Item.Description.IsEmpty()
			? Item.Name
			: FText::Format(NSLOCTEXT("BG", "InfoEquipBlock", "{0}\n{1}"), Item.Name, Item.Description));
	}

	return FText::Join(FText::FromString(TEXT("\n\n")), Blocks);
}

UWidget* UBGTurnPanelWidget::MakeJobTooltip()
{
	//	직업 설명은 DT_BGCharacterRow 에서 온다 — 직업마다 다르므로 툴팁 표에 둘 수 없다.
	const FBGInfoTooltipRow* Row = FindInfoRow(TEXT("Info.Job"));

	return MakeInfoTooltip(PreferLive(LocalSeatView.JobName, InfoTitle(Row)),
		PreferLive(LocalSeatView.JobDescription, InfoBody(Row)));
}

UWidget* UBGTurnPanelWidget::MakeEquipmentTooltip()
{
	const FBGInfoTooltipRow* Row = FindInfoRow(TEXT("Info.Equipment"));

	return MakeInfoTooltip(InfoTitle(Row), PreferLive(MakeEquipmentLines(), InfoBody(Row)));
}

UWidget* UBGTurnPanelWidget::MakeHandLimitTooltip()
{
	//	손패는 "값이 없는 상태"가 없다 — 숫자를 제목으로 올려야 표의 설명이 본문에 남는다.
	const FBGInfoTooltipRow* Row = FindInfoRow(TEXT("Info.HandLimit"));

	const FText Counts = TurnView.HandLimit == INDEX_NONE
		? FText::Format(NSLOCTEXT("BG", "InfoHandNoLimit", "{0} / -"), TurnView.HandCount)
		: FText::Format(NSLOCTEXT("BG", "InfoHandLive", "{0} / {1}"), TurnView.HandCount, TurnView.HandLimit);

	//	"손패 3 / 5" — 라벨은 표에서, 숫자만 여기서 붙인다.
	const FText Label = InfoTitle(Row);
	const FText Title = Label.IsEmpty()
		? Counts
		: FText::Format(NSLOCTEXT("BG", "InfoHandTitle", "{0} {1}"), Label, Counts);

	return MakeInfoTooltip(Title, InfoBody(Row));
}

UWidget* UBGTurnPanelWidget::MakeRoleTooltip()
{
	//	역할별 Row 이름은 태그 문자열 그대로다(Role.Sheriff). 배정 전에는 Info.Role 이 뜬다.
	const FBGInfoTooltipRow* Fallback = FindInfoRow(TEXT("Info.Role"));
	const FBGInfoTooltipRow* Row = LocalSeatView.RoleTag.IsValid()
		? FindInfoRow(FName(*LocalSeatView.RoleTag.ToString()))
		: nullptr;

	return MakeInfoTooltip(PreferLive(LocalSeatView.RoleName, InfoTitle(Fallback)),
		PreferLive(InfoBody(Row), InfoBody(Fallback)));
}

bool UBGTurnPanelWidget::TryBindRope()
{
	if (Rope.IsValid())
	{
		return true;
	}

	UBGRopeComponent* Found = UBGRopeComponent::Find(GetWorld());
	if (!Found)
	{
		return false;   //	GameState 가 아직 안 왔다(클라 접속 직후). 다음 틱에 다시 본다.
	}

	Rope = Found;
	Found->OnRopeChanged.AddUniqueDynamic(this, &UBGTurnPanelWidget::HandleRopeChanged);

	//	위젯이 밧줄보다 늦게 만들어졌으면 점화 통지를 이미 놓쳤다 — 지금 상태로 한 번 맞춘다.
	RefreshRopeText();
	RefreshRopeFeather();
	return true;
}

void UBGTurnPanelWidget::HandleRopeChanged(int32 Seat, float SecondsRemaining, float TotalSeconds)
{
	RefreshRopeText();
	RefreshRopeFeather();
}

UMaterialInstanceDynamic* UBGTurnPanelWidget::GetRopeMaterial()
{
	if (RopeMaterial)
	{
		return RopeMaterial;
	}

	if (!Image_Rope)
	{
		return nullptr;   // WBP 에 밧줄 그림을 안 올렸다. 선택 사항이다.
	}

	//	⚠️ 브러시에 머티리얼이 아니라 텍스처가 들어 있으면 여기서 nullptr 이 나온다 —
	//	   그러면 그림은 뜨지만 타지 않는다. WBP 에서 M_Feather_Burn 을 물릴 것.
	RopeMaterial = Image_Rope->GetDynamicMaterial();
	if (!RopeMaterial)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[TurnPanel] Image_Rope 브러시에 머티리얼이 없다 — 밧줄이 타지 않는다."));
	}
	return RopeMaterial;
}

void UBGTurnPanelWidget::RefreshRopeFeather()
{
	if (!Image_Rope)
	{
		return;
	}

	const UBGRopeComponent* R = Rope.Get();
	const bool bBurning = R && R->IsBurning();

	//	타는 중이 아니면 자리째 접는다. 다 탄 깃털을 남겨 두면 다음 턴까지 숯이 걸려 있다.
	Image_Rope->SetVisibility(bBurning
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);

	if (!bBurning)
	{
		return;
	}

	if (UMaterialInstanceDynamic* M = GetRopeMaterial())
	{
		M->SetScalarParameterValue(TEXT("Remaining"), FMath::Clamp(R->GetRemainingRatio(), 0.f, 1.f));
	}
}

void UBGTurnPanelWidget::RefreshRopeText()
{
	const UBGRopeComponent* R = Rope.Get();

	FString Label = TEXT("밧줄 -");
	if (R && R->IsBurning())
	{
		//	내 밧줄인지 남의 밧줄인지 구분해 둔다 — 지금은 텍스트뿐이라 이 표시가 유일한 단서다.
		const bool bMine = R->IsSeatBurning(TurnView.MySeat);
		Label = FString::Printf(TEXT("밧줄 %s%.0f초 (좌석 %d)%s"),
			bMine ? TEXT("[내 차례] ") : TEXT(""), R->GetSecondsRemaining(), R->GetRopeSeat(),
			R->IsRopePaused() ? TEXT(" [멈춤]") : TEXT(""));
	}

	if (Text_Rope)
	{
		Text_Rope->SetText(FText::FromString(Label));
	}
}

void UBGTurnPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	//	Text_Rope 가 없어도 부른다 — 그때는 화면 표시로 폴백하기 때문이다.
	if (TryBindRope())
	{
		RefreshRopeText();     //	남은 초는 매 프레임 줄어든다 — 델리게이트로는 표현할 수 없다.
		RefreshRopeFeather();  //	타는 자리도 마찬가지다.
	}
}

void UBGTurnPanelWidget::NativeDestruct()
{
	if (UBGRopeComponent* R = Rope.Get())
	{
		R->OnRopeChanged.RemoveDynamic(this, &UBGTurnPanelWidget::HandleRopeChanged);
	}
	Rope = nullptr;

	Super::NativeDestruct();
}

void UBGTurnPanelWidget::SetTurnView(const FBGTurnView& InTurnView)
{
	TurnView = InTurnView;
	RefreshVisual();
}

void UBGTurnPanelWidget::HandleEndTurnClicked()
{
	//	버튼이 눌렸다는 사실만 알린다. 실제 검증은 서버가 한다.
	OnEndTurnRequested.Broadcast();
}

void UBGTurnPanelWidget::RefreshVisual()
{
	if (Text_Status)
	{
		Text_Status->SetText(TurnView.StatusText);
	}

	if (Text_HandLimit)
	{
		//	무슨 숫자인지는 옆의 아이콘이 말해 준다. 한도가 없으면(설정 문제로 Health 를 못 읽은 경우) '-' 로 둔다.
		Text_HandLimit->SetText(TurnView.HandLimit == INDEX_NONE
			? FText::FromString(FString::Printf(TEXT("%d / -"), TurnView.HandCount))
			: FText::FromString(FString::Printf(TEXT("%d / %d"), TurnView.HandCount, TurnView.HandLimit)));
	}

	if (Text_Deck)
	{
		Text_Deck->SetText(FText::FromString(FString::Printf(TEXT("%d"), TurnView.DeckCount)));
	}

	if (Text_CardUse)
	{
		//	GDD §10: 턴 UI 에 "사용한 카드 수 / 현재 카드 사용 한도" 를 표시한다.
		Text_CardUse->SetText(FText::FromString(
			FString::Printf(TEXT("카드 사용 %d / %d"), TurnView.CardsUsed, TurnView.CardUseLimit)));
	}

	if (Button_EndTurn)
	{
		//	버리기 페이즈에서는 턴 종료를 누를 수 없다 — 카드를 버려야 넘어간다.
		//	그래서 활성 조건은 bIsMyTurn(= Play 페이즈) 뿐이다.
		Button_EndTurn->SetIsEnabled(TurnView.bIsMyTurn);
	}

	if (bHideWhenNotMyTurn)
	{
		SetVisibility((TurnView.bIsMyTurn || TurnView.bIsMyDiscard)
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UBGTurnPanelWidget::SetLocalSeatInfo(const FBGSeatView& InView)
{
	LocalSeatView = InView;

	//	이름과 직업은 판이 시작되기 전에도 보여 준다. 자리에 앉은 것만으로 정해지는 값이다.
	if (Text_Name) { Text_Name->SetText(UBGSeatViewLibrary::ClampDisplayName(LocalSeatView.PlayerName)); }
	if (Text_Job)  { Text_Job->SetText(LocalSeatView.JobName); }

	//	판 시작 전에는 값이 없다. 0/0 을 그대로 보여주면 죽은 것처럼 읽힌다.
	const bool bShowStats = ShouldShowLocalStats();

	if (Text_Health)
	{
		Text_Health->SetVisibility(bShowStats ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowStats)
		{
			//	무슨 숫자인지는 옆의 하트 아이콘이 말해 준다 — 손패 줄과 같은 표기다.
			Text_Health->SetText(FText::Format(NSLOCTEXT("BG", "LocalHealth", "{0} / {1}"),
				LocalSeatView.Health, LocalSeatView.MaxHealth));
		}
	}

	if (Text_Equipment)
	{
		Text_Equipment->SetVisibility(bShowStats ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowStats)
		{
			Text_Equipment->SetText(LocalSeatView.Equipment.IsEmpty()
				? NSLOCTEXT("BG", "LocalNoEquipment", "장비 없음")
				//	서로 다른 장비는 여러 개를 함께 낄 수 있다 — 한 줄에 이으면 패널 폭을 넘으므로 한 개씩 줄을 나눈다.
				: FText::Join(NSLOCTEXT("BG", "LocalEquipSeparator", "\n"), LocalSeatView.GetEquipmentNames()));
		}
	}

	//	역할은 판이 시작돼야 배정된다. 본인 것이라 숨기지 않되, 오기 전에는 줄째로 감춘다.
	if (Text_Role)
	{
		const bool bHasRole = !LocalSeatView.RoleName.IsEmpty();

		Text_Role->SetVisibility(bHasRole ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasRole)
		{
			Text_Role->SetText(LocalSeatView.RoleName);
		}
	}

	RefreshHealthHearts();
	RefreshIcons();

	OnLocalSeatInfoChanged(LocalSeatView);
}

void UBGTurnPanelWidget::RefreshHealthHearts()
{
	UBGHeartSlotWidget* const Hearts[] = { Heart_1, Heart_2, Heart_3, Heart_4, Heart_5, Heart_6 };

	//	판 전에는 최대 체력이 0 이다 — 줄째로 접힌다.
	UBGHeartSlotWidget::RefreshRow(Hearts, LocalSeatView.Health, ShouldShowLocalStats() ? LocalSeatView.MaxHealth : 0,
		HeartRow);
}

void UBGTurnPanelWidget::RefreshIcons()
{
	//	태그를 못 찾으면(배정 전이거나 역할이 숨겨져 있으면) WBP 가 깔아 둔 범용 아이콘으로
	//	되돌린다. 접으면 아이콘 자리가 통째로 빠져 그 줄만 쪼그라들고, 아이콘이 있는 장비·손패
	//	줄과 높이가 어긋난다. (UBGSeatInfoWidget::RefreshIcons 와 같은 규칙이다.)
	struct FIconSlot
	{
		UImage* Image;
		const TMap<FGameplayTag, TObjectPtr<UTexture2D>>& Table;
		FGameplayTag Tag;
		TObjectPtr<UTexture2D>& Fallback;
	};
	const FIconSlot Slots[] = {
		{ Img_JobIcon,  JobIcons,  LocalSeatView.JobTag,  JobIconDefault  },
		{ Img_RoleIcon, RoleIcons, LocalSeatView.RoleTag, RoleIconDefault },
		{ Img_Portrait, JobPortraits, LocalSeatView.JobTag, PortraitDefault },
	};

	for (const FIconSlot& IconSlot : Slots)
	{
		if (!IconSlot.Image)
		{
			continue;
		}

		//	WBP 가 깔아 둔 것은 처음 한 번만 기억한다 — 한 번 갈아끼우고 나면 읽을 수 없다.
		if (!IconSlot.Fallback)
		{
			IconSlot.Fallback = Cast<UTexture2D>(IconSlot.Image->GetBrush().GetResourceObject());
		}

		const TObjectPtr<UTexture2D>* Found = IconSlot.Tag.IsValid() ? IconSlot.Table.Find(IconSlot.Tag) : nullptr;
		UTexture2D* Icon = Found ? Found->Get() : IconSlot.Fallback.Get();

		//	범용 아이콘조차 없으면 흰 사각형이 남으므로 그때만 감춘다. 접지 않고 감추는 것은
		//	자리를 남겨 줄 높이를 유지하기 위해서다.
		IconSlot.Image->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (Icon)
		{
			//	bMatchSize=false — 크기는 WBP 의 슬롯이 정한다. 텍스처 해상도를 따라가면
			//	직업마다 아이콘 크기가 달라진다.
			IconSlot.Image->SetBrushFromTexture(Icon, /*bMatchSize=*/ false);
		}
	}
}
