// Fill out your copyright notice in the Description page of Project Settings.


#include "GameSystems/DialogueSystem/DialogueSubsystem.h"

#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SorelleSister.h"
#include "UObject/ConstructorHelpers.h"
#include "InputMappingContext.h"
#include "GameSystems/SorelleGameInstance.h"
#include "GameSystems/SorelleMode.h"
#include "GameSystems/MusicSystem/HarmonizedAkComponent.h"
#include "Player/SisterController.h"
#include "Wwise/API/WwiseSoundEngineAPI.h"
#include "Cinematics/SisterPuppet.h"
#include "GameSystems/DialogueSystem/NPCTalker.h"
#include "UI/SorelleBaseHUD.h"
#include "Player/SisterController.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "UMG/Public/Blueprint/WidgetTree.h"

UDialogueSubsystem::UDialogueSubsystem()
{
	//get our BP NPCUI dialogue widget
	static ConstructorHelpers::FClassFinder<UDialogueUserWidget> NPCWidget(TEXT("/Game/GameSystems/Dialogue/W_NPCDialogueBox")); 
	if(NPCWidget.Class != nullptr)
	{
		NPCUIDialogueWidgetClass = NPCWidget.Class;
	}

	//get our BP CalloutUI dialogue widget
	static ConstructorHelpers::FClassFinder<UDialogueUserWidget> CalloutWidget(TEXT("/Game/GameSystems/Dialogue/W_CalloutDialogueBox")); 
	if(CalloutWidget.Class != nullptr)
	{
		CalloutsUIDialogueWidgetClass = CalloutWidget.Class;
	}
	//get our additional mapping context from UE
	DialogueOnlyMappingContext = LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/GameSystems/Input/IMC_Dialogue"));
	bIsPlayingDialogue = false;
	currentConversationIndex = 0;
	
}

void UDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	//callbackBind.BindUFunction(this,TEXT("VOCallback"));
	DialogueEndDelegate.BindUFunction(this,TEXT("AutoAdvanceVO"));

	// abilities
	ConversationChances.FindOrAdd(GameplayConversationType::LightRay,0.2f);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_01"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_02"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_03"),3);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_04"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_05"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_06"),3);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_07"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_08"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_09"),5);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_10"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::LightRay).Add(TEXT("Capri_LightRay_11"),1);

	ConversationChances.FindOrAdd(GameplayConversationType::Levitate,0.2f);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_01"),4);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_02"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_03"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_04"),4);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_05"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_06"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_07"),4);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_08"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_09"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_10"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Levitate).Add(TEXT("Alessandra_Levitate_11"),1);

	ConversationChances.FindOrAdd(GameplayConversationType::Jump,0.1f);
	GameplayConversations.FindOrAdd(GameplayConversationType::Jump).Add(TEXT("Bella_JumpBoost_01"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Jump).Add(TEXT("Bella_JumpBoost_02"),5);
	GameplayConversations.FindOrAdd(GameplayConversationType::Jump).Add(TEXT("Bella_JumpBoost_03"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Jump).Add(TEXT("Bella_JumpBoost_04"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Jump).Add(TEXT("Bella_JumpBoost_05"),5);
	GameplayConversations.FindOrAdd(GameplayConversationType::Jump).Add(TEXT("Bella_JumpBoost_06"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Jump).Add(TEXT("Bella_JumpBoost_07"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Jump).Add(TEXT("Bella_JumpBoost_08"),5);
	GameplayConversations.FindOrAdd(GameplayConversationType::Jump).Add(TEXT("Bella_JumpBoost_09"),1);
	
	ConversationChances.FindOrAdd(GameplayConversationType::Smash,0.2f);
	GameplayConversations.FindOrAdd(GameplayConversationType::Smash).Add(TEXT("Bella_GroundPound_01"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Smash).Add(TEXT("Bella_GroundPound_03"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Smash).Add(TEXT("Bella_GroundPound_04"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Smash).Add(TEXT("Bella_GroundPound_06"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Smash).Add(TEXT("Bella_GroundPound_01"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Smash).Add(TEXT("Bella_GroundPound_09"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Smash).Add(TEXT("Bella_GroundPound_10"),1);

	ConversationChances.FindOrAdd(GameplayConversationType::Flip,0.2f);
	GameplayConversations.FindOrAdd(GameplayConversationType::Flip).Add(TEXT("Bella_GroundPound_02"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Flip).Add(TEXT("Bella_GroundPound_08"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::Flip).Add(TEXT("Bella_GroundPound_05"),1);

	
	// switches
	
	ConversationChances.FindOrAdd(GameplayConversationType::SwitchAleToCapri,0.2f);
	GameplayConversations.FindOrAdd(GameplayConversationType::SwitchAleToCapri).Add(TEXT("AlessandraSwitch_01"),1);
	ConversationChances.FindOrAdd(GameplayConversationType::SwitchAleToBella,0.2f);
	GameplayConversations.FindOrAdd(GameplayConversationType::SwitchAleToBella).Add(TEXT("AlessandraSwitch_02"),1);
	
	ConversationChances.FindOrAdd(GameplayConversationType::SwitchBellaToAle,0.2f);
	GameplayConversations.FindOrAdd(GameplayConversationType::SwitchBellaToAle).Add(TEXT("BellaSwitch_01"),1);
	ConversationChances.FindOrAdd(GameplayConversationType::SwitchBellaToCapri,0.2f);
	GameplayConversations.FindOrAdd(GameplayConversationType::SwitchBellaToCapri).Add(TEXT("BellaSwitch_02"),1);
	
	ConversationChances.FindOrAdd(GameplayConversationType::SwitchCapriToAle,0.2f);
	GameplayConversations.FindOrAdd(GameplayConversationType::SwitchCapriToAle).Add(TEXT("CapriSwitch_02"),1);
	ConversationChances.FindOrAdd(GameplayConversationType::SwitchCapriToBella,0.2f);
	GameplayConversations.FindOrAdd(GameplayConversationType::SwitchCapriToBella).Add(TEXT("CapriSwitch_01"),1);

	// stays
	ConversationChances.FindOrAdd(GameplayConversationType::StayAle,0.5f);
	GameplayConversations.FindOrAdd(GameplayConversationType::StayAle).Add(TEXT("AlessandraStay_01"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::StayAle).Add(TEXT("AlessandraStay_02"),1);
	ConversationChances.FindOrAdd(GameplayConversationType::StayBella,0.5f);
	GameplayConversations.FindOrAdd(GameplayConversationType::StayBella).Add(TEXT("Bella_Stay_01"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::StayBella).Add(TEXT("Bella_Stay_02"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::StayBella).Add(TEXT("Bella_Stay_03"),1);
	ConversationChances.FindOrAdd(GameplayConversationType::StayCapri,0.5f);
	GameplayConversations.FindOrAdd(GameplayConversationType::StayCapri).Add(TEXT("Capri_Stay_01"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::StayCapri).Add(TEXT("Capri_Stay_02"),1);
	GameplayConversations.FindOrAdd(GameplayConversationType::StayCapri).Add(TEXT("Capri_Stay_03"),1);
}


void UDialogueSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDialogueSubsystem::StartConversation(UDataTable* DataTable, FText ConversationId, ConversationType convoType, bool BlockInput = false, TArray<AActor*> InAdditionalActors = TArray<AActor*>(), bool forceDialogue )
{
	if(bIsPlayingDialogue && forceDialogue)
		EndConversation();
	if(!bIsPlayingDialogue)
	{
		GetConversationFromTable(DataTable, ConversationId);
		//set convo type
		currentConversationType = convoType;
		CurrentConversationName = ConversationId;

		//vlaidate and get the first one
		if(currentConversation.Num() > 0 )
		{
			FDialogueStruct* firstOne = currentConversation[0];
			currentConversationIndex = 0;
			bIsInputBlocked = BlockInput;
			bIsPlayingDialogue = true;

			if(ASisterController* SisterController = Cast<ASisterController>(UGameplayStatics::GetPlayerController(this, 0)))
			{
				if(currentConversationType == ConversationType::NPC)
				{
					//UIDialogueBox = CreateWidget<UDialogueUserWidget>(GetGameInstance(),NPCUIDialogueWidgetClass);
					UIDialogueBox = SisterController->BaseHUD->WBP_NPCUI;
				}
				else
				{
					//UIDialogueBox = CreateWidget<UDialogueUserWidget>(GetGameInstance(),CalloutsUIDialogueWidgetClass);
					UIDialogueBox = SisterController->BaseHUD->WBP_CalloutUI;
				}
			}

			const ASorelleSister* PlayerChar = Cast<ASorelleSister>(UGameplayStatics::GetPlayerCharacter(this, 0));
			if(bIsInputBlocked)
			{
				const APlayerController* playerController = UGameplayStatics::GetPlayerController(this, 0);
				if (UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer()))
				{

					UInputMappingContext* InputMappingContext = PlayerChar->GetDefaultInputMappingContext();
					EnhancedInputSubsystem->RemoveMappingContext(InputMappingContext);
					EnhancedInputSubsystem->AddMappingContext(DialogueOnlyMappingContext,0);
				}
			}
			
			//UIDialogueBox->AddToViewport();
			if(UIDialogueBox)
				UIDialogueBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			
			//AkComponent = PlayerChar->GetAkComponent();
			ShowDialogueUI(firstOne->actor, firstOne->txt);
			PostVOEvent(firstOne->actor,firstOne->voId);
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("There are no dialogues with that conversationId"));
			}
		}

	}
	
}
void UDialogueSubsystem::EndConversation()
{
	ResetSystem();
	if(bIsInputBlocked)
	{
		ASorelleSister* PlayerChar = Cast<ASorelleSister>(UGameplayStatics::GetPlayerCharacter(this, 0));
		APlayerController* playerController = UGameplayStatics::GetPlayerController(this, 0);
		if (UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer()))
		{

			UInputMappingContext* InputMappingContext = PlayerChar->GetDefaultInputMappingContext();
			EnhancedInputSubsystem->RemoveMappingContext(DialogueOnlyMappingContext);
			EnhancedInputSubsystem->AddMappingContext(InputMappingContext,0);
		}
		
		bIsInputBlocked = false;
		
		OnEndConversation.Broadcast(PlayerChar->NPCWithDialogue);
	}
}


void UDialogueSubsystem::AdvanceDialogue(bool CheckIfTyping = false)
{
	if(UIDialogueBox && CheckIfTyping && UIDialogueBox->bIsTyping)
	{
		UIDialogueBox->StopTypingEffectAndFill();
		return;
	}
	currentConversationIndex++;
	if(currentConversationIndex < currentConversation.Num())
	{
		FDialogueStruct* nextOne =  currentConversation[currentConversationIndex];

		
		ShowDialogueUI(nextOne->actor, nextOne->txt);
		PostVOEvent(nextOne->actor,nextOne->voId);
	}
	else
	{
		EndConversation();
	}
	
}
void UDialogueSubsystem::ShowDialogueUI(FText actor, FText txt)
{
	if(UIDialogueBox)
	{
		UIDialogueBox->StartTypingEffect(actor, txt);
	}
}

void UDialogueSubsystem::AddAdditionalActor(FString Actor, FString Conversation, class UAkComponent* InAkComponent)
{
	AdditionalActorsMap.FindOrAdd(Conversation).FindOrAdd(Actor) = InAkComponent;
}

void UDialogueSubsystem::PlayGameplayConversation(GameplayConversationType ConvoType)
{
	if(!bIsPlayingDialogue && bCalloutsEnabled)
	{
		if(GetWorld()->GetTimerManager().IsTimerActive(CalloutCooldownTimerHandle))
			return;
		if(ASorelleMode* SorelleGameMode = Cast<ASorelleMode>(GetWorld()->GetAuthGameMode()))
		{
			float* ChanceToPlay = ConversationChances.Find(ConvoType);
			const float RandomToPlay = FMath::RandRange(0.0f,1.0f);
			if(*ChanceToPlay && RandomToPlay <= *ChanceToPlay)
			{
				TMap<FString,int>* Convos = GameplayConversations.Find(ConvoType);
				if(Convos)
				{
					FString ConvoToPlay;
					int WeightTotal = 0;
					for(auto SpecificConvo : *Convos)
					{
						WeightTotal += SpecificConvo.Value;
					}
					const float Random = FMath::RandRange(0,WeightTotal);
					int i = 0;
					for(auto SpecificConvo : *Convos)
					{
						i += SpecificConvo.Value;
						if(i >= Random)
						{
							ConvoToPlay = SpecificConvo.Key;
							break;
						}
					}
					StartConversation(SorelleGameMode->ConversationDataTable.Get(),FText::FromString(ConvoToPlay),ConversationType::Callout,false,TArray<AActor*>(),false);
					GetWorld()->GetTimerManager().SetTimer(CalloutCooldownTimerHandle,CalloutCooldownTime,false);
				}
			}
		}
	}
	
}

void UDialogueSubsystem::GetConversationFromTable(UDataTable* DataTable, FText ConversationId)
{
	TArray<FDialogueStruct*> TableRows;
	currentConversation.Empty();
	DataTable->GetAllRows<FDialogueStruct>("",TableRows);
	for (FDialogueStruct* tableRow : TableRows)
	{
		if(tableRow->conversationId.CompareTo(ConversationId) == 0)
		{
			currentConversation.Add(tableRow);
		}
	}

}

void UDialogueSubsystem::VOCallback(EAkCallbackType CallbackType, UAkCallbackInfo* CallbackInfo)
{
	/*if(CallbackType == EAkCallbackType::Duration)
	{
		const UAkDurationCallbackInfo* DurationCallbackInfo = Cast<UAkDurationCallbackInfo>(CallbackInfo);
		const float                    Duration = DurationCallbackInfo->EstimatedDuration / 1000.0f;
		GetWorld()->GetTimerManager().SetTimer(DialogueTimerHandle,DialogueEndDelegate,Duration,false);
	}*/
}

void UDialogueSubsystem::ClearVO()
{
	GetWorld()->GetTimerManager().ClearTimer(DialogueTimerHandle);
	if(USorelleGameInstance* SorelleGameInstance = Cast<USorelleGameInstance>(GetGameInstance()))
	{
		if(AkComponent)
		{
			AkComponent->PostAkEvent(SorelleGameInstance->GetDialogueStop());
			StopYaps();
		}
	}
	AkComponent = nullptr;
}

void UDialogueSubsystem::StopYaps()
{
	if(bSisterMaximumYap)
	{
		if(ASorelleMode* SorelleGameMode = Cast<ASorelleMode>(GetWorld()->GetAuthGameMode()))
		{
			ASorelleSister* A = Cast<ASorelleSister>(SorelleGameMode->Alessandra);
			ASorelleSister* B = Cast<ASorelleSister>(SorelleGameMode->Bella);
			ASorelleSister* C = Cast<ASorelleSister>(SorelleGameMode->Capri);
			TArray<ASorelleSister*> Sisters = TArray<ASorelleSister*>();
			A->bIsYapping = false;
			B->bIsYapping = false;
			C->bIsYapping = false;
		}
	}
	else
	{
		if(ASorelleSister* SorelleSister = Cast<ASorelleSister>(AkComponent->GetOwner()))
		{
			SorelleSister->bIsYapping = false;
		}
		else if(ASisterPuppet* SisterPuppet = Cast<ASisterPuppet>(AkComponent->GetOwner()))
		{
			SisterPuppet->bIsYapping = false;
		}
		else if(ANPCTalker* NPCTalker = Cast<ANPCTalker>(AkComponent->GetOwner()))
		{
			NPCTalker->bIsYapping = false;
		}
	}
	AkComponent = nullptr;
}

void UDialogueSubsystem::AutoAdvanceVO()
{
	StopYaps();
	if(currentConversationType == ConversationType::Callout && bIsPlayingDialogue)
		AdvanceDialogue(false);
}

/* Reset the system. Used when we are going back to main menu. Or if any other reason */
void UDialogueSubsystem::ResetSystem()
{
	bIsPlayingDialogue = false;
	currentConversationIndex = 0;

	ClearVO();
	
	
	if(UIDialogueBox)
	{
		//UIDialogueBox->RemoveFromParent();
		UIDialogueBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Resetting dialogue system"));
	}
}

void UDialogueSubsystem::PostVOEvent(FText ActorName, FText EventName)
{
	ClearVO();
	if(ASorelleSister* PlayerChar = Cast<ASorelleSister>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		AkComponent = PlayerChar->GetAkComponent();
	}
	
	if(USorelleGameInstance* SorelleGameInstance = Cast<USorelleGameInstance>(GetGameInstance()))
	{

		FString EventProcessed = FString("Play_")+EventName.ToString();

		UAkAudioEvent* dialogueEvent = SorelleGameInstance->GetDialogueLine(EventProcessed);

		AkPlayingID result = AK_INVALID_PLAYING_ID;

		const FString ActorString = ActorName.ToString();
		const FString ConvoString = CurrentConversationName.ToString();

		if(ASorelleMode* SorelleGameMode = Cast<ASorelleMode>(GetWorld()->GetAuthGameMode()))
		{

			if(UAkComponent** AdditionalActor = AdditionalActorsMap.FindOrAdd(ConvoString).Find(ActorString))
			{
				AkComponent = *AdditionalActor;
				if(AkComponent)
					if(ANPCTalker* NPCTalker = Cast<ANPCTalker>(AkComponent->GetOwner()))
					{
						NPCTalker->bIsYapping = true;
					}
				
				if (GEngine)
					GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, "Successfully yoinked AkComponent from speaking actor");
			}

			
			if(currentConversationType == ConversationType::Cutscene)
			{
				ASisterPuppet* PlayerChar = nullptr;
				if(ActorString.Equals(FString("Alessandra")))
				{
					if(SorelleGameMode->AlessandraPuppet)
						PlayerChar =  SorelleGameMode->AlessandraPuppet;
				}
				else if(ActorString.Equals(FString("Bella")))
				{
					if(SorelleGameMode->BellaPuppet)
						PlayerChar =  SorelleGameMode->BellaPuppet;
				}
				else if(ActorString.Equals(FString("Capri")))
				{
					if(SorelleGameMode->CapriPuppet)
						PlayerChar =  SorelleGameMode->CapriPuppet;
				}
				if(PlayerChar)
				{
					AkComponent = PlayerChar->GetAkComponent();
					PlayerChar->bIsYapping = true;
				}
			}
			else
			{
				ASorelleSister* PlayerChar = nullptr;
				if(ActorString.Equals(FString("Alessandra")))
				{
					PlayerChar =  Cast<ASorelleSister>(SorelleGameMode->Alessandra);
				}
				else if(ActorString.Equals(FString("Bella")))
				{
					PlayerChar =  Cast<ASorelleSister>(SorelleGameMode->Bella);
				}
				else if(ActorString.Equals(FString("Capri")))
				{
					PlayerChar =  Cast<ASorelleSister>(SorelleGameMode->Capri);
				}
				if(PlayerChar)
				{
					AkComponent = PlayerChar->GetAkComponent();
					float animTime=.1f;
					PlayerChar->bIsYapping = true;
				}
				else if(ActorString.Equals(FString("Sorelle")) && currentConversationType == ConversationType::NPC)
				{
					ASorelleSister* A = Cast<ASorelleSister>(SorelleGameMode->Alessandra);
					ASorelleSister* B = Cast<ASorelleSister>(SorelleGameMode->Bella);
					ASorelleSister* C = Cast<ASorelleSister>(SorelleGameMode->Capri);

					AkComponent = A->GetAkComponent();
					B->GetAkComponent()->PostAkEvent(dialogueEvent);
					C->GetAkComponent()->PostAkEvent(dialogueEvent);

					A->bIsYapping = true;
					B->bIsYapping = true;
					C->bIsYapping = true;
					
					bSisterMaximumYap = true;
				
				
				}
			}
		}
		
		if(dialogueEvent)
		{
			if(AkComponent)
				result = AkComponent->PostAkEvent(dialogueEvent,AK_Duration,callbackBind);
		}
			

		if(result == AK_INVALID_PLAYING_ID)
		{
			AkPlayingID fallbackResult = AkComponent->PostAkEvent(SorelleGameInstance->GetDialogueFallback(),AK_EndOfEvent,callbackBind);
			if(result == AK_INVALID_PLAYING_ID && !bIsInputBlocked)
			{
				AdvanceDialogue(false);
			}
		}
	}
	else
	{
		AdvanceDialogue(false);
	}
}
