#include "LbbInputListDetailsObject.h"

#include "Framework/Notifications/NotificationManager.h"
#include "UObject/UnrealType.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace
{
	static void ShowBasePoseRestoredNotification()
	{
		FNotificationInfo NotificationInfo(NSLOCTEXT(
			"LbbInputListDetailsObject",
			"BasePoseRestored",
			"BasePose is required and was restored automatically."));
		NotificationInfo.ExpireDuration = 3.0f;
		NotificationInfo.bUseLargeFont = false;
		NotificationInfo.bUseSuccessFailIcons = true;

		if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo))
		{
			Notification->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}
}

void ULbbInputListDetailsObject::NormalizeInputDefinitions(const bool bNotifyIfBasePoseWasRestored)
{
	if (bIsNormalizingInputDefinitions)
	{
		return;
	}

	TGuardValue<bool> Guard(bIsNormalizingInputDefinitions, true);
	const bool bHadBasePose = InputDefinitions.ContainsByPredicate([](const FLbbInputPoseDefinition& InputDefinition)
	{
		return Lbb::IsBasePoseInputName(InputDefinition.InputName);
	});
	Lbb::NormalizeInputPoseDefinitions(InputDefinitions);

	if (bNotifyIfBasePoseWasRestored && !bHadBasePose)
	{
		ShowBasePoseRestoredNotification();
	}
}

#if WITH_EDITOR
void ULbbInputListDetailsObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	NormalizeInputDefinitions(true);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ULbbInputListDetailsObject::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	NormalizeInputDefinitions(true);
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif
