# FPS Power-Up Demo

A small first-person shooter arena demonstrating the power-up system in Void GUI v2.

## Overview

This demo showcases:

- **First-person player** with plasma rifle weapon
- **Power Surge Core** pickup that dramatically enhances your weapon
- **Target dummies** to destroy for points
- **Full HUD** with health, ammo, score, and power-up timer
- **Physics** for collision and movement
- **Audio** including spatial sounds and music
- **Visual feedback** with damage numbers, muzzle flashes, and tracers

## Gameplay

1. **Move** with WASD, **jump** with Space, **sprint** with Shift
2. **Shoot** with Left Mouse, **aim** with Right Mouse, **reload** with R
3. Walk over the **golden Power Core** in the center of the arena
4. Watch your weapon transform - gold tracers, 2.5x damage, penetration!
5. Destroy target dummies before the 15-second buff expires
6. The Power Core respawns after 25 seconds

## Power Surge Effects

| Stat | Normal | Powered | Change |
|------|--------|---------|--------|
| Damage | 20 | 50 | +150% |
| Fire Rate | 8/sec | 10/sec | +25% |
| Spread | 1.2° | 0.5° | -58% |
| Headshot | 2.0x | 3.0x | +50% |
| Penetration | 0 | 2 targets | NEW |
| Reload | 1.8s | 1.2s | -33% |
| Tracer | Blue | Gold | Visual |

## Files Structure

```
fps_powerup_demo/
├── scene.toml              # Main scene definition
├── README.md               # This file
├── items/
│   ├── power_surge_core.toml   # Power-up item
│   ├── plasma_ammo.toml        # Ammo pickup
│   └── health_small.toml       # Health pickup
├── weapons/
│   ├── plasma_rifle.toml       # Standard weapon
│   └── plasma_rifle_powered.toml # Enhanced weapon
├── blueprints/
│   └── BP_PlayerPowerUp.toml   # Power-up logic
└── loot_tables/
    └── (empty - for future use)
```

## Systems Demonstrated

### From 21-scripting.md
- C++ class attachment (`FPSPlayerController`)
- Blueprint visual scripting (`BP_PlayerPowerUp`)
- Event handlers (`OnPowerCoreCollected`, `OnTargetDestroyed`)

### From 22-triggers.md
- Trigger volume for power core area
- Layer mask filtering (player only)

### From 23-physics.md
- Character controller for player movement
- Static colliders for arena walls
- Collision layers (player, enemies, terrain, projectiles)

### From 24-combat.md
- Health component on targets
- Damage types (`energy`, `energy_powered`)
- Weapon definitions (hitscan)
- Status effects (`power_surge` buff)

### From 25-inventory.md
- Item definitions
- Pickup entities with auto-collect
- Respawning pickups

### From 26-audio.md
- Spatial audio on pickups
- Weapon sounds
- Background music
- Sound effects on events

### From 27-state.md
- Game variables (score, kills)
- Statistics tracking

### From 28-ui-hud.md
- Health bar
- Ammo counter
- Score display
- Power-up timer
- Damage numbers
- Crosshair
- Notifications

## Controls

| Key | Action |
|-----|--------|
| W/A/S/D | Move |
| Space | Jump |
| Shift | Sprint |
| Mouse | Look |
| Left Click | Fire |
| Right Click | Aim |
| R | Reload |
| E | Interact |

## Tips

- The boss target (in the back) has 300 HP - save your Power Surge for it!
- Powered shots penetrate through 2 targets - line them up!
- Ammo and health pickups respawn faster than the Power Core
- Watch the golden timer bar to know when your buff is ending

---

*Created as an example for Void GUI v2 Editor*
