// Dungeon Crawler - Main Game Logic
// Demonstrates C++/Blueprint integration for Void Engine (doc 21)
//
// This file shows how all game systems (22-29) work together:
// - Triggers (void_triggers)
// - Physics (void_physics)
// - Combat (void_combat)
// - Inventory (void_inventory)
// - Audio (void_audio)
// - Game State (void_gamestate)
// - HUD (void_hud)
// - AI (void_ai)

#include "VoidEngine.h"
#include "VoidPhysics.h"
#include "VoidCombat.h"
#include "VoidTriggers.h"
#include "VoidInventory.h"
#include "VoidAudio.h"
#include "VoidGameState.h"
#include "VoidHUD.h"
#include "VoidAI.h"

// =============================================================================
// GAME MANAGER CLASS
// =============================================================================

class DungeonCrawlerGame : public VoidGameMode
{
public:
    // Game state
    bool bBossActive = false;
    Entity* BossEntity = nullptr;

    virtual void OnGameStart() override
    {
        // Initialize game state variables
        GameState->SetInt("score", 0);
        GameState->SetInt("player_gold", 0);
        GameState->SetInt("enemies_killed", 0);
        GameState->SetInt("keys_collected", 0);

        // Load last checkpoint if exists
        FString LastCheckpoint = GameState->GetString("current_checkpoint");
        if (!LastCheckpoint.IsEmpty() && LastCheckpoint != "start")
        {
            RespawnAtCheckpoint(LastCheckpoint);
        }

        // Play ambient music
        Audio->PlayMusic("audio/music/dungeon_ambient.ogg", 2.0f);

        UE_LOG(LogGame, Log, TEXT("Dungeon Crawler started!"));
    }

    virtual void OnGameEnd() override
    {
        // Auto-save on exit
        GameState->Save(0);
    }
};

// =============================================================================
// TRIGGER EVENT HANDLERS
// =============================================================================

// Death zone - instant kill trigger
UFUNCTION()
void OnDeathZone(Trigger* Self, Entity* Other)
{
    if (Other->HasTag("player"))
    {
        // Apply lethal damage
        HealthComponent* Health = Other->GetComponent<HealthComponent>();
        if (Health)
        {
            Health->TakeDamage(9999, EDamageType::True, nullptr);
        }

        Audio->PlaySound("audio/sfx/fall_death.wav");
    }
    else if (Other->HasTag("enemy"))
    {
        // Enemies just die
        Other->Destroy();
    }
}

// Checkpoint trigger
UFUNCTION()
void OnCheckpoint(Trigger* Self, Entity* Other)
{
    if (!Other->HasTag("player")) return;

    FString CheckpointId = Self->GetData<FString>("checkpoint_id");
    FVector SpawnPos = Self->GetData<FVector>("spawn_position");
    FRotator SpawnRot = Self->GetData<FRotator>("spawn_rotation");

    // Save checkpoint
    GameState->SetString("current_checkpoint", CheckpointId);
    GameState->SetVector("checkpoint_position", SpawnPos);
    GameState->SetRotator("checkpoint_rotation", SpawnRot);

    // Auto-save
    GameState->Save(0);

    // Visual/audio feedback
    Audio->PlaySound("audio/sfx/checkpoint.wav");
    HUD->ShowNotification("Checkpoint Reached", ENotificationType::Info, 2.0f);

    UE_LOG(LogGame, Log, TEXT("Checkpoint saved: %s"), *CheckpointId);
}

// Lava damage zone
UFUNCTION()
void OnLavaDamage(Trigger* Self, Entity* Other)
{
    if (!Other->HasTag("damageable")) return;

    HealthComponent* Health = Other->GetComponent<HealthComponent>();
    if (!Health) return;

    float Damage = Self->GetData<float>("damage_per_tick");
    FString DamageType = Self->GetData<FString>("damage_type");

    Health->TakeDamage(Damage, GetDamageType(DamageType), nullptr);

    // Apply burning status effect
    if (Self->GetData<bool>("apply_dot"))
    {
        float Duration = Self->GetData<float>("dot_duration");
        Other->ApplyStatusEffect("burning", Duration);
    }
}

UFUNCTION()
void OnLavaExit(Trigger* Self, Entity* Other)
{
    // Stop burning visual effect (DOT continues)
    if (Other->HasTag("player"))
    {
        HUD->ShowNotification("Escaped the lava!", ENotificationType::Info);
    }
}

// Enemy ambush trigger
UFUNCTION()
void OnAmbushTrigger(Trigger* Self, Entity* Other)
{
    if (!Other->HasTag("player")) return;

    TArray<FString> SpawnPoints = Self->GetData<TArray<FString>>("spawn_points");
    FString EnemyType = Self->GetData<FString>("enemy_type");
    int32 EnemyCount = Self->GetData<int32>("enemy_count");
    float SpawnDelay = Self->GetData<float>("spawn_delay");

    // Play alert sound
    Audio->PlaySound("audio/sfx/ambush_alert.wav");
    HUD->ShowNotification("Ambush!", ENotificationType::Warning, 2.0f);

    // Spawn enemies with delay
    for (int32 i = 0; i < EnemyCount && i < SpawnPoints.Num(); i++)
    {
        Entity* SpawnPoint = World->FindEntity(SpawnPoints[i]);
        if (SpawnPoint)
        {
            // Delayed spawn
            TimerManager->SetTimer(
                [=]() {
                    Entity* Enemy = World->SpawnEntity(EnemyType, SpawnPoint->GetTransform());
                    Enemy->GetComponent<AIComponent>()->SetTarget(Other);
                },
                SpawnDelay * i
            );
        }
    }
}

// Boss arena trigger
UFUNCTION()
void OnBossArenaEnter(Trigger* Self, Entity* Other)
{
    if (!Other->HasTag("player")) return;

    FString BossEntityName = Self->GetData<FString>("boss_entity");
    bool bCloseExits = Self->GetData<bool>("close_exits");
    FString BossMusic = Self->GetData<FString>("boss_music");

    // Activate boss
    Entity* Boss = World->FindEntity(BossEntityName);
    if (Boss)
    {
        Boss->SetEnabled(true);
        Boss->GetComponent<AIComponent>()->SetTarget(Other);

        GameMode->bBossActive = true;
        GameMode->BossEntity = Boss;

        // Show boss health bar
        HUD->SetVariable("boss_active", true);
        HUD->SetVariable("boss.name", Boss->GetDisplayName());
    }

    // Close arena exits
    if (bCloseExits)
    {
        TArray<Entity*> Doors = World->FindEntitiesWithTag("arena_door");
        for (Entity* Door : Doors)
        {
            Door->GetComponent<PhysicsComponent>()->SetEnabled(true);
            // Play door slam animation/sound
            Audio->PlaySoundAtLocation("audio/sfx/door_slam.wav", Door->GetLocation());
        }
    }

    // Switch to boss music
    Audio->PlayMusic(BossMusic, 1.0f);

    HUD->ShowNotification("DEMON LORD AWAKENS", ENotificationType::Boss, 3.0f);
}

// =============================================================================
// PICKUP EVENT HANDLERS
// =============================================================================

UFUNCTION()
void OnHealthPickup(Entity* Collector, FString ItemId, int32 Quantity)
{
    // Heal player immediately
    HealthComponent* Health = Collector->GetComponent<HealthComponent>();
    if (Health)
    {
        Health->Heal(50); // Health potion heals 50
    }

    Audio->PlaySound("audio/sfx/heal.wav");
    HUD->ShowDamageNumber(Collector->GetLocation(), 50, EDamageNumberType::Heal);
}

UFUNCTION()
void OnKeyPickup(Entity* Collector, FString ItemId, int32 Quantity)
{
    // Increment key counter
    int32 Keys = GameState->GetInt("keys_collected") + 1;
    GameState->SetInt("keys_collected", Keys);

    Audio->PlaySound("audio/sfx/key_pickup.wav");
    HUD->ShowNotification("Gold Key acquired!", ENotificationType::Item, 3.0f);

    // Check if this unlocks something
    if (ItemId == "gold_key")
    {
        // Enable the locked door interaction
        Entity* Door = World->FindEntity("locked_door");
        if (Door)
        {
            Door->SetState("is_locked", false);
        }
    }
}

// =============================================================================
// COMBAT EVENT HANDLERS
// =============================================================================

UFUNCTION()
void OnPlayerDamage(float Amount, EDamageType Type, Entity* Source)
{
    // Screen flash effect
    HUD->FlashScreen(FLinearColor(1, 0, 0, 0.3f), 0.2f);

    // Camera shake
    CameraManager->PlayCameraShake(0.3f, 5.0f);

    // Play hurt sound
    Audio->PlaySound("audio/sfx/player_hurt.wav");

    // Show damage number
    Entity* Player = World->GetPlayerEntity();
    HUD->ShowDamageNumber(Player->GetLocation(), Amount, GetDamageNumberType(Type));
}

UFUNCTION()
void OnPlayerHeal(float Amount, Entity* Source)
{
    Entity* Player = World->GetPlayerEntity();
    HUD->ShowDamageNumber(Player->GetLocation(), Amount, EDamageNumberType::Heal);

    // Green flash
    HUD->FlashScreen(FLinearColor(0, 1, 0, 0.2f), 0.3f);
}

UFUNCTION()
void OnPlayerDeath(Entity* Killer)
{
    // Show death screen
    HUD->SetVariable("player.is_dead", true);

    // Play death sound
    Audio->PlaySound("audio/sfx/player_death.wav");

    // Slow motion effect
    World->SetTimeDilation(0.3f);

    // Allow respawn after delay
    TimerManager->SetTimer(
        [=]() {
            World->SetTimeDilation(1.0f);
            // Player can now press SPACE to respawn
        },
        2.0f
    );
}

UFUNCTION()
void OnEnemyDamage(float Amount, EDamageType Type, Entity* Source)
{
    Entity* Enemy = GetContextEntity();

    // Show damage number
    HUD->ShowDamageNumber(Enemy->GetLocation(), Amount, GetDamageNumberType(Type));

    // Alert nearby enemies
    AIComponent* AI = Enemy->GetComponent<AIComponent>();
    if (AI && Source)
    {
        AI->AlertNearbyAllies(Source, 15.0f);
    }
}

UFUNCTION()
void OnEnemyDeath(Entity* Killer)
{
    Entity* Enemy = GetContextEntity();

    // Drop loot
    DropLoot(Enemy);

    // Update score
    int32 Score = GameState->GetInt("score") + GetEnemyScoreValue(Enemy);
    GameState->SetInt("score", Score);

    int32 Kills = GameState->GetInt("enemies_killed") + 1;
    GameState->SetInt("enemies_killed", Kills);

    // Play death effects
    Audio->PlaySoundAtLocation("audio/sfx/enemy_death.wav", Enemy->GetLocation());
    World->SpawnEffect("effects/enemy_death.toml", Enemy->GetTransform());

    // Spawn bone fragments (crafting material)
    if (Enemy->HasTag("skeleton"))
    {
        SpawnPickup("bone_fragment", Enemy->GetLocation(), FMath::RandRange(1, 3));
    }
}

UFUNCTION()
void OnBossDamage(float Amount, EDamageType Type, Entity* Source)
{
    Entity* Boss = GetContextEntity();
    HealthComponent* Health = Boss->GetComponent<HealthComponent>();

    float HealthPercent = Health->GetHealthPercent();

    // Phase transitions
    if (HealthPercent < 0.6f && !Boss->GetState<bool>("phase2_triggered"))
    {
        Boss->SetState("phase2_triggered", true);
        HUD->ShowNotification("The Demon Lord grows stronger!", ENotificationType::Warning);
        Audio->PlaySound("audio/sfx/boss_enrage.wav");
    }
    else if (HealthPercent < 0.25f && !Boss->GetState<bool>("phase3_triggered"))
    {
        Boss->SetState("phase3_triggered", true);
        HUD->ShowNotification("The Demon Lord is ENRAGED!", ENotificationType::Danger);
        Audio->PlaySound("audio/sfx/boss_final_phase.wav");

        // Screen shake
        CameraManager->PlayCameraShake(1.0f, 10.0f);
    }
}

UFUNCTION()
void OnBossDefeated(Entity* Killer)
{
    Entity* Boss = GetContextEntity();

    // Victory!
    GameState->SetBool("boss_defeated", true);
    GameMode->bBossActive = false;

    // Hide boss health bar
    HUD->SetVariable("boss_active", false);

    // Epic death sequence
    World->SetTimeDilation(0.2f);

    // Drop legendary loot
    SpawnPickup("demon_essence", Boss->GetLocation(), 3);
    SpawnPickup("demon_armor", Boss->GetLocation(), 1);
    SpawnPickup("gold_coin", Boss->GetLocation(), 500);

    // Victory fanfare
    Audio->StopMusic(1.0f);
    Audio->PlaySound("audio/sfx/victory_fanfare.wav");

    HUD->ShowNotification("DEMON LORD DEFEATED!", ENotificationType::Victory, 5.0f);

    // Add massive score bonus
    int32 Score = GameState->GetInt("score") + 10000;
    GameState->SetInt("score", Score);

    // Open exit
    Entity* Exit = World->FindEntity("level_exit");
    if (Exit)
    {
        Exit->SetEnabled(true);
    }

    // Restore time after delay
    TimerManager->SetTimer(
        [=]() {
            World->SetTimeDilation(1.0f);
            Audio->PlayMusic("audio/music/victory.ogg", 2.0f);
        },
        3.0f
    );
}

// =============================================================================
// INTERACTABLE HANDLERS
// =============================================================================

UFUNCTION()
void OnDoorInteract(Entity* Door, Entity* Interactor)
{
    bool bIsLocked = Door->GetState<bool>("is_locked");
    FString RequiredKey = Door->GetState<FString>("required_key");

    if (bIsLocked)
    {
        // Check if player has the key
        InventoryComponent* Inventory = Interactor->GetComponent<InventoryComponent>();
        if (Inventory && Inventory->HasItem(RequiredKey))
        {
            // Consume key and unlock
            Inventory->RemoveItem(RequiredKey, 1);
            Door->SetState("is_locked", false);

            Audio->PlaySound("audio/sfx/door_unlock.wav");
            HUD->ShowNotification("Door unlocked!", ENotificationType::Info);

            // Open the door
            OpenDoor(Door);
        }
        else
        {
            Audio->PlaySound("audio/sfx/door_locked.wav");
            HUD->ShowNotification("This door requires a Gold Key", ENotificationType::Warning);
        }
    }
    else
    {
        // Toggle open/close
        bool bIsOpen = Door->GetState<bool>("is_open");
        if (bIsOpen)
        {
            CloseDoor(Door);
        }
        else
        {
            OpenDoor(Door);
        }
    }
}

void OpenDoor(Entity* Door)
{
    Door->SetState("is_open", true);
    Door->PlayAnimation("open");

    // Disable collision
    PhysicsComponent* Physics = Door->GetComponent<PhysicsComponent>();
    if (Physics)
    {
        Physics->SetCollisionEnabled(false);
    }

    Audio->PlaySoundAtLocation("audio/sfx/door_open.wav", Door->GetLocation());
}

void CloseDoor(Entity* Door)
{
    Door->SetState("is_open", false);
    Door->PlayAnimation("close");

    // Enable collision
    PhysicsComponent* Physics = Door->GetComponent<PhysicsComponent>();
    if (Physics)
    {
        Physics->SetCollisionEnabled(true);
    }

    Audio->PlaySoundAtLocation("audio/sfx/door_close.wav", Door->GetLocation());
}

UFUNCTION()
void OnChestOpen(Entity* Chest, Entity* Interactor)
{
    if (Chest->GetState<bool>("looted")) return;

    Chest->SetState("is_open", true);
    Chest->SetState("looted", true);
    Chest->PlayAnimation("open");

    // Get items from container
    ContainerComponent* Container = Chest->GetComponent<ContainerComponent>();
    InventoryComponent* PlayerInv = Interactor->GetComponent<InventoryComponent>();

    if (Container && PlayerInv)
    {
        TArray<FItemStack> Items = Container->GetAllItems();
        for (const FItemStack& Item : Items)
        {
            if (PlayerInv->AddItem(Item.ItemId, Item.Quantity))
            {
                HUD->ShowNotification(FString::Printf(TEXT("Acquired %s x%d"),
                    *GetItemName(Item.ItemId), Item.Quantity), ENotificationType::Item);
            }
            else
            {
                // Inventory full, spawn on ground
                SpawnPickup(Item.ItemId, Chest->GetLocation() + FVector(0, 0, 50), Item.Quantity);
            }
        }
    }

    Audio->PlaySoundAtLocation("audio/sfx/chest_open.wav", Chest->GetLocation());
    World->SpawnEffect("effects/chest_sparkle.toml", Chest->GetTransform());
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void RespawnAtCheckpoint(const FString& CheckpointId)
{
    Entity* Player = World->GetPlayerEntity();

    FVector SpawnPos = GameState->GetVector("checkpoint_position");
    FRotator SpawnRot = GameState->GetRotator("checkpoint_rotation");

    Player->SetLocation(SpawnPos);
    Player->SetRotation(SpawnRot);

    // Restore health
    HealthComponent* Health = Player->GetComponent<HealthComponent>();
    if (Health)
    {
        Health->SetHealth(Health->GetMaxHealth());
    }

    // Hide death screen
    HUD->SetVariable("player.is_dead", false);

    Audio->PlaySound("audio/sfx/respawn.wav");
}

void DropLoot(Entity* Enemy)
{
    // Random loot drops based on enemy type
    FVector Location = Enemy->GetLocation();

    // Always drop some gold
    int32 GoldAmount = FMath::RandRange(5, 20);
    SpawnPickup("gold_coin", Location, GoldAmount);

    // Chance for health drop
    if (FMath::FRand() < 0.3f)
    {
        SpawnPickup("health_potion", Location + FVector(50, 0, 0), 1);
    }
}

void SpawnPickup(const FString& ItemId, FVector Location, int32 Quantity)
{
    Entity* Pickup = World->SpawnEntity("pickup_template", FTransform(Location));

    PickupComponent* PickupComp = Pickup->GetComponent<PickupComponent>();
    if (PickupComp)
    {
        PickupComp->SetItem(ItemId, Quantity);
    }

    // Add upward impulse for "pop" effect
    PhysicsComponent* Physics = Pickup->GetComponent<PhysicsComponent>();
    if (Physics)
    {
        Physics->AddImpulse(FVector(
            FMath::RandRange(-100.0f, 100.0f),
            200.0f,
            FMath::RandRange(-100.0f, 100.0f)
        ));
    }
}

int32 GetEnemyScoreValue(Entity* Enemy)
{
    if (Enemy->HasTag("boss")) return 5000;
    if (Enemy->HasTag("elite")) return 500;
    return 100;
}

EDamageNumberType GetDamageNumberType(EDamageType Type)
{
    switch (Type)
    {
        case EDamageType::Fire: return EDamageNumberType::Fire;
        case EDamageType::Ice: return EDamageNumberType::Ice;
        case EDamageType::Poison: return EDamageNumberType::Poison;
        default: return EDamageNumberType::Physical;
    }
}

// =============================================================================
// INPUT HANDLERS
// =============================================================================

UFUNCTION()
void OnPlayerInputAction(const FString& Action)
{
    Entity* Player = World->GetPlayerEntity();

    if (Action == "Respawn" && HUD->GetVariable<bool>("player.is_dead"))
    {
        FString Checkpoint = GameState->GetString("current_checkpoint");
        RespawnAtCheckpoint(Checkpoint);
    }
    else if (Action == "QuickSave")
    {
        GameState->Save(0);
        HUD->ShowNotification("Game Saved", ENotificationType::Info, 2.0f);
        Audio->PlaySound("audio/sfx/save.wav");
    }
    else if (Action == "QuickLoad")
    {
        if (GameState->Load(0))
        {
            HUD->ShowNotification("Game Loaded", ENotificationType::Info, 2.0f);
        }
    }
    else if (Action == "UseQuickSlot1")
    {
        UseQuickSlot(Player, 0);
    }
    else if (Action == "UseQuickSlot2")
    {
        UseQuickSlot(Player, 1);
    }
    // ... etc
}

void UseQuickSlot(Entity* Player, int32 SlotIndex)
{
    InventoryComponent* Inventory = Player->GetComponent<InventoryComponent>();
    if (!Inventory) return;

    FItemStack Item = Inventory->GetQuickSlotItem(SlotIndex);
    if (Item.IsValid())
    {
        Inventory->UseItem(Item.ItemId);
    }
}
