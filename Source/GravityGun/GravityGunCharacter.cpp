// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GravityGunCharacter.h"
#include "GravityGunProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId
#include "DrawDebugHelpers.h"	
#include "Interactable/Grabbable.h"	
#include "PhysicsEngine/PhysicsHandleComponent.h"

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

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FPG_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	// Note: The ProjectileClass and the skeletal mesh/anim blueprints for Mesh1P, FPG_Gun, and VR_Gun 
	// are set in the derived blueprint asset named MyCharacter to avoid direct content references in C++.

	// Create VR Controllers.
	R_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("R_MotionController"));
	R_MotionController->MotionSource = FXRMotionControllerBase::RightHandSourceId;
	R_MotionController->SetupAttachment(RootComponent);
	L_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("L_MotionController"));
	L_MotionController->SetupAttachment(RootComponent);

	// Create a gun and attach it to the right-hand VR controller.
	// Create a gun mesh component
	VR_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VR_Gun"));
	VR_Gun->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	VR_Gun->bCastDynamicShadow = false;
	VR_Gun->CastShadow = false;
	VR_Gun->SetupAttachment(R_MotionController);
	VR_Gun->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	VR_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("VR_MuzzleLocation"));
	VR_MuzzleLocation->SetupAttachment(VR_Gun);
	VR_MuzzleLocation->SetRelativeLocation(FVector(0.000004, 53.999992, 10.000000));
	VR_MuzzleLocation->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));		// Counteract the rotation of the VR gun model.

	// Uncomment the following line to turn motion controllers on by default:
	//bUsingMotionControllers = true;

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

	// Show or hide the two versions of the gun based on whether or not we're using motion controllers.
	if (bUsingMotionControllers)
	{
		VR_Gun->SetHiddenInGame(false, true);
		Mesh1P->SetHiddenInGame(true, true);
	}
	else
	{
		VR_Gun->SetHiddenInGame(true, true);
		Mesh1P->SetHiddenInGame(false, true);
	}
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

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AGravityGunCharacter::OnFire);

	// Bind grab and throw events
	PlayerInputComponent->BindAction("Grab", IE_Pressed, this, &AGravityGunCharacter::OnGrab);
	PlayerInputComponent->BindAction("Throw", IE_Pressed, this, &AGravityGunCharacter::OnThrow);

	// Enable touchscreen input
	EnableTouchscreenMovement(PlayerInputComponent);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AGravityGunCharacter::OnResetVR);

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

void AGravityGunCharacter::OnFire()
{
	// try and fire a projectile
	if (ProjectileClass != NULL)
	{
		UWorld* const World = GetWorld();
		if (World != NULL)
		{
			if (bUsingMotionControllers)
			{
				const FRotator SpawnRotation = VR_MuzzleLocation->GetComponentRotation();
				const FVector SpawnLocation = VR_MuzzleLocation->GetComponentLocation();
				World->SpawnActor<AGravityGunProjectile>(ProjectileClass, SpawnLocation, SpawnRotation);
			}
			else
			{
				const FRotator SpawnRotation = GetControlRotation();
				// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
				const FVector SpawnLocation = ((FP_MuzzleLocation != nullptr) ? FP_MuzzleLocation->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);

				//Set Spawn Collision Handling Override
				FActorSpawnParameters ActorSpawnParams;
				ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

				// spawn the projectile at the muzzle
				World->SpawnActor<AGravityGunProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
			}
		}
	}

	// try and play the sound if specified
	if (FireSound != NULL)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	// try and play a firing animation if specified
	if (FireAnimation != NULL)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != NULL)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
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
				if (FireAnimation != NULL) {
					UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
					if (AnimInstance != NULL) {
						AnimInstance->Montage_Play(FireAnimation, -1.f, EMontagePlayReturnType::MontageLength, 1.0f);
					}
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
	if (FireAnimation != NULL) {
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != NULL) {
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

void AGravityGunCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AGravityGunCharacter::BeginTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == true)
	{
		return;
	}
	if ((FingerIndex == TouchItem.FingerIndex) && (TouchItem.bMoved == false))
	{
		OnFire();
	}
	TouchItem.bIsPressed = true;
	TouchItem.FingerIndex = FingerIndex;
	TouchItem.Location = Location;
	TouchItem.bMoved = false;
}

void AGravityGunCharacter::EndTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == false)
	{
		return;
	}
	TouchItem.bIsPressed = false;
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

//Commenting this section out to be consistent with FPS BP template.
//This allows the user to turn without using the right virtual joystick

//void AGravityGunCharacter::TouchUpdate(const ETouchIndex::Type FingerIndex, const FVector Location)
//{
//	if ((TouchItem.bIsPressed == true) && (TouchItem.FingerIndex == FingerIndex))
//	{
//		if (TouchItem.bIsPressed)
//		{
//			if (GetWorld() != nullptr)
//			{
//				UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
//				if (ViewportClient != nullptr)
//				{
//					FVector MoveDelta = Location - TouchItem.Location;
//					FVector2D ScreenSize;
//					ViewportClient->GetViewportSize(ScreenSize);
//					FVector2D ScaledDelta = FVector2D(MoveDelta.X, MoveDelta.Y) / ScreenSize;
//					if (FMath::Abs(ScaledDelta.X) >= 4.0 / ScreenSize.X)
//					{
//						TouchItem.bMoved = true;
//						float Value = ScaledDelta.X * BaseTurnRate;
//						AddControllerYawInput(Value);
//					}
//					if (FMath::Abs(ScaledDelta.Y) >= 4.0 / ScreenSize.Y)
//					{
//						TouchItem.bMoved = true;
//						float Value = ScaledDelta.Y * BaseTurnRate;
//						AddControllerPitchInput(Value);
//					}
//					TouchItem.Location = Location;
//				}
//				TouchItem.Location = Location;
//			}
//		}
//	}
//}

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

bool AGravityGunCharacter::EnableTouchscreenMovement(class UInputComponent* PlayerInputComponent)
{
	if (FPlatformMisc::SupportsTouchInput() || GetDefault<UInputSettings>()->bUseMouseForTouch)
	{
		PlayerInputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AGravityGunCharacter::BeginTouch);
		PlayerInputComponent->BindTouch(EInputEvent::IE_Released, this, &AGravityGunCharacter::EndTouch);

		//Commenting this out to be more consistent with FPS BP template.
		//PlayerInputComponent->BindTouch(EInputEvent::IE_Repeat, this, &AGravityGunCharacter::TouchUpdate);
		return true;
	}
	
	return false;
}
