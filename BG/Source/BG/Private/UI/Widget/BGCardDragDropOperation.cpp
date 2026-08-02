// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widget/BGCardDragDropOperation.h"
#include "UI/Widget/BGCardWidget.h"

void UBGCardDragDropOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
	RestoreSourceWidget();
	Super::Drop_Implementation(PointerEvent);
}

void UBGCardDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	RestoreSourceWidget();
	Super::DragCancelled_Implementation(PointerEvent);
}

void UBGCardDragDropOperation::RestoreSourceWidget()
{
	if (UBGCardWidget* Source = SourceCardWidget.Get())
	{
		Source->SetBeingDragged(false);
	}
}
