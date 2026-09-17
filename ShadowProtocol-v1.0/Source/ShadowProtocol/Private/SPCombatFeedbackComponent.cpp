#include "SPCombatFeedbackComponent.h"
USPCombatFeedbackComponent::USPCombatFeedbackComponent(){SetIsReplicatedByDefault(true);}
void USPCombatFeedbackComponent::MulticastWeaponFired_Implementation(bool bSuppressed){BP_WeaponFired(bSuppressed);}
void USPCombatFeedbackComponent::MulticastSuppression_Implementation(float Intensity){BP_Suppression(FMath::Clamp(Intensity,0.f,1.f));}
void USPCombatFeedbackComponent::MulticastBreachImpact_Implementation(FVector_NetQuantize Location,float Intensity){BP_BreachImpact(Location,FMath::Clamp(Intensity,0.f,1.f));}
