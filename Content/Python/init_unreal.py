import unreal

# The setup touches editor-only subsystems and crashes standalone (-game) runs.
if "-game" not in unreal.SystemLibrary.get_command_line().lower():
    try:
        import gallery_setup
        gallery_setup.run_on_first_launch()
    except Exception as exc:
        unreal.log_error("[VerseGallery] Could not schedule first-launch setup: %s" % exc)
