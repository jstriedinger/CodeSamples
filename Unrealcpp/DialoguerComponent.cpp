// Fill out your copyright notice in the Description page of Project Settings.


#include "GameSystems/DialogueSystem/DialoguerComponent.h"

#include "BrainComponent.h"
#include "AI/FormationWaypoint.h"
#include "AI/SisterAIController.h"
#include "GameSystems/DialogueSystem/DialogueShardUserWidget.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameSystems/SorelleGameInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "Player/SorelleSister.h"


// Sets default values for this component's properties
UDialoguerComponent::UDialoguerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	bShouldBlockInput = false;
	bPlayerReadDialogue = false;
	bForceDialogue = true;
	//get our BP NPCUI dialogue widget
	static ConstructorHelpers::FClassFinder<UDialogueShardUserWidget> NPCHeadWidget(TEXT("/Game/GameSystems/Dialogue/W_NPCHead")); 
	if(NPCHeadWidget.Class != nullptr)
	{
		UIWidgetClass = NPCHeadWidget.Class;
	}

}

void UDialoguerComponent::BeforeStartConversation()
{

	//rotate player and NPC to face each other
	FVector OwnerLocation = GetOwner()->GetActorLocation();
	FRotator OwnerRotation = GetOwner()->GetActorRotation();
	if(ASorelleSister* PlayerChar = Cast<ASorelleSister>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		
		FVector        PlayerLocation =  PlayerChar->GetActorLocation();
		FRotator PlayerRotation = PlayerChar->GetActorRotation();
		FRotator newAngle = UKismetMathLibrary::FindLookAtRotation(OwnerLocation, PlayerLocation);
		OwnerRotation.Yaw = newAngle.Yaw;
		GetOwner()->SetActorRotation(OwnerRotation);

		//now change the player rotation to the opposite
		newAngle = UKismetMathLibrary::FindLookAtRotation(PlayerLocation, OwnerLocation);
		PlayerRotation.Yaw = newAngle.Yaw;
		PlayerChar->SetActorRotation(PlayerRotation);

		//Now lets double check the distance between them. If Yaw is less than X, move it a little bit
		//So players are on frame

		//if there is camera movement
		if(bChangeCamera)
		{
			float d = (PlayerLocation - OwnerLocation).Length();
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,   FString::Printf(TEXT("Distance between: %f"),d));
			if(d > 190)
			{
				//change player location
				FVector newLocation = OwnerLocation + (GetOwner()->GetActorForwardVector()*180);
				PlayerChar->SetActorLocation(newLocation);
			}
			PlayerChar->SetupSisterForDialogue();
			
		}
		ReceiveBeforeStartConversation();
	}
	
	
}

void UDialoguerComponent::AfterEndConversation()
{
	
	if(bChangeCamera)
	{
		if(ASorelleSister* PlayerChar = Cast<ASorelleSister>(UGameplayStatics::GetPlayerCharacter(this, 0)))
		{
			PlayerChar->ResumeSisterAIControl();
		}
		
	}
	ReceiveAfterEndConversation();
}


// Called when the game starts
void UDialoguerComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UDialoguerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

