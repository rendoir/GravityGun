#include "GrabbableActor.h"

void AGrabbableActor::OnGrab() {
	UE_LOG(LogTemp, Log, TEXT("Actor Grabbed"));
}

void AGrabbableActor::OnRelease() {
	UE_LOG(LogTemp, Log, TEXT("Actor Released"));
}
