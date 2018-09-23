// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SpaceShooterGameMode.h"
#include "SpaceShooterPawn.h"

ASpaceShooterGameMode::ASpaceShooterGameMode()
{
	// set default pawn class to our character class
	DefaultPawnClass = ASpaceShooterPawn::StaticClass();
}

