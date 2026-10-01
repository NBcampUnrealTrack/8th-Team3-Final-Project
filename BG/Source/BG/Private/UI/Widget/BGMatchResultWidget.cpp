#include "UI/Widget/BGMatchResultWidget.h"

#include "BGGameplayTags.h"
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

void UBGMatchResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
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

bool UBGMatchResultWidget::DidIWin() const
{
	const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwningPlayer());
	return PC && PC->DidIWin(GetWinningRole());
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
    if (Text_ResultTitle) Text_ResultTitle->SetText(Title);
    if (Text_ResultReason) Text_ResultReason->SetText(Reason);
    if (!Rows_Container || !WidgetTree) return;
    Rows_Container->ClearChildren();
    for (const FBGMatchResultRow& Row : InRows)
    {
        UBorder* Background=WidgetTree->ConstructWidget<UBorder>();
        Background->SetPadding(FMargin(16,8));
        Background->SetBrushColor(Row.Seat.bIsLocalPlayer?FLinearColor(.16f,.105f,.21f,.85f):FLinearColor(.04f,.025f,.017f,.7f));
        UHorizontalBox* Line=WidgetTree->ConstructWidget<UHorizontalBox>();
        Background->SetContent(Line);
        const FText Values[]={Row.Seat.bIsLocalPlayer?FText::Format(NSLOCTEXT("BGEnding","Me","{0} · 나"),Row.Seat.PlayerName):Row.Seat.PlayerName,Row.Seat.JobName,Row.Seat.RoleName,
            Row.Seat.bIsAlive?NSLOCTEXT("BGEnding","Alive","생존"):NSLOCTEXT("BGEnding","Dead","탈락"),
            !Row.Seat.RoleTag.IsValid()?NSLOCTEXT("BGEnding","Pending","확인 중"):Row.bWinner?NSLOCTEXT("BGEnding","Win","승리"):NSLOCTEXT("BGEnding","Lose","패배")};
        const float Widths[]={190,125,130,85,100};
        for (int32 Col=0;Col<5;++Col)
        {
            USizeBox* Cell=WidgetTree->ConstructWidget<USizeBox>();Cell->SetWidthOverride(Widths[Col]);Cell->SetMinDesiredHeight(34);
            UTextBlock* Text=WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(Values[Col]);
            FSlateFontInfo Font=ResultFont;Font.Size=18;Text->SetFont(Font);
            Text->SetAutoWrapText(true);Text->SetWrapTextAt(Widths[Col]-12);
            Text->SetColorAndOpacity(FSlateColor(Col==4&&Row.bWinner?FLinearColor(.83f,.56f,.23f):FLinearColor(.83f,.77f,.65f)));
            Cell->SetContent(Text);Line->AddChildToHorizontalBox(Cell)->SetVerticalAlignment(VAlign_Center);
        }
        Rows_Container->AddChildToVerticalBox(Background)->SetPadding(FMargin(0,0,0,2));
    }
}
