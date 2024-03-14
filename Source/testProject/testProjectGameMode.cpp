// Copyright Epic Games, Inc. All Rights Reserved.

#include "testProjectGameMode.h"
#include "testProjectCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameStateBase.h"

AtestProjectGameMode::AtestProjectGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AHUD> PlayerHUDBPClass(TEXT("/Game/ThirdPerson/Blueprints/UI/testHUD"));
	if (PlayerHUDBPClass.Class != NULL)
	{
		HUDClass = PlayerHUDBPClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AGameStateBase> ProjectGameStateClass(TEXT("/Game/ThirdPerson/Blueprints/testProjectGameState"));
	if (ProjectGameStateClass.Class != NULL)
	{
		GameStateClass = ProjectGameStateClass.Class;
	}
}
