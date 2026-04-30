// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/Bella.h"

#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameSystems/SorelleMode.h"
#include "GameSystems/MusicSystem/HarmonizedAkComponent.h"
#include "GameSystems/MusicSystem/MusicSubsystem.h"
#include "Interactables/Interactable.h"

// Sets default values
ABella::ABella()
{
	GPCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("GroundPound Collider"));
	GPCollider->SetupAttachment(RootComponent);
	GPCollider->SetBoxExtent(FVector(24,24,40),false);
	GPCollider->SetGenerateOverlapEvents(true);
	GPCollider->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	GPCollider->SetRelativeLocation({0,0,-70});
	
	GPFloatingTime = 0.2f;
	GPFallingSpeed = 1000;
	DoubleJumpSpeed = 600;
	bUseGPCameraCorrection = true;
	CharacterTag = FGameplayTag::RequestGameplayTag("Character.Bella");
	LookDownTimer = 0.0f;
	DefaultLookDownTime = 1.0f;
	bCanDoubleJump = true;
	bIsGroundPounding = false;
}

void ABella::OnGroundPoundBeginFalling()
{
	//GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("groundpound: falling"));	
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		EnableInput(PlayerController);
	}

	//collider channels edits
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel2, ECollisionResponse::ECR_Ignore);
	GPCollider->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel2, ECollisionResponse::ECR_Overlap);
	
	UCharacterMovementComponent* charMovement = GetCharacterMovement();
	charMovement->GravityScale = GPGravityBefore;
	const FVector GPFallSpeed {0.0f, 0.0f, -GPFallingSpeed};
	LaunchCharacter(GPFallSpeed, false, true);
}

void ABella::Jump()
{
	Super::Jump();
	//Overriding for Bella double jump check
	UCharacterMovementComponent* charMovement = GetCharacterMovement();
	if( charMovement->IsFalling() && bCanDoubleJump && !bIsGroundPounding)
	{
		const FVector VDoubleJumpSpeed {0.0f, 0.0f, DoubleJumpSpeed};
		LaunchCharacter(VDoubleJumpSpeed, false, true);
		bCanDoubleJump = false;

		OnDoubleJump.Broadcast();
		
		// Temp (for Samhi) Play launch sound here instead
		GetAkComponent()->PostAkEventOnRoot(PoundLaunch);
		if(UDialogueSubsystem* DialogueSubsystem = GetGameInstance()->GetSubsystem<UDialogueSubsystem>())
		{
			DialogueSubsystem->PlayGameplayConversation(GameplayConversationType::Jump);
		}

		// playing jump boost animation
		float animTime=.1f;
		if(JumpBoostMontage != nullptr)
		{
			UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				animTime = AnimInstance->Montage_Play(JumpBoostMontage, 1.f);
				if (animTime < 0.1f)
				{
					//The function returns 0 if the animation can't be played.
					animTime = 0.1f;
				}
			}
		}
		if(animTime == .1f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Call animation could not be played"));
		}
	}
	
}

void ABella::DoGroundPound()
{
	//detect if in fact is in midair
	UCharacterMovementComponent* charMovement = GetCharacterMovement();
	if(charMovement->IsFalling() && bCanUseAbility)
	{
		//First stage: freezing up in the air
		bCanUseAbility = false;
		bIsGroundPounding = true;
		GPGravityBefore = charMovement->GravityScale;
		GPBeforeAirControl = charMovement->AirControl;
	
		//best practice erasing timer before creating one
		GetWorld()->GetTimerManager().ClearTimer(GroundPoundTimerHandle);

		//Do not let player move while freezing
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			DisableInput(PlayerController);
		}
		charMovement->StopMovementImmediately();
		
		//increase aircontrol while falling in the air
		charMovement->AirControl = 1;
		charMovement->GravityScale = 0;

		//here it will make an animation
		GetWorld()->GetTimerManager().SetTimer(GroundPoundTimerHandle, this, &ABella::OnGroundPoundBeginFalling, GPFloatingTime, false);
		UE_LOG(LogTemp, Warning, TEXT("Attempting to play ability animation"));

		// playing ground pound animation
		float animTime=.1f;
		if(AbilityMontage != nullptr)
		{
			UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				animTime = AnimInstance->Montage_Play(AbilityMontage, 1.f);
				if (animTime < 0.1f)
				{
					//The function returns 0 if the animation can't be played.
					animTime = 0.1f;
				}
			}
		}
		if(animTime == .1f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Ability animation could not be played"));
		}

		// Play flip sound
		if(UMusicSubsystem* MusicSubsystem = GetGameInstance()->GetSubsystem<UMusicSubsystem>())
		{
			float Shift = (4.0f/6.0f) * MusicSubsystem->GetTempo()/175.0f;
			if(Shift < .5)
				Shift *= 2;
			//GetAkComponent()->SetRTPCValue(TempoShift,Shift,0.0f,"");
			FAkAudioDevice::Get()->SetRTPCValue(TempoShift,Shift,0.0f,NULL);
		}
		GetAkComponent()->PostAkEventOnRoot(PoundFlip);
		if(UDialogueSubsystem* DialogueSubsystem = GetGameInstance()->GetSubsystem<UDialogueSubsystem>())
		{
			DialogueSubsystem->PlayGameplayConversation(GameplayConversationType::Flip);
		}
		
	}
}

void ABella::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if(bIsGroundPounding)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			DisableInput(PlayerController);
			GetWorld()->GetTimerManager().ClearTimer(GroundPoundTimerHandle);
			GetWorld()->GetTimerManager().SetTimer(GroundPoundTimerHandle, this, &ABella::OnGroundPoundLanded, 0.4f, false);

		}
		//return to default aircontrol
		GetCharacterMovement()->AirControl = GPBeforeAirControl;
		// Breakable box collissions back to normal
		GPCollider->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel2, ECollisionResponse::ECR_Ignore);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel2, ECollisionResponse::ECR_Block);
		
		// Play land sound
		GetAkComponent()->PostAkEventOnRoot(PoundLand);
	}

	// hit a trampoline during groundPounding
	if (bIsGroundPounding && Hit.GetActor()->IsA<AInteractable>() && Cast<AInteractable>(Hit.GetActor())->HasInteractableTag("Trampoline"))
	{
		bIsTrampolineJumping = true;
		GetAkComponent()->PostAkEventOnRoot(PoundBounce);
		OnTrampolineBounce.Broadcast();
	}
	else if (bIsTrampolineJumping)
	{
		// continue trampolineJumping
		if (Hit.GetActor()->IsA<AInteractable>() && Cast<AInteractable>(Hit.GetActor())->HasInteractableTag("Trampoline"))
		{
			// play sound
			GetAkComponent()->PostAkEventOnRoot(PoundBounce);
		}
		// trampolineJumping stopping condition
		else
		{
			bIsTrampolineJumping = false;
		}
	}

	bIsGroundPounding = false;
	bCanUseAbility = true;
	bCanDoubleJump = true;
}

void ABella::Move(const FInputActionValue& Value)
{
	Super::Move(Value);
}

void ABella::OnGroundPoundLanded()
{
	
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		EnableInput(PlayerController);
	}
}

void ABella::AIStay(const FInputActionValue& Value)
{
	Super::AIStay(Value);
	if(!canMove)
	{
		if(UDialogueSubsystem* DialogueSubsystem = GetGameInstance()->GetSubsystem<UDialogueSubsystem>())
		{
			DialogueSubsystem->PlayGameplayConversation(GameplayConversationType::StayBella);
		}
	}
}

void ABella::Look(const FInputActionValue& Value)
{
	Super::Look(Value);
}

void ABella::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	/*if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(AbilityMidairAction, ETriggerEvent::Triggered, this, &ABella::UseGPFromMidAir);
	}*/
	
}

void ABella::BeginPlay()
{
	Super::BeginPlay();
}

void ABella::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (LookDownTimer > 0.0f)
	{
		LookDown(DeltaSeconds);
	}
}

void ABella::LookDown(float DeltaSeconds)
{
	LookDownTimer -= DeltaSeconds;

	if (Controller != nullptr && bUseGPCameraCorrection)
	{
		FRotator CurrentRotation = GetControlRotation();
		float	 CurrentPitchRaw = FRotator::NormalizeAxis(CurrentRotation.Pitch);
		if (CurrentPitchRaw > -40.0f)
		{
			//auto	 alpha = (DefaultLookDownTime - LookDownTimer) / DefaultLookDownTime;
			float	 IdealPitch = -40.0f;
			float LerpRate = 0.05f;
			auto	 TargetPitch = FMath::Lerp(CurrentPitchRaw, IdealPitch, LerpRate);
			auto	 PitchToAdd = abs(abs(TargetPitch) - abs(CurrentPitchRaw));
			AddControllerPitchInput(PitchToAdd);
		}
		
	}
}