// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Editor host of the road tool layer, built on the new editor-mode framework (UEdMode + ITF).
 *
 * It lives next to the legacy RoadBuilderEditor module rather than replacing it, so the two modes can be
 * compared in the editor and the legacy one can be deleted independently once the migration is done.
 */
class FRoadBuilderToolsEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
