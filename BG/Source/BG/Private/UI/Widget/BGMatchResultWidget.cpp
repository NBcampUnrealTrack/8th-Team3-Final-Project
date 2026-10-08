#include "UI/Widget/BGMatchResultWidget.h"

#include "BGGameplayTags.h"
#include "Audio/BGAudioSubsystem.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "Core/BGGameMode.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "UI/View/BGSeatViewLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/GameInstance.h"

namespace
{
	//	행 첫 칸이 시작하는 x. Scroll_Rows 왼쪽(24) + 행 배경 안쪽 여백(16).
	constexpr float ResultColumnStartX = 40.f;

	//	열 폭. 합은 둘 다 638 = 행 폭 670 - 좌우 여백 16x2.
	//	PvP 는 플레이어 이름이 길어 첫 칸을 넓히고 왼쪽 정렬한다.
	//	PvE 는 값이 모두 짧아("1조우", "00:00") 고르게 나누고 가운데 정렬한다 — 왼쪽 정렬이면
	//	헤더와 값의 길이 차이만큼 중심이 어긋나 보인다.
	constexpr float PvPColumnWidths[] = {190.f, 125.f, 130.f, 85.f, 108.f};
	constexpr float PvEColumnWidths[] = {128.f, 127.f, 128.f, 127.f, 128.f};
	constexpr int32 ResultColumnCount = UE_ARRAY_COUNT(PvPColumnWidths);
	static_assert(UE_ARRAY_COUNT(PvEColumnWidths) == ResultColumnCount, "열 수가 같아야 헤더 5칸을 함께 쓴다");
}

void UBGMatchResultWidget::LayoutHeaders(const float* Widths, bool bCenter)
{
	//	헤더는 WBP 캔버스에 있지만 위치는 여기서 열 폭으로 계산한다 — 행과 숫자가 따로 놀지 않게.
	//	세로는 WBP 값(구분선과 행 사이 가운데)을 그대로 쓴다.
	UTextBlock* Headers[] = {Header_0, Header_1, Header_2, Header_3, Header_4};
	float X = ResultColumnStartX;
	for (int32 Col = 0; Col < ResultColumnCount; ++Col)
	{
		if (UTextBlock* Header = Headers[Col])
		{
			Header->SetJustification(bCenter ? ETextJustify::Center : ETextJustify::Left);
			if (UCanvasPanelSlot* HeaderSlot = Cast<UCanvasPanelSlot>(Header->Slot))
			{
				//	크기는 글자에 맞춰지므로(Size To Content) 가운데 정렬은 기준점을 열 중심에 둔다.
				HeaderSlot->SetAlignment(FVector2D(bCenter ? 0.5f : 0.f, HeaderSlot->GetAlignment().Y));
				HeaderSlot->SetPosition(FVector2D(bCenter ? X + Widths[Col] * 0.5f : X, HeaderSlot->GetPosition().Y));
			}
		}
		X += Widths[Col];
	}
}

void UBGMatchResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->BindButtonSoundGroup(Button_Return, EBGUIButtonSoundGroup::Navigation);
	}
	if (Button_Return) Button_Return->OnClicked.AddUniqueDynamic(this, &UBGMatchResultWidget::RequestReturnToLobby);

	bWaitingForValues = !RefreshRows();
}

void UBGMatchResultWidget::ShowResult()
{
	if (Banner_Victory)
	{
		Banner_Victory->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Background_Dim)
	{
		Background_Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Scale_Result)
	{
		Scale_Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Scale_Result->SetRenderOpacity(0.f);
		Scale_Result->SetRenderTranslation(FVector2D(40.f, 0.f));
		ResultElapsed = 0.f;
	}

	SetVisibility(ESlateVisibility::Visible);   //	버튼을 눌러야 하므로 입력을 받는다.
	bWaitingForValues = !RefreshRows();
}

FGameplayTag UBGMatchResultWidget::GetWinningRole() const
{
	const UWorld* World = GetWorld();
	const ABGGameState* GS = World ? World->GetGameState<ABGGameState>() : nullptr;

	return GS ? GS->GetWinningRole() : FGameplayTag();
}

void UBGMatchResultWidget::RequestReturnToLobby()
{
	if (ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwningPlayer()))
	{
		PC->RequestReturnToLobby();
	}
}

void UBGMatchResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ResultElapsed >= 0.f && Scale_Result)
	{
		ResultElapsed += InDeltaTime;
		const float Alpha = FMath::Clamp(ResultElapsed / 0.5f, 0.f, 1.f);
		const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f);   // ease-out cubic
		Scale_Result->SetRenderOpacity(Eased);
		Scale_Result->SetRenderTranslation(FVector2D(40.f * (1.f - Eased), 0.f));
		if (Alpha >= 1.f)
		{
			ResultElapsed = -1.f;
		}
	}

	if (!bWaitingForValues)
	{
		return;
	}

	//	값이 늦게 오는 동안만 도는 폴링이다. 다 채워지면 스스로 멈춘다.
	TimeSinceRetry += InDeltaTime;
	if (TimeSinceRetry < RetryInterval)
	{
		return;
	}
	TimeSinceRetry = 0.f;

	bWaitingForValues = !RefreshRows();
}

bool UBGMatchResultWidget::RefreshRows()
{
	const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwningPlayer());
	const UWorld* World = GetWorld();
	const ABGGameState* GS = World ? World->GetGameState<ABGGameState>() : nullptr;

	if (!PC || !GS)
	{
		return false;
	}

	TArray<FBGSeatView> Views = UBGSeatViewLibrary::MakeSeatViews(PC);
	if (Views.IsEmpty())
	{
		return false;
	}

	//	표는 좌석 순서로 읽는다 — 매 판 줄 순서가 달라지면 누가 누구인지 찾기 어렵다.
	Views.Sort([](const FBGSeatView& A, const FBGSeatView& B) { return A.SeatIndex < B.SeatIndex; });

	TArray<FBGMatchResultRow> NewRows;
	NewRows.Reserve(Views.Num());

	bool bAllRolesKnown = GS->GetWinningRole().IsValid();
	for (const FBGSeatView& View : Views)
	{
		FBGMatchResultRow& Row = NewRows.AddDefaulted_GetRef();
		Row.Seat    = View;
		Row.bWinner = GS->IsRoleWinner(View.RoleTag);

		//	판이 끝나면 전원 공개되지만, 그 복제가 아직 안 온 좌석이 있을 수 있다.
		bAllRolesKnown &= View.RoleTag.IsValid();
	}

	//	⚠️ 값이 다 올 때까지 이 함수가 0.2초마다 돈다. 같은 내용이면 여기서 멈춘다 —
	//	   표를 다시 만들면 줄이 통째로 새로 생겨 등장 연출과 스크롤 위치가 초기화된다.
	if (RowsEqual(Rows, NewRows))
	{
		return bAllRolesKnown;
	}

	Rows = MoveTemp(NewRows);
	OnResultReady(Rows);

	return bAllRolesKnown;
}

//	표에 그리는 값만 본다 — 체력이나 손패처럼 결과 화면이 쓰지 않는 값이 바뀌었다고
//	줄을 다시 만들 이유가 없다.
bool UBGMatchResultWidget::RowsEqual(const TArray<FBGMatchResultRow>& A, const TArray<FBGMatchResultRow>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		const FBGMatchResultRow& Left  = A[Index];
		const FBGMatchResultRow& Right = B[Index];

		if (Left.bWinner != Right.bWinner
			|| Left.Seat.SeatIndex != Right.Seat.SeatIndex
			|| Left.Seat.bIsAlive != Right.Seat.bIsAlive
			|| Left.Seat.bIsLocalPlayer != Right.Seat.bIsLocalPlayer
			|| Left.Seat.RoleTag != Right.Seat.RoleTag
			|| Left.Seat.JobTag != Right.Seat.JobTag
			|| !Left.Seat.PlayerName.EqualTo(Right.Seat.PlayerName))
		{
			return false;
		}
	}

	return true;
}

void UBGMatchResultWidget::NativeDestruct()
{
    if (Button_Return) Button_Return->OnClicked.RemoveDynamic(this, &UBGMatchResultWidget::RequestReturnToLobby);
    Super::NativeDestruct();
}

void UBGMatchResultWidget::OnResultReady_Implementation(const TArray<FBGMatchResultRow>& InRows)
{
    //	⚠️ 태그를 문자열로 비교하지 않는다 — 오타나 태그 이름 변경을 컴파일러가 잡지 못해
    //	   아무 문구도 안 뜨는 채로 넘어간다.
    const FGameplayTag Winner = GetWinningRole();
    FText Title=NSLOCTEXT("BGEnding","Loading","결과 확인 중");
    FText Reason;
    if (Winner == BG::Role::Sheriff.GetTag()) { Title=NSLOCTEXT("BGEnding","Sheriff","보안관 진영 승리"); Reason=NSLOCTEXT("BGEnding","SheriffReason","무법자와 배신자가 모두 쓰러졌습니다."); }
    else if (Winner == BG::Role::Outlaw.GetTag()) { Title=NSLOCTEXT("BGEnding","Outlaw","무법자 진영 승리"); Reason=NSLOCTEXT("BGEnding","OutlawReason","보안관이 쓰러지고, 무법자가 승리했습니다."); }
    else if (Winner == BG::Role::Renegade.GetTag()) { Title=NSLOCTEXT("BGEnding","Renegade","배신자 단독 승리"); Reason=NSLOCTEXT("BGEnding","RenegadeReason","배신자가 마지막까지 살아남았습니다."); }
    const UWorld* World = GetWorld();
    const ABGGameState* GS = World ? World->GetGameState<ABGGameState>() : nullptr;
    if (GS && ABGGameMode::GetPlayMode(this) == EBGPlayMode::PvE)
    {
        ShowPvERunRecords(*GS, InRows);
        return;
    }
    if (Text_ResultTitle) Text_ResultTitle->SetText(Title);
    if (Text_ResultReason) Text_ResultReason->SetText(Reason);
    LayoutHeaders(PvPColumnWidths, false);
    if (!Rows_Container || !WidgetTree) return;
    Rows_Container->ClearChildren();
    for (const FBGMatchResultRow& Row : InRows)
    {
        UBorder* Background=WidgetTree->ConstructWidget<UBorder>();
        Background->SetPadding(FMargin(16,8));
        //	턴 패널과 같은 색 — 내 줄은 선택된 탭처럼 호박색으로, 나머지는 어두운 회색으로 둔다.
        Background->SetBrushColor(Row.Seat.bIsLocalPlayer?FLinearColor(.10f,.055f,.012f,.85f):FLinearColor(.028f,.030f,.036f,.75f));
        UHorizontalBox* Line=WidgetTree->ConstructWidget<UHorizontalBox>();
        Background->SetContent(Line);
        const FText Values[]={Row.Seat.bIsLocalPlayer?FText::Format(NSLOCTEXT("BGEnding","Me","{0} · 나"),Row.Seat.PlayerName):Row.Seat.PlayerName,Row.Seat.JobName,Row.Seat.RoleName,
            Row.Seat.bIsAlive?NSLOCTEXT("BGEnding","Alive","생존"):NSLOCTEXT("BGEnding","Dead","탈락"),
            !Row.Seat.RoleTag.IsValid()?NSLOCTEXT("BGEnding","Pending","확인 중"):Row.bWinner?NSLOCTEXT("BGEnding","Win","승리"):NSLOCTEXT("BGEnding","Lose","패배")};
        const float* Widths=PvPColumnWidths;
        for (int32 Col=0;Col<5;++Col)
        {
            USizeBox* Cell=WidgetTree->ConstructWidget<USizeBox>();Cell->SetWidthOverride(Widths[Col]);Cell->SetMinDesiredHeight(34);
            UTextBlock* Text=WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(Values[Col]);
            FSlateFontInfo Font=ResultFont;Font.Size=18;Text->SetFont(Font);
            Text->SetAutoWrapText(true);Text->SetWrapTextAt(Widths[Col]-12);
            Text->SetColorAndOpacity(FSlateColor(Col==4&&Row.bWinner?FLinearColor(.745f,.479f,.162f):FLinearColor(.82f,.79f,.74f)));
            Cell->SetContent(Text);Line->AddChildToHorizontalBox(Cell)->SetVerticalAlignment(VAlign_Center);
        }
        Rows_Container->AddChildToVerticalBox(Background)->SetPadding(FMargin(0,0,0,2));
    }
}

FText UBGMatchResultWidget::GetJobDisplayName(const FGameplayTag& Job) const
{
	if (!Job.IsValid())
	{
		return FText::GetEmpty();
	}

	const UGameInstance* GI = GetGameInstance();
	const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	if (const FBGCharacterRow* Row = Data ? Data->GetCharacterRow(Job) : nullptr)
	{
		if (!Row->DisplayName.IsEmpty())
		{
			return Row->DisplayName;
		}
	}

	//	DT 에 이름이 없으면 태그의 마지막 마디를 쓴다(좌석 표와 같은 폴백).
	FString Leaf = Job.ToString();
	int32 LastDot = INDEX_NONE;
	if (Leaf.FindLastChar(TEXT('.'), LastDot))
	{
		Leaf = Leaf.RightChop(LastDot + 1);
	}
	return FText::FromString(Leaf);
}

void UBGMatchResultWidget::ShowPvERunRecords(const ABGGameState& GS, const TArray<FBGMatchResultRow>& InRows)
{
	const TArray<FBGPvEEncounterRecord>& Records = GS.GetPvERunRecords();
	if (!ensureMsgf(!Records.IsEmpty(), TEXT("[PvE] 결과 화면에 표시할 런 기록이 비어 있습니다.")))
	{
		return;
	}

	const bool bCleared = GS.IsPvERunCleared();

	//	부제에 이름을 붙인다. PvE 는 사람이 한 명이라 행마다 이름을 반복할 이유가 없다.
	FText PlayerName;
	for (const FBGMatchResultRow& Row : InRows)
	{
		if (Row.Seat.bIsLocalPlayer)
		{
			PlayerName = Row.Seat.PlayerName;
			break;
		}
	}

	FText Title;
	FText Reason;
	if (bCleared)
	{
		Title  = NSLOCTEXT("BGEnding", "PvEClear", "런 클리어");
		Reason = FText::Format(NSLOCTEXT("BGEnding", "PvEClearReason", "{0}님이 모든 조우를 돌파했습니다."), PlayerName);
	}
	else
	{
		Title  = FText::Format(NSLOCTEXT("BGEnding", "PvEFail", "{0}조우에서 패배"), FText::AsNumber(Records.Last().EncounterIndex));
		Reason = FText::Format(NSLOCTEXT("BGEnding", "PvEFailReason", "{0}님이 쓰러졌습니다."), PlayerName);
	}
	if (Text_ResultTitle) Text_ResultTitle->SetText(Title);
	if (Text_ResultReason) Text_ResultReason->SetText(Reason);

	//	헤더 5칸을 그대로 쓰고 라벨과 열 배치만 PvE 용으로 바꾼다.
	if (Header_0) Header_0->SetText(NSLOCTEXT("BGEnding", "PvEHeaderEncounter", "조우"));
	if (Header_1) Header_1->SetText(NSLOCTEXT("BGEnding", "PvEHeaderJob", "직업"));
	if (Header_2) Header_2->SetText(NSLOCTEXT("BGEnding", "PvEHeaderMatchup", "대진"));
	if (Header_3) Header_3->SetText(NSLOCTEXT("BGEnding", "PvEHeaderResult", "결과"));
	if (Header_4) Header_4->SetText(NSLOCTEXT("BGEnding", "PvEHeaderTime", "플레이 시간"));

	LayoutHeaders(PvEColumnWidths, true);

	if (!Rows_Container || !WidgetTree) return;
	Rows_Container->ClearChildren();

	const float* Widths = PvEColumnWidths;
	for (const FBGPvEEncounterRecord& Record : Records)
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetPadding(FMargin(16, 8));
		Background->SetBrushColor(Record.bCleared ? FLinearColor(.04f, .025f, .017f, .7f) : FLinearColor(.2f, .06f, .05f, .8f));
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		Background->SetContent(Line);

		const int32 Seconds = FMath::RoundToInt(Record.Seconds);
		const FText Values[] = {
			FText::Format(NSLOCTEXT("BGEnding", "PvEEncounterCell", "{0}조우"), FText::AsNumber(Record.EncounterIndex)),
			GetJobDisplayName(Record.JobTag),
			FText::Format(NSLOCTEXT("BGEnding", "PvEMatchupCell", "{0} 대 {1}"), FText::AsNumber(Record.AllyCount), FText::AsNumber(Record.EnemyCount)),
			Record.bCleared ? NSLOCTEXT("BGEnding", "PvECleared", "클리어") : NSLOCTEXT("BGEnding", "PvEDefeated", "패배"),
			FText::FromString(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60))};

		for (int32 Col = 0; Col < 5; ++Col)
		{
			USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>();
			Cell->SetWidthOverride(Widths[Col]);
			Cell->SetMinDesiredHeight(34);
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
			Text->SetText(Values[Col]);
			FSlateFontInfo Font = ResultFont;
			Font.Size = 18;
			Text->SetFont(Font);
			Text->SetAutoWrapText(true);
			Text->SetWrapTextAt(Widths[Col] - 12);
			Text->SetJustification(ETextJustify::Center);
			Text->SetColorAndOpacity(FSlateColor(Col == 3 && Record.bCleared ? FLinearColor(.83f, .56f, .23f) : FLinearColor(.83f, .77f, .65f)));
			Cell->SetContent(Text);
			Line->AddChildToHorizontalBox(Cell)->SetVerticalAlignment(VAlign_Center);
		}
		Rows_Container->AddChildToVerticalBox(Background)->SetPadding(FMargin(0, 0, 0, 2));
	}
}
