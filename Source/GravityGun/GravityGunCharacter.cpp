// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GravityGunCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"	
#include "PhysicsEngine/PhysicsHandleComponent.h"

#include "Interactable/Grabbable.h"	

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// AGravityGunCharacter

const float AGravityGunCharacter::MAX_GRAB_DISTANCE = 1000.0;
const float AGravityGunCharacter::THROW_FORCE = 1000000.0;
const float AGravityGunCharacter::DROP_DISTANCE_THRESHOLD = 1.5f;

AGravityGunCharacter::AGravityGunCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	// Create a gun mesh component
	FPG_Gun = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FPG_Gun"));
	FPG_Gun->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	FPG_Gun->bCastDynamicShadow = false;
	FPG_Gun->CastShadow = false;
	// FPG_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FPG_Gun->SetupAttachment(RootComponent);

	// Setup grab system
	isHoldingObject = false;
	physicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));
	grabLocation = CreateDefaultSubobject<USceneComponent>(TEXT("GrabLocation"));
	grabLocation->SetupAttachment(FirstPersonCameraComponent);
	grabLocation->SetRelativeLocation(FVector(200.0, 0.0, 0.0));
}

void AGravityGunCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FPG_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::KeepWorld, true), TEXT("GripPoint"));
}

//////////////////////////////////////////////////////////////////////////
// Input

void AGravityGunCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind grab and throw events
	PlayerInputComponent->BindAction("Grab", IE_Pressed, this, &AGravityGunCharacter::OnGrab);
	PlayerInputComponent->BindAction("Throw", IE_Pressed, this, &AGravityGunCharacter::OnThrow);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &AGravityGunCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AGravityGunCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AGravityGunCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AGravityGunCharacter::LookUpAtRate);
}

void AGravityGunCharacter::OnGrab()
{
	if (isHoldingObject)
		DropObject();
	else GrabObject();
}

void AGravityGunCharacter::OnThrow()
{
	if (isHoldingObject)
		ThrowObject();
}

void AGravityGunCharacter::GrabObject()
{
	UWorld* const World = GetWorld();
	if (World != NULL) {
		FHitResult hit;
		FVector start = this->FirstPersonCameraComponent->GetComponentLocation();
		FVector end = start + this->FirstPersonCameraComponent->GetForwardVector() * MAX_GRAB_DISTANCE;

		ECollisionChannel traceChannel = ECC_Visibility;
		FCollisionQueryParams queryParams;
		queryParams.AddIgnoredActor(this);
		FCollisionResponseParams responseParams;

		bool hasHit = World->LineTraceSingleByChannel(hit, start, end, traceChannel, queryParams, responseParams);

		if (hasHit) {
			UE_LOG(LogTemp, Log, TEXT("Grab hit %s"), *hit.GetActor()->GetClass()->GetName());
			//DrawDebugLine(GetWorld(), start, end, FColor::Red, false, 5.f, ECC_WorldStatic, 5.f);
			grabbedObject = Cast<IGrabbable>(hit.Actor);
			if (grabbedObject) {
				grabbedObject->OnGrab();

				isHoldingObject = true;
				grabbedMesh = Cast<UMeshComponent>(hit.GetComponent());
				physicsHandle->GrabComponentAtLocation(grabbedMesh, NAME_None, grabbedMesh->Bounds.GetBox().GetCenter());
				grabbedMesh->SetAngularDamping(10.0f);

				// Play reverse animation
				if (GrabAnimation != NULL) {
					UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
					if (AnimInstance != NULL) {
						AnimInstance->Montage_Play(GrabAnimation, -1.f, EMontagePlayReturnType::MontageLength, 1.0f);
					}
				}

				// Play sound
				if (GrabSound != NULL) {
					UGameplayStatics::PlaySoundAtLocation(this, GrabSound, GetActorLocation());
				}
			}
		} else {
			UE_LOG(LogTemp, Log, TEXT("Grab miss"));
			//DrawDebugLine(GetWorld(), start, end, FColor::Green, false, 5.f, ECC_WorldStatic, 5.f);
		}
	}
}

void AGravityGunCharacter::DropObject()
{
	grabbedObject->OnRelease();

	physicsHandle->ReleaseComponent();
	grabbedMesh->SetAngularDamping(0.0f);
	isHoldingObject = false;
}

void AGravityGunCharacter::ThrowObject()
{
	DropObject();

	FVector force = this->FirstPersonCameraComponent->GetForwardVector() * THROW_FORCE;
	grabbedMesh->AddImpulse(force);

	// Play animation
	if (GrabAnimation != NULL) {
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != NULL) {
			AnimInstance->Montage_Play(GrabAnimation, 1.f);
		}
	}

	// Play sound
	if (ThrowSound != NULL) {
		UGameplayStatics::PlaySoundAtLocation(this, ThrowSound, GetActorLocation());
	}
}

void AGravityGunCharacter::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	UpdateGravityGun(DeltaTime);
}

void AGravityGunCharacter::UpdateGravityGun(float DeltaTime)
{
	// Update grabbing location
	physicsHandle->SetTargetLocation(grabLocation->GetComponentLocation());

	// Drop if object is too far
	if (isHoldingObject) {
		FVector cameraPosition = this->FirstPersonCameraComponent->GetComponentLocation();
		FVector meshPosition = grabbedMesh->GetComponentLocation();
		float distance = FVector::Distance(cameraPosition, meshPosition);
		if (distance > MAX_GRAB_DISTANCE * DROP_DISTANCE_THRESHOLD) {
			DropObject();
		}
	}
}

void AGravityGunCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AGravityGunCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AGravityGunCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AGravityGunCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

