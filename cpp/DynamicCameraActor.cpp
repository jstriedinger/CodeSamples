// this is an edited file from the DynamicCamera actor system on my project The Guardian
//This file is like that mainly for code sample purposes


#include "DynamicCameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Guardian/GuardianAI.h"
#include "Kismet/GameplayStatics.h"
#include "TheGuardianUE5/TheGuardianUE5GameMode.h"

ADynamicCameraActor::ADynamicCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	TargetsTracker = CreateDefaultSubobject<USphereComponent>(TEXT("DebugSphereComponent"));
	TargetsTracker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetsTracker->SetupAttachment(RootComponent);

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->TargetArmLength = 2000;
	SpringArmComponent->SetupAttachment(RootComponent);
	SpringArmComponent->bDoCollisionTest = false;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);
}

void ADynamicCameraActor::BeginPlay()
{
	Super::BeginPlay();

	DefaultArmAngles = SpringArmComponent->GetComponentRotation();
	DefaultMaxArmLength = MaxArmLength;
	DefaultMinArmLength = MinArmLength;

	SetupDynamicCamera();
}

void ADynamicCameraActor::SetupDynamicCamera()
{
	if (!bUseDynamicCamera)
		return;

	ViewTargets.Empty();

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!PlayerCharacter)
		return;

	ViewTargets.Add(PlayerCharacter);
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		GetRootComponent()->SetWorldLocation(PlayerCharacter->GetActorLocation());
		PlayerController->SetViewTargetWithBlend(this, CameraBlendTime, EViewTargetBlendFunction::VTBlend_Linear);
	}
}

void ADynamicCameraActor::AddViewTarget(AActor* ViewTarget)
{
	if (!ViewTargets.Contains(ViewTarget))
	{
		ViewTargets.Add(ViewTarget);
	}
}

void ADynamicCameraActor::RemoveViewTarget(AActor* ViewTarget)
{
	ViewTargets.Remove(ViewTarget);
}

void ADynamicCameraActor::ToggleGuardianOnTargetsArray(bool addGuardian)
{
	if (AGuardianAI* Guardian = FindGuardian())
	{
		if (addGuardian)
		{
			AddViewTarget(Guardian);
		}
		else
		{
			RemoveViewTarget(Guardian);
		}
	}
}


void ADynamicCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bCanFollow)
	{
		return;
	}

	if (ViewTargets.IsEmpty())
	{
		TargetsViewPoint = GetActorLocation();
		return;
	}

	HandleCameraTracking(DeltaTime);
}

void ADynamicCameraActor::HandleCameraTracking(float DeltaTime)
{
	// Always computed, the rail zone reads TargetsViewPoint even with position tracking off
	ComputeTargetsViewPoint();

	if (bCanTrackPositions)
	{
		SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetsViewPoint, DeltaTime, CamPositionSpeed));
	}

	if (bCanTrackDistance)
	{
		UpdateArmLength(DeltaTime);
	}
}

void ADynamicCameraActor::ComputeTargetsViewPoint()
{
	FVector WeightedSum = FVector::ZeroVector;
	float TotalWeight = 0.f;

	for (int32 Index = 0; Index < ViewTargets.Num(); ++Index)
	{
		const AActor* Target = ViewTargets[Index].Get();
		if (!IsValid(Target) || !Target->GetRootComponent())
		{
			continue;
		}

		const float Weight = (Index == 0) ? PlayerWeight : 1.0f;
		WeightedSum += Target->GetActorLocation() * Weight;
		TotalWeight += Weight;
	}

	TargetsViewPoint = (TotalWeight > 0.f) ? WeightedSum / TotalWeight : FVector::ZeroVector;
}

void ADynamicCameraActor::UpdateArmLength(float DeltaTime)
{
	float MaxDist = 0.f;
	for (const TWeakObjectPtr<AActor>& Target : ViewTargets)
	{
		if (Target.IsValid())
		{
			MaxDist = FMath::Max(MaxDist, FVector::Dist(Target->GetActorLocation(), TargetsViewPoint));
		}
	}

	const float HalfFOV = FMath::DegreesToRadians(CameraComponent->FieldOfView * 0.5f);
	const float Pitch = FMath::DegreesToRadians(FMath::Abs(SpringArmComponent->GetComponentRotation().Pitch));
	const float PitchSin = FMath::Max(FMath::Sin(Pitch), KINDA_SMALL_NUMBER);

	// Arm length that puts the farthest target at the edge of the view
	const float DesiredArmLength = FMath::Clamp(MaxDist / (FMath::Tan(HalfFOV) * PitchSin), MinArmLength, MaxArmLength);
	SpringArmComponent->TargetArmLength = FMath::FInterpTo(SpringArmComponent->TargetArmLength, DesiredArmLength, DeltaTime, CamSpringarmSpeed);
}
