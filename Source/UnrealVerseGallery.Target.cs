using UnrealBuildTool;

public class UnrealVerseGalleryTarget : TargetRules
{
	public UnrealVerseGalleryTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("VerseGallery");
	}
}
