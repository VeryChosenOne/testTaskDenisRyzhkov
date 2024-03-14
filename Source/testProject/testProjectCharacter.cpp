// Copyright Epic Games, Inc. All Rights Reserved.

#include "testProjectCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AtestProjectCharacter

AtestProjectCharacter::AtestProjectCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AtestProjectCharacter::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AtestProjectCharacter, LookRotation);
}


void AtestProjectCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AtestProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AtestProjectCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AtestProjectCharacter::Look);

		EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &AtestProjectCharacter::Dash);

		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, & AtestProjectCharacter::Aim);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, & AtestProjectCharacter::AimStop);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AtestProjectCharacter::Server_LookRotation_Implementation(FRotator Rotation) {
	LookRotation = Rotation;
}

void AtestProjectCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AtestProjectCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		Server_LookRotation(Controller->GetControlRotation());
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

//Dash
void AtestProjectCharacter::DashCooldownReset() {
	DashCD = false;
}

void AtestProjectCharacter::Dash() {
	if (!ADS) {
		if (!DashCD) {
			DashCD = true;
			FTimerHandle CoolDownResetTimerHandle;
			FTimerDelegate TimerDel;
			TimerDel.BindUFunction(this, FName("DashCooldownReset"));
			GetWorld()->GetTimerManager().SetTimer(CoolDownResetTimerHandle, TimerDel, 1.0f, false);

			Server_Dash();
		}
	}
}

void AtestProjectCharacter::Server_Dash_Implementation() {
	Client_DashMulti();
}

void AtestProjectCharacter::Client_DashMulti_Implementation() {
	GetMesh()->bPauseAnims = true;
	StartLocation = GetActorLocation();
	
	if (FVector(GetVelocity().X, GetVelocity().Y, 0).Length() > 5)
		ForwardVector = FVector(GetVelocity().X, GetVelocity().Y, 0);
	else
		ForwardVector = FVector(FollowCamera->GetForwardVector().X, FollowCamera->GetForwardVector().Y, 0);

	ForwardVector.Normalize();

	DashAlpha = 0.0f;
	DashInAction();
}

void AtestProjectCharacter::DashInAction() {
	if (DashAlpha <= 1.0f) {
		DashAlpha += GetWorld()->GetDeltaSeconds() / 0.5f;

		SetActorLocation(FMath::Lerp(StartLocation, StartLocation + 1000 * ForwardVector, DashAlpha), true);
		FTimerDelegate TimerDel;
		TimerDel.BindUFunction(this, FName("DashInAction"));
		GetWorld()->GetTimerManager().SetTimerForNextTick(TimerDel);
	}
	else {
		GetCharacterMovement()->MaxWalkSpeed = 500;
		GetMesh()->bPauseAnims = false;
	}
}

//ADS
void AtestProjectCharacter::Aim() {
	ADS = true;
	GetCharacterMovement()->MaxWalkSpeed = 250;
	Server_Aim(true);
	fTargetCameraInterp = 100;
	vTargetCameraInterp = FVector(0, 50, 30);
	if (!InterpTimer) {
		InterpTimer = true;
		AimCameraInterp();
	}
}

void AtestProjectCharacter::AimStop() {
	ADS = false;
	GetCharacterMovement()->MaxWalkSpeed = 500;
	Server_Aim(false);
	fTargetCameraInterp = 200;
	vTargetCameraInterp = FVector(0, 100, 0);
	if (!InterpTimer) {
		InterpTimer = true;
		AimCameraInterp();
	}
}

void AtestProjectCharacter::Server_Aim_Implementation(bool b) {
	Client_AimMulti(b);
}

void AtestProjectCharacter::Client_AimMulti_Implementation(bool b) {
	if (!IsLocallyControlled()) {
		if (b) {
			ADS = true;
			GetCharacterMovement()->MaxWalkSpeed = 250;
		}
		else {
			ADS = false;
			GetCharacterMovement()->MaxWalkSpeed = 500;
		}
	}
}

void AtestProjectCharacter::AimCameraInterp() {
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, fTargetCameraInterp, GetWorld()->GetDeltaSeconds(), 10);
		CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, vTargetCameraInterp, GetWorld()->GetDeltaSeconds(), 10);

		if (CameraBoom->TargetArmLength == fTargetCameraInterp && vTargetCameraInterp == CameraBoom->SocketOffset) {
			InterpTimer = false;
		}
		else {
			FTimerDelegate TimerDel;
			TimerDel.BindUFunction(this, FName("AimCameraInterp"));
			GetWorld()->GetTimerManager().SetTimerForNextTick(TimerDel);
		}
}
