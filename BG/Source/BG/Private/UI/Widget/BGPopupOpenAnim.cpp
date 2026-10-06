#include "UI/Widget/BGPopupOpenAnim.h"

#include "Components/Widget.h"

namespace
{
	// 펼쳐지는 데 걸리는 시간(초). 길면 메뉴가 굼떠 보인다.
	constexpr float PopupOpenDuration = 0.22f;

	// 시작할 때의 세로 배율. 가로는 처음부터 제 폭이다 — 위아래로만 펼쳐진다.
	constexpr float PopupStartScaleY = 0.7f;

	// 투명도는 전체 시간의 이 비율 안에 다 차오른다. 늦게 차면 펼쳐진 뒤에도 비쳐 보인다.
	constexpr float PopupFadePortion = 0.6f;

	void ApplyPopupPose(UWidget& Widget, float ScaleY, float Opacity)
	{
		Widget.SetRenderScale(FVector2D(1.f, ScaleY));
		Widget.SetRenderOpacity(Opacity);
	}
}

void FBGPopupOpenAnim::Start(UWidget* InTarget)
{
	Target = InTarget;
	if (!InTarget)
	{
		Elapsed = -1.f;
		return;
	}

	Elapsed = 0.f;
	InTarget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	ApplyPopupPose(*InTarget, PopupStartScaleY, 0.f);
}

void FBGPopupOpenAnim::Tick(float DeltaTime)
{
	if (Elapsed < 0.f)
	{
		return;
	}

	UWidget* Widget = Target.Get();
	if (!Widget)
	{
		Elapsed = -1.f;
		return;
	}

	Elapsed += DeltaTime;
	const float Alpha = FMath::Clamp(Elapsed / PopupOpenDuration, 0.f, 1.f);
	const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f);   // ease-out cubic
	const float Fade = FMath::Clamp(Alpha / PopupFadePortion, 0.f, 1.f);

	ApplyPopupPose(*Widget, FMath::Lerp(PopupStartScaleY, 1.f, Eased), Fade);

	if (Alpha >= 1.f)
	{
		Elapsed = -1.f;
	}
}
