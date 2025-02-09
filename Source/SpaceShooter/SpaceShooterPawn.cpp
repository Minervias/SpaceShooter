// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SpaceShooterPawn.h"
#include "SpaceShooterProjectile.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

const FName ASpaceShooterPawn::MoveForwardBinding("MoveForward");
const FName ASpaceShooterPawn::MoveRightBinding("MoveRight");
const FName ASpaceShooterPawn::FireForwardBinding("FireForward");
const FName ASpaceShooterPawn::FireRightBinding("FireRight");
const FName ASpaceShooterPawn::SpeedBoostBinding("SpeedBoost");
const FName ASpaceShooterPawn::JumpBinding("JumpAbility");

ASpaceShooterPawn::ASpaceShooterPawn()
{	
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShipMesh(TEXT("/Game/TwinStick/Meshes/TwinStickUFO.TwinStickUFO"));
	// Create the mesh component
	ShipMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	RootComponent = ShipMeshComponent;
	ShipMeshComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	ShipMeshComponent->SetStaticMesh(ShipMesh.Object);
	
	// Cache our sound effect
	static ConstructorHelpers::FObjectFinder<USoundBase> FireAudio(TEXT("/Game/TwinStick/Audio/TwinStickFire.TwinStickFire"));
	FireSound = FireAudio.Object;

	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bAbsoluteRotation = true; // Don't want arm to rotate when ship does
	CameraBoom->TargetArmLength = 1200.f;
	CameraBoom->RelativeRotation = FRotator(-80.f, 0.f, 0.f);
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	// Create a camera...
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	CameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	CameraComponent->bUsePawnControlRotation = false;	// Camera does not rotate relative to arm

	// Movement
	MoveSpeed = 1000.0f;
	// Weapon
	GunOffset = FVector(90.f, 0.f, 0.f);
	FireRate = 0.1f;
	bCanFire = true;
	// SpeedBoost
	SpeedBoost = MoveSpeed + 1500.0f;
	JumpActivated = false;

}

void ASpaceShooterPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	// set up gameplay key bindings
	PlayerInputComponent->BindAxis(MoveForwardBinding);
	PlayerInputComponent->BindAxis(MoveRightBinding);
	PlayerInputComponent->BindAxis(FireForwardBinding);
	PlayerInputComponent->BindAxis(FireRightBinding);

	// Speed Boost Bindings
	PlayerInputComponent->BindAction("SpeedBoost", IE_Pressed, this, &ASpaceShooterPawn::SpeedBoostAbility);
	PlayerInputComponent->BindAction("SpeedBoost", IE_Released, this, &ASpaceShooterPawn::SpeedBoostAbilityStop);

	// Jump Bindings
	PlayerInputComponent->BindAction("JumpAbility", IE_Pressed, this, &ASpaceShooterPawn::JumpAbility);
	PlayerInputComponent->BindAction("JumpAbility", IE_Released, this, &ASpaceShooterPawn::JumpAbilityStop);
}


// Increase Speed of Ship with shift
void ASpaceShooterPawn::SpeedBoostAbility()
{
	MoveSpeed = SpeedBoost;
}

// Return Speed of Ship to original with shift release
void ASpaceShooterPawn::SpeedBoostAbilityStop()
{
	MoveSpeed = MoveSpeed - 1500.0f;
}

// Jump Ship height of 2m
void ASpaceShooterPawn::JumpAbility()
{
	JumpActivated = true;
}

// Return ship to previous height
void ASpaceShooterPawn::JumpAbilityStop()
{
	JumpActivated = false;
}


void ASpaceShooterPawn::Tick(float DeltaSeconds)
{
	// Find movement direction
	const float ForwardValue = GetInputAxisValue(MoveForwardBinding);
	const float RightValue = GetInputAxisValue(MoveRightBinding);

	// Clamp max size so that (X=1, Y=1) doesn't cause faster movement in diagonal directions
	const FVector MoveDirection = FVector(ForwardValue, RightValue, 0.f).GetClampedToMaxSize(1.0f);

	if (JumpActivated)
	{
		const FVector MoveDirection = FVector(ForwardValue, RightValue, 1000.f).GetClampedToMaxSize(1.0f);
	}

	// Calculate  movement
	const FVector Movement = MoveDirection * MoveSpeed * DeltaSeconds;

	// If non-zero size, move this actor
	if (Movement.SizeSquared() > 0.0f)
	{
		const FRotator NewRotation = Movement.Rotation();
		FHitResult Hit(1.f);

		RootComponent->MoveComponent(Movement, NewRotation, true, &Hit);
		
		if (Hit.IsValidBlockingHit())
		{
			const FVector Normal2D = Hit.Normal.GetSafeNormal2D();
			const FVector Deflection = FVector::VectorPlaneProject(Movement, Normal2D) * (1.f - Hit.Time);
			RootComponent->MoveComponent(Deflection, NewRotation, true);
		}

		
	}
	
	// Create fire direction vector
	const float FireForwardValue = GetInputAxisValue(FireForwardBinding);
	const float FireRightValue = GetInputAxisValue(FireRightBinding);
	const FVector FireDirection = FVector(FireForwardValue, FireRightValue, 0.f);

	// Try and fire a shot
	FireShot(FireDirection);
}

void ASpaceShooterPawn::FireShot(FVector FireDirection)
{
	// If it's ok to fire again
	if (bCanFire == true)
	{
		// If we are pressing fire stick in a direction
		if (FireDirection.SizeSquared() > 0.0f)
		{
			const FRotator FireRotation = FireDirection.Rotation();
			// Spawn projectile at an offset from this pawn
			const FVector SpawnLocation = GetActorLocation() + FireRotation.RotateVector(GunOffset);

			UWorld* const World = GetWorld();
			if (World != NULL)
			{
				// spawn the projectile
				World->SpawnActor<ASpaceShooterProjectile>(SpawnLocation, FireRotation);
			}

			bCanFire = false;
			World->GetTimerManager().SetTimer(TimerHandle_ShotTimerExpired, this, &ASpaceShooterPawn::ShotTimerExpired, FireRate);

			// try and play the sound if specified
			if (FireSound != nullptr)
			{
				UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
			}

			bCanFire = false;
		}
	}
}

void ASpaceShooterPawn::ShotTimerExpired()
{
	bCanFire = true;
}

