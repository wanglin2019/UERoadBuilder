// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Shared road editing tool layer, written against the InteractiveToolsFramework (ITF).
 *
 * The same tool implementations are driven by two hosts:
 *   - editor host:  URoadToolsMode (UEdMode) in the RoadBuilderToolsEditor module
 *   - runtime host: a game-side UInteractiveToolsContext in the RoadBuilderToolsRuntime module
 *
 * ITF itself is a Runtime module (Core / CoreUObject / InputCore / ApplicationCore / MeshDescription /
 * Engine / RHI / GeometryCore only), which is the entire reason this layer can live outside the editor.
 * Keep it free of editor-only types so that stays true.
 */
class FRoadBuilderTools : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
