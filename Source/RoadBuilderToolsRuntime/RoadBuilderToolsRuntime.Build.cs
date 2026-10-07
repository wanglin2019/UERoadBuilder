// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RoadBuilderToolsRuntime : ModuleRules
	{
		public RoadBuilderToolsRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					// The runtime host derives from UInteractiveToolsContext and implements the three host
					// interfaces (Queries / Transactions / Render), so ITF is part of its public shape -
					// URoadToolsContext's base class and RoadToolsWorldSubsystem's members name its types.
					"InteractiveToolsFramework",
					"RoadBuilder",
					"RoadBuilderTools",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"InputCore",
					// Runtime host UI. These are Runtime modules themselves (UMG / Slate / SlateCore all
					// live under Engine/Source/Runtime), which is what makes an in-game editing host
					// possible at all. They are declared here and not in RoadBuilderTools on purpose:
					// the shared tool layer stays free of UI so that it keeps compiling in a game target.
					"Slate",
					"SlateCore",
					"UMG",
				}
				);

			// The runtime host, complete as of S4. Map of what lives where:
			//
			//   Public|Private/Editing/   the host core - URoadToolsContext (a UInteractiveToolsContext
			//                             subclass) plus its three ITF interfaces: FRoadRuntimeQueriesAPI,
			//                             FRoadRuntimeTransactionsAPI and FRoadRuntimeUndoStack (the undo
			//                             history the Transactions API writes into), and
			//                             URoadRuntimeContextObject (the game-side IRoadEditorContext).
			//   Public|Private/Input/     FRoadRuntimeInputBridge, which turns a player controller's
			//                             mouse/key state into FInputDeviceState for
			//                             UInputRouter::PostInputEvent().
			//   Public|Private/Rendering/ FRoadRuntimeRenderAPI and its FPrimitiveDrawInterface adapter
			//                             (S0.5, pre-existing).
			//   Public|Private/UI/        SRoadRuntimePropertyPanel, the stand-in for the editor's
			//                             details view, which a game build cannot have.
			//   RoadToolsWorldSubsystem    owns all of the above and decides when editing runs. It is
			//                             off until a game calls StartEditing().
			//
			// The gizmo note from S0.5 is honoured in URoadToolsContext::InitializeRoadEditing(), which
			// calls UE::TransformGizmoUtil::RegisterTransformGizmoContextObject() on itself the same way
			// URoadToolsMode::Enter() does for the editor host.
		}
	}
}
