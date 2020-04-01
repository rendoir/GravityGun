// Fill out your copyright notice in the Description page of Project Settings.

#include "Grabbable.h"


void IGrabbable::OnGrab() {
	UE_LOG(LogTemp, Log, TEXT("OnGrab"));
}

void IGrabbable::OnRelease() {
	UE_LOG(LogTemp, Log, TEXT("OnRelease"));
}
