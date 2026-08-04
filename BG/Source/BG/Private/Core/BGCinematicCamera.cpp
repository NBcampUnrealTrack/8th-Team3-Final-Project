#include "Core/BGCinematicCamera.h"

#include "Camera/CameraComponent.h"
#include "Core/BGPlayerBase.h"

ABGCinematicCamera::ABGCinematicCamera()
{
	PrimaryActorTick.bCanEverTick = true;

	//	로컬 전용 액터다 — 복제하지 않는다(헤더 상단 주석 참고).
	bReplicates = false;
	SetReplicatingMovement(false);

	//	연출용이라 레벨에 배치되는 카메라 목록에 끼어들 이유가 없다.
	//	켜 두면 첫 플레이어가 자동으로 이 카메라를 보게 되어 게임 시작 화면이 엉킨다.
	if (UCameraComponent* Cam = GetCameraComponent())
	{
		Cam->SetAutoActivate(true);
	}
}

bool ABGCinematicCamera::ResolveShotPov(const FBGCameraShot& Shot, FTransform& OutTransform, float& OutFov)
{
	if (!Shot.Target)
	{
		return false;
	}

	//	B 카메라는 활성화하지 않는다 — 위치와 FOV 만 읽어간다.
	//	활성화하면 그 캐릭터를 보고 있던 사람의 화면이 B 로 튄다.
	const UCameraComponent* Source = Shot.Target->GetSecondaryCamera();
	if (!Source)
	{
		return false;
	}

	OutTransform = Source->GetComponentTransform();
	OutFov       = Source->FieldOfView;
	return true;
}

void ABGCinematicCamera::BeginShot(const FBGCameraShot& Shot, float ElapsedSeconds)
{
	FTransform NewTarget;
	float NewFov = 90.f;
	if (!ResolveShotPov(Shot, NewTarget, NewFov))
	{
		return;
	}

	CurrentShot     = Shot;
	TargetTransform = NewTarget;
	TargetFov       = NewFov;

	//	출발점은 "목표" 가 아니라 "지금 화면" 이다. 이동 중에 다시 불려도 여기서 이어진다.
	StartTransform = GetActorTransform();
	StartFov       = GetCameraComponent() ? GetCameraComponent()->FieldOfView : NewFov;

	//	늦게 접속한 클라는 이미 지난 만큼을 건너뛰고 시작한다.
	ShotElapsed = FMath::Max(0.f, ElapsedSeconds);
	bShotActive = true;

	if (Shot.BlendSeconds <= 0.f || ShotElapsed >= Shot.BlendSeconds)
	{
		SnapToShot(Shot);
	}
}

void ABGCinematicCamera::SnapToShot(const FBGCameraShot& Shot)
{
	FTransform NewTarget;
	float NewFov = 90.f;
	if (!ResolveShotPov(Shot, NewTarget, NewFov))
	{
		return;
	}

	CurrentShot = Shot;
	SetActorTransform(NewTarget);

	if (UCameraComponent* Cam = GetCameraComponent())
	{
		Cam->SetFieldOfView(NewFov);
	}

	//	도착했으므로 더 보간하지 않는다. 계속 틱을 돌면 목표가 움직일 때(캐릭터가 걸어갈 때)
	//	따라가지 못하고 제자리에 남는데, 그건 아래 Tick 의 목표 재계산이 맡는다.
	bShotActive     = false;
	StartTransform  = NewTarget;
	StartFov        = NewFov;
	TargetTransform = NewTarget;
	TargetFov       = NewFov;
}

FVector ABGCinematicCamera::OrbitLerp(const FVector& Start, const FVector& End, float Alpha) const
{
	//	선회는 수평면에서만 돈다. 높이는 따로 보간해야 위아래로 출렁이지 않는다.
	const FVector StartOffset = Start - LookAtLocation;
	const FVector EndOffset   = End   - LookAtLocation;

	const FVector2D StartFlat(StartOffset.X, StartOffset.Y);
	const FVector2D EndFlat(EndOffset.X, EndOffset.Y);

	const float StartRadius = StartFlat.Size();
	const float EndRadius   = EndFlat.Size();

	//	중심에 너무 가까우면 방향이 정의되지 않는다 — 각도를 돌릴 수 없으므로 직선으로 뺀다.
	constexpr float MinRadius = 1.f;
	if (StartRadius < MinRadius || EndRadius < MinRadius)
	{
		return FMath::Lerp(Start, End, Alpha);
	}

	//	각도를 짧은 쪽으로 돈다. 그냥 보간하면 359°→1° 가 358° 를 도는 길로 간다.
	const float StartAngle = FMath::Atan2(StartFlat.Y, StartFlat.X);
	const float EndAngle   = FMath::Atan2(EndFlat.Y, EndFlat.X);

	float DeltaAngle = FMath::UnwindRadians(EndAngle - StartAngle);
	const float Angle  = StartAngle + DeltaAngle * Alpha;
	const float Radius = FMath::Lerp(StartRadius, EndRadius, Alpha);
	const float Height = FMath::Lerp(StartOffset.Z, EndOffset.Z, Alpha);

	return LookAtLocation + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Height);
}

void ABGCinematicCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!CurrentShot.IsActive())
	{
		return;
	}

	//	목표를 매 프레임 다시 읽는다 — 대상 캐릭터가 움직이면 샷도 따라가야 한다.
	FTransform LiveTarget;
	float LiveFov = TargetFov;
	if (ResolveShotPov(CurrentShot, LiveTarget, LiveFov))
	{
		TargetTransform = LiveTarget;
		TargetFov       = LiveFov;
	}

	if (!bShotActive)
	{
		//	도착 상태 — 목표에 붙어서 따라다닌다.
		SetActorTransform(TargetTransform);
		if (UCameraComponent* Cam = GetCameraComponent())
		{
			Cam->SetFieldOfView(TargetFov);
		}
		return;
	}

	ShotElapsed += DeltaSeconds;

	const float Duration = FMath::Max(KINDA_SMALL_NUMBER, CurrentShot.BlendSeconds);
	const float RawAlpha = FMath::Clamp(ShotElapsed / Duration, 0.f, 1.f);
	const float Alpha    = FMath::InterpEaseInOut(0.f, 1.f, RawAlpha, EaseExponent);

	//	위치 — 선회면 테이블을 감아 돌고, 직선이면 그냥 lerp.
	const FVector NewLocation = (CurrentShot.Blend == EBGShotBlend::Orbit)
		? OrbitLerp(StartTransform.GetLocation(), TargetTransform.GetLocation(), Alpha)
		: FMath::Lerp(StartTransform.GetLocation(), TargetTransform.GetLocation(), Alpha);

	//	회전은 항상 두 샷 사이를 slerp 한다. 이동 중 LookAtLocation 을 바라보게 하지 않는 이유:
	//	도착 순간의 회전이 목표 B 카메라와 어긋나 화면이 한 번 튄다.
	//	두 B 카메라가 모두 테이블을 향해 있으면 slerp 만으로도 구도가 유지된다.
	const FQuat NewRotation = FQuat::Slerp(
		StartTransform.GetRotation(), TargetTransform.GetRotation(), Alpha).GetNormalized();

	SetActorLocationAndRotation(NewLocation, NewRotation);

	if (UCameraComponent* Cam = GetCameraComponent())
	{
		Cam->SetFieldOfView(FMath::Lerp(StartFov, TargetFov, Alpha));
	}

	if (RawAlpha >= 1.f)
	{
		bShotActive = false;
	}
}
