// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Inline panel of the road editing mode: **pure layout**.
 * Lays the mode-owned detail panel into three slots — never creates, holds or releases FRoadInspector
 * (ownership is in the mode: created in Enter, cleared in Exit), so this class has no members and no destructor.
 *
 * Tool palettes (button rows) are built by FRoadEdModeToolkit itself from the registry;
 * tool indexing, per-palette default tools and the tool-to-settings mapping all live in FEdModeRoad.
 * This class knows nothing about palettes or any Tool.
 */
class SRoadEdit : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRoadEdit) {}
	SLATE_END_ARGS()

	/** SCompoundWidget functions */
	void Construct(const FArguments& InArgs);
};
