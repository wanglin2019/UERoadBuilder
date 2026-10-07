// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadBuilderToolsRuntime.h"

#include "RoadLog.h"

void FRoadBuilderToolsRuntime::StartupModule()
{
	// Nothing to register: the host is a UWorldSubsystem, which the engine instantiates per world on demand
	// and which decides for itself whether a world can host editing (see ShouldCreateSubsystem).
	//
	// The subsystem is off until a game calls URoadToolsWorldSubsystem::StartEditing(). That is deliberate:
	// this module is a Runtime one of an enabled plugin, so it loads in a packaged game whether or not the
	// game wants an editor, and a host that started editing unprompted would be a surprise in every project
	// that ships the plugin.
	RoadLog_Debug(TEXT("RoadBuilderToolsRuntime module started (host is off until StartEditing)"));
}

void FRoadBuilderToolsRuntime::ShutdownModule()
{
}

IMPLEMENT_MODULE(FRoadBuilderToolsRuntime, RoadBuilderToolsRuntime)
