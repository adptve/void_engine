# Audio System

The audio system provides spatial sound effects, music playback, and environmental audio using the rodio backend.

## Basic Audio

### Sound on Entity

```toml
[[entities]]
name = "torch"
mesh = "torch.glb"

[entities.audio]
[entities.audio.ambient]
clip = "sounds/fire_crackle.wav"
volume = 0.8
loop = true
play_on_start = true
```

## Audio Source Configuration

```toml
[[entities]]
name = "speaker"

[entities.audio]
# Ambient/looping sound
[entities.audio.ambient]
clip = "sounds/ambient_hum.wav"
volume = 1.0
pitch = 1.0
loop = true
play_on_start = true
spatial = true                   # 3D positioned audio
min_distance = 1.0               # Full volume within this
max_distance = 30.0              # Silent beyond this
rolloff = "logarithmic"          # linear, logarithmic, custom

# One-shot sounds (triggered by events/scripts)
[entities.audio.sounds]
activate = "sounds/switch_on.wav"
deactivate = "sounds/switch_off.wav"
alert = "sounds/alarm.wav"
```

## Spatial Audio

### Distance Attenuation

```toml
[entities.audio.ambient]
clip = "sounds/waterfall.wav"
spatial = true
min_distance = 2.0               # Full volume radius
max_distance = 50.0              # Fade to silence
rolloff = "logarithmic"          # Realistic falloff

# Custom rolloff curve
[entities.audio.ambient.custom_rolloff]
points = [
    [0.0, 1.0],                  # Distance 0, volume 100%
    [5.0, 0.8],                  # Distance 5, volume 80%
    [20.0, 0.3],                 # Distance 20, volume 30%
    [50.0, 0.0]                  # Distance 50, silent
]
```

### Rolloff Modes

| Mode | Description |
|------|-------------|
| `linear` | Linear falloff from min to max distance |
| `logarithmic` | Realistic inverse-square falloff |
| `custom` | User-defined curve |
| `none` | No distance attenuation |

### Directional Audio

```toml
[entities.audio.ambient]
clip = "sounds/speaker_announcement.wav"
spatial = true
directional = true
cone_inner_angle = 45            # Full volume cone
cone_outer_angle = 90            # Attenuated outside inner
cone_outer_volume = 0.2          # Volume at outer edge
direction = [0, 0, 1]            # Forward direction
```

## Scene-Level Audio

### Global Audio Settings

```toml
[audio]
master_volume = 1.0
music_volume = 0.7
sfx_volume = 1.0
ambient_volume = 0.8
voice_volume = 1.0
```

### Background Music

```toml
[audio.music]
track = "music/exploration.ogg"
volume = 0.6
loop = true
fade_in = 2.0                    # Seconds to fade in
crossfade = 3.0                  # Crossfade when switching

# Playlist mode
[audio.music.playlist]
mode = "shuffle"                 # sequential, shuffle, random
tracks = [
    "music/ambient_01.ogg",
    "music/ambient_02.ogg",
    "music/ambient_03.ogg"
]
gap = 2.0                        # Seconds between tracks
```

### Ambient Soundscape

```toml
[audio.ambient]
# Layer multiple ambient sounds
[[audio.ambient.layers]]
clip = "sounds/wind.wav"
volume = 0.3
loop = true

[[audio.ambient.layers]]
clip = "sounds/birds.wav"
volume = 0.2
loop = true
random_delay_min = 5.0           # Random gaps
random_delay_max = 15.0

[[audio.ambient.layers]]
clip = "sounds/distant_thunder.wav"
volume = 0.4
loop = false
random_delay_min = 30.0
random_delay_max = 120.0
```

## Reverb Zones

Define areas with different acoustic properties:

```toml
[[audio.reverb_zones]]
name = "cave_reverb"
shape = "box"
bounds = { min = [-20, 0, -20], max = [20, 10, 20] }
preset = "cave"

[[audio.reverb_zones]]
name = "outdoor"
shape = "box"
bounds = { min = [50, 0, 50], max = [150, 30, 150] }
preset = "outdoor"

[[audio.reverb_zones]]
name = "custom_hall"
shape = "sphere"
center = [0, 5, 100]
radius = 30.0

[audio.reverb_zones.parameters]
room_size = 0.8
damping = 0.5
wet_level = 0.4
dry_level = 0.6
decay_time = 2.5
pre_delay = 0.02
```

### Reverb Presets

| Preset | Description |
|--------|-------------|
| `none` | No reverb |
| `room` | Small room |
| `hall` | Large hall |
| `cave` | Echoey cave |
| `outdoor` | Open outdoor |
| `underwater` | Muffled underwater |
| `metal` | Metallic industrial |

## Audio Triggers

Play sounds on events:

```toml
[[entities]]
name = "door"

[entities.audio]
[entities.audio.sounds]
open = "sounds/door_open.wav"
close = "sounds/door_close.wav"
locked = "sounds/door_locked.wav"
unlock = "sounds/door_unlock.wav"

[entities.blueprint.events]
on_interact = "OnDoorInteract"
# Blueprint plays appropriate sound based on door state
```

## Sound Effects

### Pitch Variation

```toml
[entities.audio.sounds]
footstep = {
    clip = "sounds/footstep.wav",
    volume = 0.8,
    pitch_min = 0.9,
    pitch_max = 1.1              # Random pitch variation
}
```

### Random Clips

```toml
[entities.audio.sounds]
impact = {
    clips = [
        "sounds/impact_01.wav",
        "sounds/impact_02.wav",
        "sounds/impact_03.wav"
    ],
    volume = 1.0,
    mode = "random"              # random, sequential, shuffle
}
```

### Sound Groups

```toml
[audio.sound_groups]
[audio.sound_groups.footsteps_concrete]
clips = [
    "sounds/footstep_concrete_01.wav",
    "sounds/footstep_concrete_02.wav",
    "sounds/footstep_concrete_03.wav",
    "sounds/footstep_concrete_04.wav"
]
volume = 0.7
pitch_min = 0.95
pitch_max = 1.05
cooldown = 0.3

[audio.sound_groups.footsteps_grass]
clips = [
    "sounds/footstep_grass_01.wav",
    "sounds/footstep_grass_02.wav"
]
volume = 0.5
pitch_min = 0.9
pitch_max = 1.1
```

## Audio Mixer

### Mix Channels

```toml
[audio.mixer]
[audio.mixer.channels]
master = { volume = 1.0 }
music = { volume = 0.7, parent = "master" }
sfx = { volume = 1.0, parent = "master" }
ambient = { volume = 0.8, parent = "master" }
voice = { volume = 1.0, parent = "master" }
ui = { volume = 0.9, parent = "sfx" }
weapons = { volume = 1.0, parent = "sfx" }
footsteps = { volume = 0.8, parent = "sfx" }
```

### Sound Priority

```toml
[entities.audio.sounds]
critical_alert = {
    clip = "sounds/alert.wav",
    priority = 100,              # Higher = more important
    ducking = true               # Reduce other sounds
}
```

## Audio Events

| Event | Description |
|-------|-------------|
| `on_sound_start` | Sound began playing |
| `on_sound_end` | Sound finished |
| `on_music_beat` | Music beat marker (if defined) |
| `on_music_end` | Music track ended |

## Audio Scripting API

### Playing Sounds

```cpp
// Play at entity
Entity->PlaySound("sounds/explosion.wav");
Entity->PlaySoundWithParams("activate", 0.8f, 1.2f); // volume, pitch

// Play at position
Audio->PlaySoundAtLocation("sounds/explosion.wav", Position, Volume);

// Play 2D (non-spatial)
Audio->PlaySound2D("sounds/ui_click.wav");

// From sound group
Audio->PlaySoundGroup("footsteps_concrete", Position);
```

### Music Control

```cpp
// Play music
Audio->PlayMusic("music/battle.ogg", FadeInTime);

// Stop music
Audio->StopMusic(FadeOutTime);

// Crossfade to new track
Audio->CrossfadeMusic("music/victory.ogg", CrossfadeTime);

// Queue next track
Audio->QueueMusic("music/ambient.ogg");

// Pause/resume
Audio->PauseMusic();
Audio->ResumeMusic();
```

### Volume Control

```cpp
// Channel volumes
Audio->SetChannelVolume("music", 0.5f);
Audio->SetChannelVolume("sfx", 1.0f);

// Master volume
Audio->SetMasterVolume(0.8f);

// Get volume
float MusicVol = Audio->GetChannelVolume("music");
```

### Sound Handles

```cpp
// Play with handle for control
FSoundHandle Handle = Audio->PlaySound("sounds/alarm.wav");

// Control playback
Handle.SetVolume(0.5f);
Handle.SetPitch(1.5f);
Handle.Pause();
Handle.Resume();
Handle.Stop();

// Check status
bool Playing = Handle.IsPlaying();
float Time = Handle.GetPlaybackPosition();
```

### Listener

```cpp
// Set listener position (usually camera)
Audio->SetListenerPosition(CameraPosition);
Audio->SetListenerRotation(CameraRotation);

// Or attach to entity
Audio->AttachListenerToEntity(CameraEntity);
```

## Audio Snapshots

Predefined audio states:

```toml
[audio.snapshots]
[audio.snapshots.normal]
music = 0.7
sfx = 1.0
ambient = 0.8

[audio.snapshots.combat]
music = 0.9
sfx = 1.0
ambient = 0.4

[audio.snapshots.underwater]
music = 0.4
sfx = 0.6
ambient = 0.3
lowpass_frequency = 800

[audio.snapshots.paused]
music = 0.3
sfx = 0.0
ambient = 0.0
```

Apply snapshots:

```cpp
Audio->TransitionToSnapshot("combat", 1.0f); // 1 second transition
```

## Complete Example

```toml
# Scene with comprehensive audio

[audio]
master_volume = 1.0
music_volume = 0.6
sfx_volume = 1.0
ambient_volume = 0.7

# Background music
[audio.music]
track = "music/forest_ambient.ogg"
volume = 0.5
loop = true
fade_in = 3.0

# Ambient soundscape
[[audio.ambient.layers]]
clip = "sounds/ambient/wind_leaves.wav"
volume = 0.4
loop = true

[[audio.ambient.layers]]
clip = "sounds/ambient/birds_chirp.wav"
volume = 0.25
loop = true
random_delay_min = 3.0
random_delay_max = 10.0

# Sound groups
[audio.sound_groups]
[audio.sound_groups.footsteps_dirt]
clips = [
    "sounds/footsteps/dirt_01.wav",
    "sounds/footsteps/dirt_02.wav",
    "sounds/footsteps/dirt_03.wav"
]
volume = 0.6
pitch_min = 0.95
pitch_max = 1.05

[audio.sound_groups.sword_swing]
clips = [
    "sounds/combat/sword_swing_01.wav",
    "sounds/combat/sword_swing_02.wav"
]
volume = 0.8
pitch_min = 0.9
pitch_max = 1.1

# Mixer channels
[audio.mixer.channels]
master = { volume = 1.0 }
music = { volume = 0.6, parent = "master" }
sfx = { volume = 1.0, parent = "master" }
ambient = { volume = 0.7, parent = "master" }
combat = { volume = 1.0, parent = "sfx" }
ui = { volume = 0.8, parent = "sfx" }

# Reverb zones
[[audio.reverb_zones]]
name = "cave_entrance"
shape = "box"
bounds = { min = [80, 0, 40], max = [120, 20, 80] }
preset = "cave"

# Snapshots
[audio.snapshots]
[audio.snapshots.exploration]
music = 0.6
ambient = 0.8
combat = 0.7

[audio.snapshots.combat]
music = 0.8
ambient = 0.3
combat = 1.0

# Entities with audio

# Waterfall with spatial audio
[[entities]]
name = "waterfall"
mesh = "environment/waterfall.glb"

[entities.transform]
position = [50, 10, 30]

[entities.audio.ambient]
clip = "sounds/environment/waterfall.wav"
volume = 1.0
loop = true
spatial = true
min_distance = 5.0
max_distance = 50.0
rolloff = "logarithmic"

# Torch with fire sound
[[entities]]
name = "torch_01"
mesh = "props/torch.glb"

[entities.transform]
position = [10, 2, 0]

[entities.audio.ambient]
clip = "sounds/environment/fire_crackle.wav"
volume = 0.6
loop = true
spatial = true
min_distance = 1.0
max_distance = 10.0

# Door with sound effects
[[entities]]
name = "wooden_door"
mesh = "props/door_wooden.glb"

[entities.transform]
position = [25, 1.5, 0]

[entities.audio.sounds]
open = "sounds/objects/door_open_wood.wav"
close = "sounds/objects/door_close_wood.wav"
locked = "sounds/objects/door_locked.wav"

# Player with footstep sounds
[[entities]]
name = "player"
mesh = "characters/player.glb"

[entities.audio]
# Character audio handled in C++/Blueprint
# Footsteps based on surface type
# Combat sounds
# Voice/grunts

# Music trigger zone
[[triggers]]
name = "combat_music_trigger"
shape = "sphere"
radius = 20.0
position = [100, 0, 100]
layer_mask = ["player"]
on_enter = "StartCombatMusic"
on_exit = "EndCombatMusic"

[triggers.data]
combat_track = "music/combat_intense.ogg"
crossfade_time = 1.5
```
