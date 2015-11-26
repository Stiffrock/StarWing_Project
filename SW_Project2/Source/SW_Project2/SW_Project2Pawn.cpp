// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SW_Project2.h"
#include "SW_Project2Pawn.h"
#include "Engine.h"
#include "Projectile.h"
#include "Block.h"
#include "Engine.h"
#include "SW_Project2GameMode.h"

ASW_Project2Pawn::ASW_Project2Pawn()
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UStaticMesh> PlaneMesh;
		FConstructorStatics()
			: PlaneMesh(TEXT("/Game/Flying/Meshes/UFO.UFO"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	// Create static mesh component
	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh0"));
	PlaneMesh->SetStaticMesh(ConstructorStatics.PlaneMesh.Get());
	RootComponent = PlaneMesh;

	// Create a spring arm component
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm0"));
	SpringArm->AttachTo(RootComponent);
	SpringArm->TargetArmLength = 160.0f; // The camera follows at this distance behind the character	
	SpringArm->SocketOffset = FVector(0.f,0.f,60.f);
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 3.f;

	PointLight = CreateDefaultSubobject<UPointLightComponent>(FName(TEXT("TestLight")));
	PointLight->AttachTo(this->GetPlaneMesh(), "Socket_Shoot");

	// Create camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera0"));
	Camera->AttachTo(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false; // Don't rotate camera with controller

	// Set handling parameters
	Acceleration = 500.f;
	TurnSpeed = 50.f;
	MaxSpeed = 4000.f;
	MinSpeed = 1000.f;
	CurrentForwardSpeed = 1200.f;

	MuzzleOffset = FVector(200.f, 0.f, 0.f);

	energy = 1.0f;
	health = 1.0f;

	bThrust = false;
	bFinish = false;

	static ConstructorHelpers::FObjectFinder<UBlueprint> ItemBlueprint(TEXT("Blueprint'/Game/Projectile_BP.Projectile_BP'"));
	if (ItemBlueprint.Object)
	Projectile_BP = (UClass*)ItemBlueprint.Object->GeneratedClass;
}

void ASW_Project2Pawn::Tick(float DeltaSeconds)
{
	const FVector LocalMove = FVector(CurrentForwardSpeed * DeltaSeconds, 0.f, 0.f);

	AGameMode* gameModePtr = GetWorld()->GetAuthGameMode();
	ASW_Project2GameMode* ProjectGameModePtr = Cast<ASW_Project2GameMode>(gameModePtr);

	if (health <= 0.0f)
		this->Destroy();

	if (!ProjectGameModePtr->bMapPause)
	{
	

		// Move plan forwards (with sweep so we stop when we collide with things)
		AddActorLocalOffset(LocalMove, true);
		SetEnergy(DeltaSeconds);
		// Calculate change in rotation this frame
		FRotator DeltaRotation(0, 0, 0);
		FRotator CurrentRotation = GetActorRotation();

		DeltaRotation.Pitch = CurrentPitchSpeed * DeltaSeconds;
		DeltaRotation.Yaw = CurrentYawSpeed * DeltaSeconds;
		DeltaRotation.Roll = CurrentRollSpeed * DeltaSeconds;


		// Rotate and limit plane
		if (CurrentRotation.Pitch >= 30.0f && DeltaRotation.Pitch > 0.f)
			DeltaRotation.Pitch = 0.f;

		if (CurrentRotation.Pitch <= -30.0f && DeltaRotation.Pitch < 0.f)
			DeltaRotation.Pitch = 0.f;

		if (CurrentRotation.Yaw >= 30.0f && DeltaRotation.Yaw > 0.f)
			DeltaRotation.Yaw = 0.f;

		if (CurrentRotation.Yaw <= -30.0f && DeltaRotation.Yaw < 0.f)
			DeltaRotation.Yaw = 0.f;

		AddActorLocalRotation(DeltaRotation);

	}
	else
	CurrentForwardSpeed = 0.0f;

	// Call any parent class Tick implementation
	Super::Tick(DeltaSeconds);
}

void ASW_Project2Pawn::SetEnergy(float DeltaSeconds)
{
	if (bThrust && energy != 0.0f)
		energy -= 0.5f * DeltaSeconds;
	if (energy <= 1.0f)
		energy += 0.1f * DeltaSeconds;
	
}

void ASW_Project2Pawn::NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	ABlock* block = Cast<ABlock>(Other);

	if (Other == block)
	{
		if (!block->Pickup)
			HurtfulCollision(Other);
		else
			PickupCollision(Other);
		
		if (block->Finish)
			FinishedGame();
		
	}
}

void ASW_Project2Pawn::PickupCollision(AActor* Other)
{
	AGameMode* gModePtr = GetWorld()->GetAuthGameMode();
	ASW_Project2GameMode* GameModePtr = Cast<ASW_Project2GameMode>(gModePtr);
	GameModePtr->time += 10.0f;
	Other->Destroy();
}

void ASW_Project2Pawn::HurtfulCollision(AActor* Other)
{
	AGameMode* gModePtr = GetWorld()->GetAuthGameMode();
	ASW_Project2GameMode* GameModePtr = Cast<ASW_Project2GameMode>(gModePtr);
	GameModePtr->health -= 0.2f;
	health -= 0.2f;
	Other->Destroy();
	CurrentForwardSpeed = 0.f;
}

void ASW_Project2Pawn::FinishedGame()
{
	UObject* worldGameMode = GetWorld()->GetAuthGameMode();
	ASW_Project2GameMode* gameMode = Cast<ASW_Project2GameMode>(worldGameMode);
	gameMode->bGameFinish = true;
}


void ASW_Project2Pawn::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	check(InputComponent);

	// Bind our control axis' to callback functions
	InputComponent->BindAxis("Thrust", this, &ASW_Project2Pawn::ThrustInput);
	InputComponent->BindAxis("MoveUp", this, &ASW_Project2Pawn::MoveUpInput);
	InputComponent->BindAxis("MoveRight", this, &ASW_Project2Pawn::MoveRightInput);
	InputComponent->BindAction("Shoot", IE_Pressed, this, &ASW_Project2Pawn::ShootProjectile);
}

void ASW_Project2Pawn::ThrustInput(float Val)
{
	if (energy >= 0.0f)
		ThrustAccelerate(Val);
	else
		ThurstDeaccelerate(Val);
}

void ASW_Project2Pawn::ThurstDeaccelerate(float Val)
{
	bool bHasInput = !FMath::IsNearlyEqual(Val, 0.f);
	bHasInput = false;
	bThrust = false;
	// If input is not held down, reduce speed
	float CurrentAcc = bHasInput ? (Val * Acceleration) : (-0.5f * Acceleration);
	// Calculate new speed
	float NewForwardSpeed = CurrentForwardSpeed + (GetWorld()->GetDeltaSeconds() * CurrentAcc);
	// Clamp between MinSpeed and MaxSpeed
	CurrentForwardSpeed = FMath::Clamp(NewForwardSpeed, MinSpeed, MaxSpeed);
}

void ASW_Project2Pawn::ThrustAccelerate(float Val)
{
	bool bHasInput = !FMath::IsNearlyEqual(Val, 0.f);
	// If input is not held down, reduce speed
	float CurrentAcc = bHasInput ? (Val * Acceleration) : (-0.5f * Acceleration);
	// Calculate new speed
	float NewForwardSpeed = CurrentForwardSpeed + (GetWorld()->GetDeltaSeconds() * CurrentAcc);
	// Clamp between MinSpeed and MaxSpeed
	CurrentForwardSpeed = FMath::Clamp(NewForwardSpeed, MinSpeed, MaxSpeed);
	//Set bool
	if (bHasInput)
		bThrust = true;
	else
		bThrust = false;
}

void ASW_Project2Pawn::MoveUpInput(float Val)
{
	// Target pitch speed is based in input
	float TargetPitchSpeed = (Val * TurnSpeed * 1.f);	
	// When steering, we decrease pitch slightly
	TargetPitchSpeed += (FMath::Abs(CurrentYawSpeed) * -0.2f);
	// Smoothly interpolate to target pitch speed
	CurrentPitchSpeed = FMath::FInterpTo(CurrentPitchSpeed, TargetPitchSpeed, GetWorld()->GetDeltaSeconds(), 2.f);
}

void ASW_Project2Pawn::MoveRightInput(float Val)
{
	// Target yaw speed is based on input
	float TargetYawSpeed = (Val * TurnSpeed);

	// Smoothly interpolate to target yaw speed
	CurrentYawSpeed = FMath::FInterpTo(CurrentYawSpeed, TargetYawSpeed, GetWorld()->GetDeltaSeconds(), 2.f);

	// Is there any left/right input?
	const bool bIsTurning = FMath::Abs(Val) > 0.2f;

	// If turning, yaw value is used to influence roll
	// If not turning, roll to reverse current roll value
	float TargetRollSpeed = bIsTurning ? (CurrentYawSpeed * 0.5f) : (GetActorRotation().Roll * -2.f);

	// Smoothly interpolate roll speed
	CurrentRollSpeed = FMath::FInterpTo(CurrentRollSpeed, TargetRollSpeed, GetWorld()->GetDeltaSeconds(), 2.f);
}

void ASW_Project2Pawn::ShootProjectile()
{
	FVector CameraLoc = GetActorLocation();
	FRotator CameraRot = GetActorRotation();
	FVector const MuzzleLocation = CameraLoc + FTransform(CameraRot).TransformVector(MuzzleOffset);
	FRotator MuzzleRotation = CameraRot;

	UWorld* const World = GetWorld();
	if (World && energy >= 0.25f)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = Instigator;
		AProjectile* Projectile = World->SpawnActor<AProjectile>(Projectile_BP, MuzzleLocation, MuzzleRotation, SpawnParams);
		energy -= 0.25f;
	}
}

