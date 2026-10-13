using UnrealBuildTool;

public class UnrealVerseGalleryEditorTarget : TargetRules
{
	public UnrealVerseGalleryEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("VerseGallery");
	}
}
