# UI/HUD System

The UI system provides heads-up display elements, menus, and in-world UI using egui integration.

## Basic HUD

```toml
[ui.hud]
enabled = true
layout = "ui/layouts/game_hud.toml"
```

## HUD Elements

### Health Bar

```toml
[[ui.hud.elements]]
type = "progress_bar"
id = "health_bar"
anchor = "top_left"
offset = [20, 20]
size = [200, 20]
bind_to = "player.health.current"
bind_max = "player.health.max"
fill_color = [0.8, 0.2, 0.2, 1.0]
background_color = [0.2, 0.2, 0.2, 0.8]
border_color = [0.1, 0.1, 0.1, 1.0]
border_width = 2
show_text = true
text_format = "{value}/{max}"
```

### Text Display

```toml
[[ui.hud.elements]]
type = "text"
id = "score_display"
anchor = "top_right"
offset = [-20, 20]
font = "fonts/game_font.ttf"
font_size = 24
color = [1, 1, 1, 1]
bind_to = "state.score"
format = "Score: {value}"
shadow = true
shadow_offset = [2, 2]
shadow_color = [0, 0, 0, 0.5]
```

### Image/Icon

```toml
[[ui.hud.elements]]
type = "image"
id = "crosshair"
anchor = "center"
offset = [0, 0]
size = [32, 32]
image = "ui/crosshair.png"
tint = [1, 1, 1, 1]
```

### Container/Panel

```toml
[[ui.hud.elements]]
type = "panel"
id = "status_panel"
anchor = "bottom_left"
offset = [20, -20]
size = [250, 100]
background_color = [0, 0, 0, 0.7]
border_radius = 8
padding = [10, 10, 10, 10]

[[ui.hud.elements.children]]
type = "text"
id = "ammo_text"
offset = [0, 0]
bind_to = "player.weapon.ammo"
format = "Ammo: {value}"

[[ui.hud.elements.children]]
type = "text"
id = "weapon_name"
offset = [0, 25]
bind_to = "player.weapon.name"
```

## Anchor Points

| Anchor | Position |
|--------|----------|
| `top_left` | Top-left corner |
| `top_center` | Top-center |
| `top_right` | Top-right corner |
| `center_left` | Middle-left |
| `center` | Center of screen |
| `center_right` | Middle-right |
| `bottom_left` | Bottom-left corner |
| `bottom_center` | Bottom-center |
| `bottom_right` | Bottom-right corner |

## Element Types

### Progress Bar

```toml
[[ui.hud.elements]]
type = "progress_bar"
id = "stamina_bar"
anchor = "top_left"
offset = [20, 50]
size = [150, 12]
direction = "horizontal"         # horizontal, vertical
bind_to = "player.stamina"
bind_max = "player.max_stamina"
fill_color = [0.2, 0.8, 0.2, 1.0]
background_color = [0.1, 0.1, 0.1, 0.8]
segments = 0                     # 0 = smooth, >0 = segmented
gradient = false
gradient_colors = [[1, 0, 0, 1], [1, 1, 0, 1], [0, 1, 0, 1]]
```

### Icon Grid (Inventory Quick Bar)

```toml
[[ui.hud.elements]]
type = "icon_grid"
id = "quickbar"
anchor = "bottom_center"
offset = [0, -20]
cell_size = [50, 50]
columns = 10
spacing = 5
bind_to = "player.inventory.quickbar"
show_quantity = true
show_keybind = true
keybinds = ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"]
selected_border = [1, 1, 0, 1]
empty_color = [0.2, 0.2, 0.2, 0.5]
```

### Minimap

```toml
[[ui.hud.elements]]
type = "minimap"
id = "minimap"
anchor = "top_right"
offset = [-20, 20]
size = [200, 200]
shape = "circle"                 # circle, square
zoom = 1.0
rotate_with_player = true
show_player_icon = true
show_enemies = true
show_objectives = true
show_pickups = false
background_color = [0.1, 0.1, 0.1, 0.8]
border_color = [0.3, 0.3, 0.3, 1.0]
player_color = [0, 1, 0, 1]
enemy_color = [1, 0, 0, 1]
objective_color = [1, 1, 0, 1]
```

### Compass

```toml
[[ui.hud.elements]]
type = "compass"
id = "compass"
anchor = "top_center"
offset = [0, 20]
size = [400, 40]
show_degrees = true
show_markers = true
marker_types = ["quest", "poi", "enemy"]
background_color = [0, 0, 0, 0.5]
```

### Buff/Status Icons

```toml
[[ui.hud.elements]]
type = "status_icons"
id = "buffs"
anchor = "top_left"
offset = [20, 80]
icon_size = [32, 32]
max_icons = 8
direction = "horizontal"
spacing = 5
bind_to = "player.status_effects"
show_duration = true
show_stacks = true
```

## Damage Numbers

```toml
[ui.damage_numbers]
enabled = true
font = "fonts/damage_font.ttf"
font_size = 20
float_speed = 80                 # Pixels per second upward
float_duration = 1.0
fade_start = 0.5                 # Start fading at 50% of duration
scale_by_damage = true
min_scale = 0.8
max_scale = 2.0
randomize_position = true
random_offset = 20

[ui.damage_numbers.colors]
physical = [1, 1, 1, 1]
fire = [1, 0.5, 0, 1]
ice = [0, 0.8, 1, 1]
electric = [1, 1, 0, 1]
poison = [0.5, 1, 0, 1]
heal = [0, 1, 0, 1]
critical = [1, 0.2, 0.2, 1]
```

## Interaction Prompts

```toml
[ui.interaction]
enabled = true
position = "center"              # center, bottom_center
offset = [0, 100]
font_size = 18
background = true
background_color = [0, 0, 0, 0.7]
padding = [15, 10]
show_keybind = true
keybind_style = "bracket"        # bracket, box, none
```

## In-World UI

### Nameplates

```toml
[ui.nameplates]
enabled = true
max_distance = 30.0
fade_distance = 25.0
offset_y = 2.0                   # Above entity
scale_with_distance = true

[ui.nameplates.friendly]
show_name = true
show_health = false
name_color = [0.2, 0.8, 0.2, 1]

[ui.nameplates.enemy]
show_name = true
show_health = true
show_level = true
name_color = [1, 0.2, 0.2, 1]
health_bar_width = 100
health_bar_height = 8

[ui.nameplates.neutral]
show_name = true
show_health = false
name_color = [1, 1, 0, 1]
```

### World Markers

```toml
[[ui.world_markers]]
id = "quest_marker"
type = "objective"
target = "quest_destination"     # Entity name or position
icon = "ui/icons/quest_marker.png"
color = [1, 1, 0, 1]
show_distance = true
min_distance = 5.0               # Hide when close
clamp_to_screen = true           # Show at edge when off-screen
pulse = true
pulse_speed = 2.0
```

## Menus

### Menu Definition

```toml
# ui/menus/pause_menu.toml
[menu]
id = "pause_menu"
title = "Paused"
background = "ui/backgrounds/pause_bg.png"
background_blur = true
pause_game = true

[[menu.items]]
type = "button"
id = "resume"
text = "Resume"
action = "ResumeGame"

[[menu.items]]
type = "button"
id = "settings"
text = "Settings"
action = "OpenMenu"
action_param = "settings_menu"

[[menu.items]]
type = "button"
id = "quit"
text = "Quit to Main Menu"
action = "QuitToMainMenu"
confirm = true
confirm_message = "Are you sure?"
```

### Menu Item Types

```toml
# Button
[[menu.items]]
type = "button"
id = "start"
text = "Start Game"
action = "StartGame"

# Slider
[[menu.items]]
type = "slider"
id = "volume"
label = "Volume"
min = 0.0
max = 1.0
step = 0.1
bind_to = "settings.master_volume"

# Toggle
[[menu.items]]
type = "toggle"
id = "fullscreen"
label = "Fullscreen"
bind_to = "settings.fullscreen"

# Dropdown
[[menu.items]]
type = "dropdown"
id = "resolution"
label = "Resolution"
options = ["1920x1080", "2560x1440", "3840x2160"]
bind_to = "settings.resolution"

# Key Binding
[[menu.items]]
type = "keybind"
id = "jump_key"
label = "Jump"
action = "jump"
bind_to = "input.bindings.jump"

# Separator
[[menu.items]]
type = "separator"

# Submenu
[[menu.items]]
type = "submenu"
id = "graphics"
label = "Graphics Settings"
menu = "ui/menus/graphics_menu.toml"
```

## Widgets

### Dialog Box

```toml
[ui.widgets.dialog]
id = "npc_dialog"
anchor = "bottom_center"
offset = [0, -50]
width = 600
background_color = [0, 0, 0, 0.9]
border_radius = 10
padding = 20
speaker_name_color = [1, 1, 0, 1]
text_color = [1, 1, 1, 1]
text_speed = 30                  # Characters per second
show_portrait = true
portrait_size = [100, 100]
show_choices = true
choice_style = "buttons"         # buttons, list
```

### Notification Toast

```toml
[ui.notifications]
position = "top_center"
offset = [0, 60]
max_visible = 3
duration = 3.0
fade_duration = 0.5
spacing = 10
width = 400
background_color = [0.1, 0.1, 0.1, 0.9]
border_radius = 5

[ui.notifications.types]
info = { icon = "ui/icons/info.png", color = [0.5, 0.5, 1, 1] }
success = { icon = "ui/icons/check.png", color = [0.2, 0.8, 0.2, 1] }
warning = { icon = "ui/icons/warning.png", color = [1, 0.8, 0, 1] }
error = { icon = "ui/icons/error.png", color = [1, 0.2, 0.2, 1] }
achievement = { icon = "ui/icons/trophy.png", color = [1, 0.8, 0, 1] }
```

## UI Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_element_click` | `element_id: string` | UI element clicked |
| `on_element_hover` | `element_id: string, is_hovered: bool` | Hover state change |
| `on_menu_open` | `menu_id: string` | Menu opened |
| `on_menu_close` | `menu_id: string` | Menu closed |
| `on_dialog_choice` | `dialog_id: string, choice_id: string` | Dialog choice made |

## UI Scripting API

### Show/Hide Elements

```cpp
// Show/hide
UI->ShowElement("health_bar");
UI->HideElement("minimap");
UI->SetElementVisible("crosshair", bAiming);

// Enable/disable
UI->SetElementEnabled("interact_prompt", bCanInteract);
```

### Update Values

```cpp
// Direct update (bypasses binding)
UI->SetText("message_text", "Hello World");
UI->SetProgressValue("loading_bar", 0.75f);
UI->SetImage("weapon_icon", "ui/icons/shotgun.png");
```

### Menus

```cpp
// Open/close menus
UI->OpenMenu("pause_menu");
UI->CloseMenu("pause_menu");
UI->CloseAllMenus();

// Check state
bool IsPaused = UI->IsMenuOpen("pause_menu");
```

### Notifications

```cpp
// Show notifications
UI->ShowNotification("Item collected!", ENotificationType::Info);
UI->ShowNotification("Achievement unlocked!", ENotificationType::Achievement, 5.0f);

// Custom notification
FNotification Notif;
Notif.Title = "Level Up!";
Notif.Message = "You reached level 10";
Notif.Icon = "ui/icons/levelup.png";
Notif.Duration = 4.0f;
UI->ShowNotification(Notif);
```

### Dialog

```cpp
// Start dialog
UI->StartDialog("npc_greeting");

// With choices
FDialogOptions Options;
Options.Choices = {
    { "choice_accept", "Accept Quest" },
    { "choice_decline", "Decline" },
    { "choice_more_info", "Tell me more" }
};
UI->ShowDialogWithChoices("quest_offer", Options, [](FString ChoiceId) {
    // Handle choice
});
```

### World Markers

```cpp
// Add marker
UI->AddWorldMarker("quest_01", QuestPosition, "ui/icons/quest.png");

// Update marker
UI->SetMarkerPosition("quest_01", NewPosition);
UI->SetMarkerVisible("quest_01", bShowMarker);

// Remove marker
UI->RemoveWorldMarker("quest_01");
```

## Complete Example

```toml
# Full game HUD configuration

[ui]
[ui.hud]
enabled = true

# Health and shields
[[ui.hud.elements]]
type = "panel"
id = "vitals_panel"
anchor = "top_left"
offset = [20, 20]
size = [220, 70]
background_color = [0, 0, 0, 0.6]
border_radius = 5

[[ui.hud.elements.children]]
type = "progress_bar"
id = "health_bar"
offset = [10, 10]
size = [200, 20]
bind_to = "player.health.current"
bind_max = "player.health.max"
fill_color = [0.8, 0.1, 0.1, 1]
background_color = [0.2, 0, 0, 0.8]
show_text = true

[[ui.hud.elements.children]]
type = "progress_bar"
id = "shield_bar"
offset = [10, 40]
size = [200, 15]
bind_to = "player.shield.current"
bind_max = "player.shield.max"
fill_color = [0.2, 0.5, 0.9, 1]
background_color = [0, 0.1, 0.2, 0.8]

# Ammo display
[[ui.hud.elements]]
type = "panel"
id = "ammo_panel"
anchor = "bottom_right"
offset = [-20, -20]
size = [150, 60]
background_color = [0, 0, 0, 0.6]
border_radius = 5

[[ui.hud.elements.children]]
type = "text"
id = "ammo_current"
offset = [10, 10]
font_size = 32
bind_to = "player.weapon.current_ammo"
color = [1, 1, 1, 1]

[[ui.hud.elements.children]]
type = "text"
id = "ammo_reserve"
offset = [70, 20]
font_size = 18
bind_to = "player.weapon.reserve_ammo"
format = "/ {value}"
color = [0.7, 0.7, 0.7, 1]

# Crosshair
[[ui.hud.elements]]
type = "image"
id = "crosshair"
anchor = "center"
size = [24, 24]
image = "ui/crosshair_dot.png"
tint = [1, 1, 1, 0.8]

# Minimap
[[ui.hud.elements]]
type = "minimap"
id = "minimap"
anchor = "top_right"
offset = [-20, 20]
size = [180, 180]
shape = "circle"
rotate_with_player = true
show_enemies = true
show_objectives = true
background_color = [0, 0, 0, 0.7]

# Quick bar
[[ui.hud.elements]]
type = "icon_grid"
id = "quickbar"
anchor = "bottom_center"
offset = [0, -20]
cell_size = [45, 45]
columns = 5
spacing = 3
bind_to = "player.inventory.quickbar"
show_quantity = true
show_keybind = true

# Buff icons
[[ui.hud.elements]]
type = "status_icons"
id = "status_effects"
anchor = "top_left"
offset = [20, 100]
icon_size = [28, 28]
max_icons = 6
direction = "horizontal"
spacing = 4
bind_to = "player.status_effects"
show_duration = true

# Objective tracker
[[ui.hud.elements]]
type = "panel"
id = "objective_panel"
anchor = "top_right"
offset = [-20, 220]
size = [250, 150]
background_color = [0, 0, 0, 0.5]
border_radius = 5

[[ui.hud.elements.children]]
type = "text"
id = "objective_title"
offset = [10, 10]
font_size = 14
text = "Current Objective"
color = [1, 0.8, 0, 1]

[[ui.hud.elements.children]]
type = "text"
id = "objective_text"
offset = [10, 35]
font_size = 12
bind_to = "quest.current_objective"
color = [1, 1, 1, 1]
wrap_width = 230

# Damage numbers
[ui.damage_numbers]
enabled = true
font_size = 18
float_speed = 60
float_duration = 1.2
scale_by_damage = true

[ui.damage_numbers.colors]
physical = [1, 1, 1, 1]
fire = [1, 0.4, 0, 1]
critical = [1, 1, 0, 1]
heal = [0.2, 1, 0.2, 1]

# Interaction prompts
[ui.interaction]
enabled = true
position = "center"
offset = [0, 80]
font_size = 16
background = true
background_color = [0, 0, 0, 0.8]
padding = [12, 8]
show_keybind = true

# Nameplates
[ui.nameplates]
enabled = true
max_distance = 25.0

[ui.nameplates.enemy]
show_name = true
show_health = true
show_level = true
health_bar_width = 80
health_bar_height = 6

# Notifications
[ui.notifications]
position = "top_center"
offset = [0, 80]
max_visible = 3
duration = 4.0
width = 350
```
