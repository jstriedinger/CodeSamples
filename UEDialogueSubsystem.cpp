//explanation: this is a code sample of a Dialogue subsystem in UE5
// created for a SUC Games AGP project
#include "GameSystems/DialogueSystem/DialogueSubsystem.h"

#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SorelleSister.h"
#include "UObject/ConstructorHelpers.h"
#include "InputMappingContext.h"
#include "GameSystems/SorelleGameInstance.h"
#include "GameSystems/MusicSystem/HarmonizedAkComponent.h"
#include "Wwise/API/WwiseSoundEngineAPI.h"

UDialogueSubsystem::UDialogueSubsystem()
{
	static ConstructorHelpers::FClassFinder<UDialogueUserWidget> NPCWidget(TEXT("/Game/GameSystems/Dialogue/W_NPCDialogueBox")); 
	if(NPCWidget.Class != nullptr)
	{
		NPCUIDialogueWidgetClass = NPCWidget.Class;
	}

	static ConstructorHelpers::FClassFinder<UDialogueUserWidget> CalloutWidget(TEXT("/Game/GameSystems/Dialogue/W_CalloutDialogueBox")); 
	if(CalloutWidget.Class != nullptr)
	{
		CalloutsUIDialogueWidgetClass = CalloutWidget.Class;
	}

	DialogueOnlyMappingContext = LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/GameSystems/Input/IMC_Dialogue"));
}

void UDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	callbackBind.BindUFunction(this,TEXT("VOLineEnd"));
}


void UDialogueSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDialogueSubsystem::StartConversation(FDataTableCategoryHandle dialogues, ConversationType convoType, bool BlockInput = false)
{
	if(!bIsPlayingDialogue)
	{
		//get current dialogue tree
		dialogues.GetRows(currentDialogueTree,"");

		currentConversationType = convoType;

		if(currentDialogueTree.Num() > 0 )
		{
			FDialogueStruct* firstOne = currentDialogueTree[0];
			currentDialogueIndex = 0;
			bIsInputBlocked = BlockInput;
			bIsPlayingDialogue = true;
			
			if(currentConversationType == ConversationType::NPC)
				UIDialogueBox = CreateWidget<UDialogueUserWidget>(GetGameInstance(),NPCUIDialogueWidgetClass);
			else
			{
				UIDialogueBox = CreateWidget<UDialogueUserWidget>(GetGameInstance(),CalloutsUIDialogueWidgetClass);
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

			//setup corresponding dialoguebox
			UIDialogueBox->AddToViewport();
			AkComponent = PlayerChar->GetAkComponent();
			ShowDialogueUI(firstOne->actor, firstOne->txt);
			PostVOEvent(firstOne->voId);
		}

	}
	
}
void UDialogueSubsystem::EndConversation()
{
	
	UIDialogueBox->RemoveFromParent();
	bIsPlayingDialogue = false;
	
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
	}
}


void UDialogueSubsystem::AdvanceDialogue(bool CheckIfTyping = false)
{
	if(CheckIfTyping && UIDialogueBox->bIsTyping)
	{
		UIDialogueBox->StopTypingEffectAndFill();
		return;
	}
	currentDialogueIndex++;
	if(currentDialogueIndex < currentDialogueTree.Num())
	{
		FDialogueStruct* nextOne =  currentDialogueTree[currentDialogueIndex];
		ShowDialogueUI(nextOne->actor, nextOne->txt);
		PostVOEvent(nextOne->voId);
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

void UDialogueSubsystem::VOLineEnd()
{
	if(!bIsInputBlocked)
		AdvanceDialogue(false);
}

void UDialogueSubsystem::PostVOEvent(FText EventName)
{
	if(USorelleGameInstance* SorelleGameInstance = Cast<USorelleGameInstance>(GetGameInstance()))
	{
		FString EventProcessed = FString("Play_")+EventName.ToString();

		UAkAudioEvent* dialogueEvent = SorelleGameInstance->GetDialogueLine(EventProcessed);

		AkPlayingID result = AK_INVALID_PLAYING_ID;
		
		if(dialogueEvent)
			result = AkComponent->PostAkEvent(dialogueEvent,AK_EndOfEvent,callbackBind,"");

		if(result == AK_INVALID_PLAYING_ID)
		{
			AkPlayingID fallbackResult = AkComponent->PostAkEvent(SorelleGameInstance->GetDialogueFallback(),AK_EndOfEvent,callbackBind,"");
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