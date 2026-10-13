# The Verse Gallery (Unreal Engine version)

An Unreal Engine 5 C++ conversion of the Unity project **The Verse Gallery** (`Assets/Scenes/my scene.unity`).

It's the same first-person gallery: the room layout from the modular kit, the 10 book podiums with their books and wall posters, the lights, the noon sky, background music, and the voice-over intro. Walking up to a podium opens a panel with that project's title, author, description, screenshots, and a **Visit Website** button.

When you press Play, an **intro camera tour** runs first: the player is frozen while the camera glides to each podium as the voice-over describes it, then the view eases back to the player and you can walk around. Press **Space** or **Enter** to skip it.

---

## What you need on the Unreal machine

| | |
|---|---|
| Unreal Engine | **5.1 or newer** (installed through the Epic Games Launcher) |
| Visual Studio 2022 | Community edition is fine. In the Visual Studio Installer, check the **"Game development with C++"** workload. Unreal needs it to compile this project's C++ code the first time. |
| Disk space | About 5 GB free for the compiled code and imported assets |

---

## First time opening the project

1. **Copy the whole `UnrealVerseGallery` folder** from the thumb drive to the Unreal machine's internal drive, for example `C:\Projects\UnrealVerseGallery`. Unreal is very slow running from a USB stick, and the folder path should not contain spaces.
2. **Double-click `UnrealVerseGallery.uproject`.**
   - If Windows asks which Unreal version to use, pick the version you have installed.
3. Unreal will say **"The following modules are missing or built with a different engine version... Would you like to rebuild them now?"** Click **Yes**. The first compile takes a few minutes.
4. The editor opens. **A progress window titled "Setting up The Verse Gallery..." appears automatically** a few seconds later. It:
   - imports the textures, music, voice-over, and sky image from `GalleryData/` into `Content/Gallery/`
   - creates the gallery materials
   - builds the level and saves it as `Content/Maps/VerseGallery`
5. When it finishes you'll see the gallery in the viewport. Press **Play** (Alt+P).

From then on, just double-click the `.uproject` and press Play. The setup doesn't run again.

> If the progress window never appears, run the setup by hand: **Tools > Execute Python Script...** and choose `Content/Python/gallery_setup.py`.

---

## Controls

| Action | Keyboard / Mouse | Gamepad |
|---|---|---|
| Move | `W` `A` `S` `D` | Left stick |
| Look around | Mouse | Right stick |
| Sprint | `Left Shift` | Left stick press |
| Jump | `Space` | A / Cross |
| Skip the intro tour | `Space` or `Enter` | |
| Close a book panel | `Q` (or click **Close**) | |
| Open the project's website | Click **Visit Website** on the panel | |

> In the editor, `Esc` stops Play mode, so use `Q` to close book panels. In a packaged game `Esc` also closes them.

---

## How the Unity scripts map to C++

All code is in `Source/VerseGallery/`.

| Unity (C#) | Unreal (C++) | What it does |
|---|---|---|
| `GalleryBook.cs` | `GalleryBook.h/.cpp` (`AGalleryBook`) | Invisible sphere trigger at each podium holding the book's title, author, description, URL, and screenshots |
| `BookInteractionUI.cs` | `SGalleryBookPanel.h/.cpp` + `GalleryPlayerController.h/.cpp` | The on-screen book panel, and unlocking the mouse while it's open |
| `BackgroundMusic.cs` | `BackgroundMusicComponent.h/.cpp` | Looping background music |
| `VoiceOverIntro.cs` | `VoiceOverIntroComponent.h/.cpp` | Voice-over 2 seconds after Play starts |
| `GalleryIntroTour.cs` | `GalleryIntroTourComponent.h/.cpp` | Intro camera tour synced to the voice-over, then hands control to the player |
| `SkyboxRotator.cs` | `SkyboxRotatorComponent.h/.cpp` | Slowly spins the sky |
| Starter Assets `FirstPersonController` | `GalleryCharacter.h/.cpp` | Walk 4 m/s, sprint 6 m/s, jump 1.2 m, same as Unity |
| The scene itself | `GalleryBuilder.h/.cpp` (`AGalleryBuilder`) | Rebuilds every mesh, collider, light, and the sky from `GalleryData/gallery.json` |
| (game setup) | `GalleryGameMode.h/.cpp` | Uses the character and controller above and spawns you where the Unity player started |

### Where the scene data comes from

`GalleryData/` was exported straight out of the Unity project:

- `gallery.json` holds every mesh (already converted to Unreal's coordinates and centimeters), every collider, the 14 lights, the 10 books, the player start, the sky settings, the audio settings, and the intro tour (`introTour`).
- `Textures/` holds the materials' textures and the book screenshots.
- `Audio/` holds the background music and voice-over, converted to WAV (Unreal can't import MP3).
- `Sky/` holds the noon HDR sky image.

---

## Tweaking things

Select **GalleryBuilder** in the Outliner. In the Details panel you'll find:

- **Local Light Scale / Directional Light Scale**: brightness of the ceiling lamps and the sun
- **Local Light Saturation**: how purple the lamps are (0 = white, 1 = Unity's full purple). Unity's baked lighting made the room read white, so the default keeps only a slight tint.
- **Local Light Range Scale**: how far the lamps' light reaches past Unity's range, so it gets to the walls
- **Exposure**: brightens or darkens the whole picture (each -1 halves it)
- **Ambient Fill**: even fill light on every surface, like Unity's ambient lighting
- **Sky Light Intensity**: brightness of the sky's light
- **Rebuild**: a button that re-reads `gallery.json` and rebuilds the scene

When the editor opens, the viewport camera moves to the intro tour's first shot (the doorway) so you start inside the gallery.

Book text and links are in `GalleryData/gallery.json` under `"books"`. Edit them, then press **Rebuild**.

### Intro camera tour

The tour's camera spots and timings live in `gallery.json` under `"introTour"`:

- `shots`: one per podium, in voice-over order. `start` is the number of seconds into the voice-over when the camera starts gliding there; `pos` (centimeters), `pitch` and `yaw` (degrees) are the camera spot.
- `handBack`: seconds into the voice-over when the player gets control back.
- `moveSeconds`: how long each glide takes. `pushInSpeed`: slow forward drift while holding a shot (cm/s; 0 holds still).

They were copied from the Unity scene. The easiest way to change them is to adjust the tour in Unity (camera spots under **Gallery_IntroTour > CameraSpots**, **Start Time** on each shot), save the scene, then copy it over again from PowerShell in this folder:

```powershell
powershell -ExecutionPolicy Bypass -File Tools\SyncIntroTourFromUnity.ps1
```

It only reads the Unity scene (default `D:\The Verse Gallery\Assets\Scenes\my scene.unity`; pass `-UnityScene "<path>"` if it's elsewhere) and rewrites the `introTour` line in `gallery.json`. Changes show up the next time you press Play.

The Output Log prints a line like `LogGalleryIntroTour: 34.2s -> Feedback Site` each time the camera changes shot, which helps when matching timings to the voice-over.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Opening a fresh GitHub download compiles C++ first | Normal: compiled files aren't stored in the repository. It takes a few minutes and needs the Visual Studio setup below. |
| "Could not be compiled. Try rebuilding from source manually." | Visual Studio 2022 with the **Game development with C++** workload isn't installed. Install it, then open the `.uproject` again. |
| Everything is plain colored with no textures | The setup script didn't run. Use **Tools > Execute Python Script...** with `Content/Python/gallery_setup.py`. |
| The viewport is empty after opening | Open `Content/Maps/VerseGallery`, or select GalleryBuilder and press **Rebuild**. |
| No music or no voice-over | Check that `Content/Gallery/Audio` has `S_Dreams_of_Highland_Pass` and `S_GalleryIntro_MyVoice`. If not, run the setup script again. New files in `GalleryData` are also imported automatically each time the editor opens. |
| Intro tour doesn't play | Check the Output Log for `LogGalleryIntroTour`. If there's nothing, `gallery.json` has no `introTour` line; run `Tools\SyncIntroTourFromUnity.ps1`. |
| Too dark or too bright | Adjust the light scales on GalleryBuilder (see above). |
| Mouse doesn't look around | Click inside the viewport after pressing Play. |

---

## Known differences from Unity

- Lighting is real-time Unreal lighting (Lumen) instead of Unity's baked lighting, so brightness and shadows look a bit different.
- The rooms are built from procedural meshes at load time, not imported static-mesh assets. They look the same, but you edit them through `gallery.json`, not by dragging individual walls in the level.
- One small book that floats high above the world origin in the Unity scene is carried over as-is.
- The third-party assets (Barking Dog modular kit, Timeless European Strings music, Staggart skybox) come from the Unity Asset Store. Check their licenses before shipping this outside the company.
