#include "UI/Widget/BGIntroRoleCardWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/View/BGInfoTooltipRow.h"
#include "UI/View/BGSeatViewLibrary.h"

void UBGIntroRoleCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bWaitingForValues = !RefreshFromGame();
}

void UBGIntroRoleCardWidget::PrepareCard()
{
	//	화면에는 아직 올리지 않는다 — 값을 읽고 그림 요청만 걸어 둔다.
	bWaitingForValues = !RefreshFromGame();
}

void UBGIntroRoleCardWidget::ShowCard()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RemoveTimer);
	}

	bCardShown = true;
	bHiding = false;
	bFallbackOut = false;
	RevealElapsed = 0.f;
	SetRenderOpacity(1.f);
	SetRenderTransform(FWidgetTransform());
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Border_JobAbility) { Border_JobAbility->SetRenderOpacity(0.f); }
	if (Border_SecretRole) { Border_SecretRole->SetRenderOpacity(0.f); }
	bWaitingForValues = !RefreshFromGame();

	if (Anim_In)
	{
		PlayAnimation(Anim_In);
	}
}

void UBGIntroRoleCardWidget::HideCard()
{
	if (bHiding) { return; }
	bHiding = true;
	bCardShown = false;
	if (Anim_In) { StopAnimation(Anim_In); }
	if (!Anim_Out)
	{
		//	WBP 에 퇴장 애니메이션이 없다 — 작아지며 정보창 쪽으로 빨려 들어가는 연출을 코드가 낸다.
		bFallbackOut = true;
		FallbackOutElapsed = 0.f;
		return;
	}

	PlayAnimation(Anim_Out);

	//	애니메이션이 끝난 뒤에 뗀다. 바로 떼면 퇴장 연출이 한 프레임도 안 보인다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RemoveTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { RemoveFromParent(); }),
			FMath::Max(Anim_Out->GetEndTime(), 0.01f), /*bLoop=*/false);
	}
}

void UBGIntroRoleCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bFallbackOut)
	{
		TickFallbackOut(InDeltaTime);
		return;   //	나가는 중에 값을 다시 채울 이유가 없다.
	}

	UpdateReveal(InDeltaTime);

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

	bWaitingForValues = !RefreshFromGame();
}

//	"역할 카드가 내 정보창으로 작아지며 들어감" — 줄어들면서 좌하단으로 밀리고 투명해진다.
void UBGIntroRoleCardWidget::TickFallbackOut(float DeltaTime)
{
	FallbackOutElapsed += DeltaTime;

	const float Alpha = FallbackOutSeconds > 0.f
		? FMath::Clamp(FallbackOutElapsed / FallbackOutSeconds, 0.f, 1.f)
		: 1.f;

	if (Alpha >= 1.f)
	{
		bFallbackOut = false;
		RemoveFromParent();
		return;
	}

	//	처음엔 빠르게 빨려 들어가고 끝에서 잦아든다.
	const float Eased = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);
	const float Scale = FMath::Lerp(1.f, FallbackOutScale, Eased);

	FWidgetTransform Transform;
	Transform.Scale = FVector2D(Scale, Scale);
	Transform.Translation = FallbackOutOffset * Eased;

	SetRenderTransform(Transform);
	SetRenderOpacity(1.f - Eased);
}

void UBGIntroRoleCardWidget::NativeDestruct()
{
	bCardShown = false;
	//	⚠️ 타이머가 파괴 뒤에 발동하면 죽은 위젯을 건드린다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RemoveTimer);
	}

	Super::NativeDestruct();
}

//	카드 그림은 직업 표(DT_BGCharacterRow)가 준다 — 위젯이 직업→텍스처 표를 따로 들지 않는다.
//
//	⚠️ 동기 로드(LoadSynchronous)를 쓰지 않는다. 그림이 아직 메모리에 없으면 로드가 끝날 때까지
//	   게임 스레드가 멈추는데, 하필 그 시점이 화면이 막 밝아지는 순간이라 끊김이 가장 잘 보인다.
void UBGIntroRoleCardWidget::RequestCardArt()
{
	const FSoftObjectPath Path = View.IntroCardTexture.ToSoftObjectPath();
	if (Path == RequestedCardArt)
	{
		return;   //	같은 그림을 이미 요청했다. 폴링이 여러 번 돌아도 요청은 한 번뿐이다.
	}

	RequestedCardArt = Path;
	CardArtHandle.Reset();

	if (Path.IsNull())
	{
		ApplyCardArt();   //	이 직업은 그림이 없다 — 배경을 비운다.
		return;
	}

	CardArtHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Path, FStreamableDelegate::CreateWeakLambda(this, [this]() { ApplyCardArt(); }));
}

void UBGIntroRoleCardWidget::ApplyCardArt()
{
	if (!Border_Root)
	{
		return;
	}

	UTexture2D* CardTexture = View.IntroCardTexture.Get();

	Border_Root->SetBrushFromTexture(CardTexture);

	//	아직 안 왔거나 그림이 없는 직업이면 배경을 비운다. 안 그러면 흰 사각형이 남는다.
	Border_Root->SetBrushColor(CardTexture ? FLinearColor::White : FLinearColor::Transparent);
}

bool UBGIntroRoleCardWidget::RefreshFromGame()
{
	const APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return false;
	}

	//	내 좌석 것만 고른다. 남의 역할은 애초에 이 목록에 공개된 것만 들어 있다.
	bool bFound = false;
	for (const FBGSeatView& Candidate : UBGSeatViewLibrary::MakeSeatViews(PC))
	{
		if (Candidate.bIsLocalPlayer)
		{
			View = Candidate;
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		return false;   //	좌석이 아직 안 왔다. 다음 주기에 다시 본다.
	}

	//	그림 요청은 값이 바뀌었는지와 무관하게 먼저 본다 — 로드가 끝나 반영만 남은 경우가 있다.
	RequestCardArt();
	ApplyCardArt();

	//	⚠️ 값이 다 올 때까지 폴링이 도는 자리다. 같은 값이면 여기서 멈춘다 —
	//	   안 그러면 0.2초마다 글자를 다시 칠하고 OnRoleInfoChanged 도 계속 쏜다.
	if (AppliedJobTag == View.JobTag && AppliedRoleTag == View.RoleTag)
	{
		return View.JobTag.IsValid() && View.RoleTag.IsValid();
	}

	AppliedJobTag  = View.JobTag;
	AppliedRoleTag = View.RoleTag;

	if (Text_Job)
	{
		Text_Job->SetText(View.JobName);
	}

	if (Text_JobAbility)
	{
		Text_JobAbility->SetText(View.JobDescription);
	}

	if (Text_Role)
	{
		Text_Role->SetText(View.RoleName);
	}

	if (Text_WinCondition)
	{
		//	역할별 Row 이름은 태그 문자열 그대로다(Role.Outlaw). 표나 Row 가 없으면 줄째로 접는다.
		const FBGInfoTooltipRow* Row = (InfoTable && View.RoleTag.IsValid())
			? InfoTable->FindRow<FBGInfoTooltipRow>(
				FName(*View.RoleTag.ToString()), TEXT("BGIntroRoleCard"), /*bWarnIfMissing=*/false)
			: nullptr;

		const FText Body = Row ? Row->Body : FText::GetEmpty();

		Text_WinCondition->SetVisibility(Body.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
		Text_WinCondition->SetText(Body);
	}

	OnRoleInfoChanged(View);

	//	직업과 역할이 둘 다 와야 끝이다. 역할은 본인에게만 오는 값이라 늦게 도착하는 쪽이다.
	return View.JobTag.IsValid() && View.RoleTag.IsValid();
}

void UBGIntroRoleCardWidget::UpdateReveal(float DeltaTime)
{
	if (!bCardShown || bHiding || bWaitingForValues)
	{
		return;
	}

	//	그림이 아직 오는 중이면 기다린다 — 빈 카드 위에서 글자만 떠오르면 어색하다.
	if (CardArtHandle.IsValid() && !CardArtHandle->HasLoadCompleted())
	{
		return;
	}

	RevealElapsed += DeltaTime;
	const float JobAlpha = FMath::Clamp((RevealElapsed - 0.3f) / 0.25f, 0.f, 1.f);
	const float RoleAlpha = FMath::Clamp((RevealElapsed - 0.65f) / 0.25f, 0.f, 1.f);
	if (Border_JobAbility) { Border_JobAbility->SetRenderOpacity(JobAlpha); }
	if (Border_SecretRole) { Border_SecretRole->SetRenderOpacity(RoleAlpha); }
}
