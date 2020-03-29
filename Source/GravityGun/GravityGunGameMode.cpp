// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GravityGunGameMode.h"
#include "GravityGunHUD.h"
#include "GravityGunCharacter.h"
#include "UObject/ConstructorHelpers.h"

AGravityGunGameMode::AGravityGunGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = AGravityGunHUD::StaticClass();
}
