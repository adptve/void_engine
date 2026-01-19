/**
 * Void Engine C++ API
 *
 * This header provides the interface for creating C++ game logic classes
 * that can be loaded by the Void Engine at runtime.
 *
 * Usage:
 *   1. Include this header in your C++ project
 *   2. Inherit from VoidActor or VoidComponent
 *   3. Implement lifecycle methods (BeginPlay, Tick, etc.)
 *   4. Export your class using VOID_EXPORT_CLASS macro
 *   5. Compile to a shared library (.dll/.so/.dylib)
 *
 * Example:
 *   class PlayerController : public VoidActor {
 *   public:
 *       float MaxHealth = 100.0f;
 *       float MoveSpeed = 5.0f;
 *
 *       void BeginPlay() override {
 *           // Initialization code
 *       }
 *
 *       void Tick(float DeltaTime) override {
 *           // Per-frame logic
 *       }
 *   };
 *
 *   VOID_EXPORT_CLASS(PlayerController)
 */

#ifndef VOID_API_H
#define VOID_API_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

// ============================================================================
// Platform Macros
// ============================================================================

#ifdef _WIN32
    #define VOID_EXPORT __declspec(dllexport)
    #define VOID_IMPORT __declspec(dllimport)
    #define VOID_CALL __cdecl
#else
    #define VOID_EXPORT __attribute__((visibility("default")))
    #define VOID_IMPORT
    #define VOID_CALL
#endif

#ifdef VOID_BUILDING_LIBRARY
    #define VOID_API VOID_EXPORT
#else
    #define VOID_API VOID_IMPORT
#endif

// ============================================================================
// API Version
// ============================================================================

#define VOID_CPP_API_VERSION 1

// ============================================================================
// Basic Types
// ============================================================================

/// 3D Vector
struct FVector {
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;

    FVector() = default;
    FVector(float x, float y, float z) : X(x), Y(y), Z(z) {}

    static const FVector Zero;
    static const FVector One;
    static const FVector Up;
    static const FVector Forward;
    static const FVector Right;

    FVector operator+(const FVector& Other) const {
        return FVector(X + Other.X, Y + Other.Y, Z + Other.Z);
    }

    FVector operator-(const FVector& Other) const {
        return FVector(X - Other.X, Y - Other.Y, Z - Other.Z);
    }

    FVector operator*(float Scale) const {
        return FVector(X * Scale, Y * Scale, Z * Scale);
    }

    float Length() const;
    float LengthSquared() const { return X*X + Y*Y + Z*Z; }
    FVector Normalized() const;
    static float Dot(const FVector& A, const FVector& B);
    static FVector Cross(const FVector& A, const FVector& B);
};

inline const FVector FVector::Zero = FVector(0, 0, 0);
inline const FVector FVector::One = FVector(1, 1, 1);
inline const FVector FVector::Up = FVector(0, 1, 0);
inline const FVector FVector::Forward = FVector(0, 0, 1);
inline const FVector FVector::Right = FVector(1, 0, 0);

/// Quaternion rotation
struct FQuat {
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float W = 1.0f;

    FQuat() = default;
    FQuat(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}

    static const FQuat Identity;

    static FQuat FromEuler(const FVector& Euler);
    FVector ToEuler() const;
    FVector RotateVector(const FVector& V) const;
};

inline const FQuat FQuat::Identity = FQuat(0, 0, 0, 1);

/// Transform (position, rotation, scale)
struct FTransform {
    FVector Position;
    FQuat Rotation;
    FVector Scale = FVector::One;

    FTransform() = default;

    static const FTransform Identity;
};

inline const FTransform FTransform::Identity = FTransform();

/// Entity identifier
struct FEntityId {
    uint32_t Index = UINT32_MAX;
    uint32_t Generation = 0;

    FEntityId() = default;
    FEntityId(uint32_t index, uint32_t gen) : Index(index), Generation(gen) {}

    bool IsValid() const { return Index != UINT32_MAX; }

    static const FEntityId Invalid;
};

inline const FEntityId FEntityId::Invalid = FEntityId();

/// Hit result from collision/raycast
struct FHitResult {
    bool bHit = false;
    FVector Point;
    FVector Normal;
    float Distance = 0.0f;
    FEntityId Entity;
};

/// Damage information
struct FDamageInfo {
    float Amount = 0.0f;
    int32_t DamageType = 0;
    FEntityId Source;
    FVector HitPoint;
    FVector HitNormal;
    bool bIsCritical = false;
};

/// Input action
struct FInputAction {
    const char* ActionName = nullptr;
    float Value = 0.0f;
    bool bPressed = false;
};

// ============================================================================
// Extended Types (Game Systems)
// ============================================================================

/// 2D Vector (for UI coordinates)
struct FVector2 {
    float X = 0.0f;
    float Y = 0.0f;

    FVector2() = default;
    FVector2(float x, float y) : X(x), Y(y) {}

    static const FVector2 Zero;
    static const FVector2 One;
};

inline const FVector2 FVector2::Zero = FVector2(0, 0);
inline const FVector2 FVector2::One = FVector2(1, 1);

/// Color (RGBA)
struct FColor {
    float R = 1.0f;
    float G = 1.0f;
    float B = 1.0f;
    float A = 1.0f;

    FColor() = default;
    FColor(float r, float g, float b, float a = 1.0f) : R(r), G(g), B(b), A(a) {}

    static const FColor White;
    static const FColor Black;
    static const FColor Red;
    static const FColor Green;
    static const FColor Blue;
    static const FColor Yellow;
};

inline const FColor FColor::White = FColor(1, 1, 1, 1);
inline const FColor FColor::Black = FColor(0, 0, 0, 1);
inline const FColor FColor::Red = FColor(1, 0, 0, 1);
inline const FColor FColor::Green = FColor(0, 1, 0, 1);
inline const FColor FColor::Blue = FColor(0, 0, 1, 1);
inline const FColor FColor::Yellow = FColor(1, 1, 0, 1);

/// Health information
struct FHealthInfo {
    float Current = 100.0f;
    float Max = 100.0f;
    bool bIsAlive = true;
    bool bIsInvulnerable = false;
    float RegenRate = 0.0f;
};

/// Status effect information
struct FStatusEffect {
    uint32_t EffectId = 0;
    uint32_t Stacks = 0;
    float RemainingDuration = 0.0f;
    FEntityId Source;
};

/// Weapon information
struct FWeaponInfo {
    uint32_t WeaponId = 0;
    uint32_t AmmoCurrent = 0;
    uint32_t AmmoReserve = 0;
    uint32_t AmmoMaxMagazine = 0;
    bool bIsReloading = false;
    float FireRate = 1.0f;
    float Damage = 10.0f;
};

/// Item information
struct FItemInfo {
    uint32_t ItemId = 0;
    uint32_t SlotIndex = UINT32_MAX;
    uint32_t Count = 0;
    uint32_t MaxStack = 1;
    float Weight = 0.0f;
    bool bIsEquipped = false;
};

/// Inventory information
struct FInventoryInfo {
    uint32_t TotalSlots = 20;
    uint32_t UsedSlots = 0;
    float CurrentWeight = 0.0f;
    float MaxWeight = 999999.0f;
};

/// AI state information
struct FAiState {
    uint32_t CurrentState = 0;
    FEntityId TargetEntity;
    FVector TargetPosition;
    float AlertLevel = 0.0f;
    bool bHasTarget = false;
    bool bCanSeeTarget = false;
    bool bCanHearTarget = false;
};

/// Navigation path information
struct FNavPath {
    bool bIsValid = false;
    bool bIsPartial = false;
    float PathLength = 0.0f;
    uint32_t WaypointCount = 0;
};

/// Cover point information
struct FCoverPoint {
    FVector Position;
    FVector Facing;
    bool bIsOccupied = false;
    int32_t CoverType = 0; // 0=full, 1=half, 2=lean
};

/// Quest status enumeration
enum class EQuestStatus : int32_t {
    NotStarted = 0,
    InProgress = 1,
    Completed = 2,
    Failed = 3,
};

/// Quest information
struct FQuestInfo {
    uint32_t QuestId = 0;
    EQuestStatus Status = EQuestStatus::NotStarted;
    uint32_t CurrentObjective = 0;
    uint32_t TotalObjectives = 0;
};

/// Objective progress information
struct FObjectiveInfo {
    uint32_t ObjectiveId = 0;
    uint32_t CurrentCount = 0;
    uint32_t RequiredCount = 1;
    bool bIsComplete = false;
    bool bIsOptional = false;
};

/// Sound handle for managing playing sounds
struct FSoundHandle {
    uint64_t Id = 0;

    bool IsValid() const { return Id != 0; }

    static const FSoundHandle Invalid;
};

inline const FSoundHandle FSoundHandle::Invalid = FSoundHandle();

/// Audio playback parameters
struct FAudioParams {
    float Volume = 1.0f;
    float Pitch = 1.0f;
    bool bLooping = false;
    bool b3D = false;
    FVector Position;
    float MinDistance = 1.0f;
    float MaxDistance = 100.0f;
};

/// HUD element information
struct FHudElement {
    uint32_t ElementId = 0;
    bool bIsVisible = true;
    FVector2 Position;
    FVector2 Size;
    float Opacity = 1.0f;
};

// ============================================================================
// Damage Types
// ============================================================================

enum class EDamageType : int32_t {
    Physical = 0,
    Fire = 1,
    Ice = 2,
    Electric = 3,
    Poison = 4,
    Energy = 5,
    True = 6,  // Ignores resistances
};

// ============================================================================
// World Context (Engine Access)
// ============================================================================

/// Forward declaration
class VoidWorld;

/// World context provided by the engine
class VOID_API VoidWorldContext {
public:
    // ========== Entity Operations ==========
    FEntityId SpawnEntity(const char* Prefab);
    void DestroyEntity(FEntityId Entity);
    FEntityId GetEntityByName(const char* Name);
    bool EntityHasTag(FEntityId Entity, const char* Tag);
    float GetDistance(FEntityId A, FEntityId B);
    bool HasLineOfSight(FEntityId From, FEntityId To);
    void SetEntityEnabled(FEntityId Entity, bool bEnabled);
    bool IsEntityEnabled(FEntityId Entity);

    // ========== Transform ==========
    FVector GetEntityPosition(FEntityId Entity);
    void SetEntityPosition(FEntityId Entity, const FVector& Position);
    FQuat GetEntityRotation(FEntityId Entity);
    void SetEntityRotation(FEntityId Entity, const FQuat& Rotation);
    FTransform GetEntityTransform(FEntityId Entity);
    void SetEntityTransform(FEntityId Entity, const FTransform& Transform);
    FVector GetEntityScale(FEntityId Entity);
    void SetEntityScale(FEntityId Entity, const FVector& Scale);

    // ========== Physics ==========
    void ApplyForce(FEntityId Entity, const FVector& Force);
    void ApplyForceAtLocation(FEntityId Entity, const FVector& Force, const FVector& Location);
    void ApplyImpulse(FEntityId Entity, const FVector& Impulse);
    void SetVelocity(FEntityId Entity, const FVector& Velocity);
    FVector GetVelocity(FEntityId Entity);
    FHitResult Raycast(const FVector& Origin, const FVector& Direction, float MaxDistance);
    FHitResult SphereCast(const FVector& Origin, const FVector& Direction, float Radius, float MaxDistance);
    FHitResult BoxCast(const FVector& Origin, const FVector& Direction, const FVector& HalfExtent, const FQuat& Rotation, float MaxDistance);
    void SetGravityScale(FEntityId Entity, float Scale);
    void SetPhysicsEnabled(FEntityId Entity, bool bEnabled);

    // ========== Audio (doc 26) ==========
    void PlaySound(const char* SoundName);
    void PlaySoundAtLocation(const char* SoundName, const FVector& Location);
    FSoundHandle PlaySoundEx(const char* SoundName, const FAudioParams& Params);
    void StopSound(FSoundHandle Handle);
    void SetSoundVolume(FSoundHandle Handle, float Volume);
    void SetSoundPitch(FSoundHandle Handle, float Pitch);
    void SetSoundPosition(FSoundHandle Handle, const FVector& Position);
    void PlayMusic(const char* TrackName, float FadeTime = 0.0f);
    void StopMusic();
    void SetMusicVolume(float Volume);
    void CrossfadeMusic(const char* NewTrack, float FadeTime);

    // ========== Combat (doc 24) ==========
    FHealthInfo GetHealthInfo(FEntityId Entity);
    void ApplyDamage(FEntityId Target, const FDamageInfo& DamageInfo);
    void HealEntity(FEntityId Target, float Amount, FEntityId Source = FEntityId::Invalid);
    void SetInvulnerable(FEntityId Entity, bool bInvulnerable);
    void ApplyStatusEffect(FEntityId Target, uint32_t EffectId, float Duration, FEntityId Source = FEntityId::Invalid);
    void RemoveStatusEffect(FEntityId Target, uint32_t EffectId);
    bool HasStatusEffect(FEntityId Entity, uint32_t EffectId);
    FWeaponInfo GetWeaponInfo(FEntityId Entity);
    void FireWeapon(FEntityId Entity);
    void ReloadWeapon(FEntityId Entity);

    // ========== Inventory (doc 25) ==========
    FInventoryInfo GetInventoryInfo(FEntityId Entity);
    bool AddItem(FEntityId Entity, uint32_t ItemId, uint32_t Count = 1);
    bool RemoveItem(FEntityId Entity, uint32_t ItemId, uint32_t Count = 1);
    bool HasItem(FEntityId Entity, uint32_t ItemId, uint32_t Count = 1);
    uint32_t GetItemCount(FEntityId Entity, uint32_t ItemId);
    bool EquipItem(FEntityId Entity, uint32_t ItemId);
    void UnequipItem(FEntityId Entity, uint32_t ItemId);
    bool UseItem(FEntityId Entity, uint32_t ItemId);
    FEntityId DropItem(FEntityId Entity, uint32_t ItemId, uint32_t Count, const FVector& Position);

    // ========== AI/Navigation (doc 29) ==========
    FAiState GetAiState(FEntityId Entity);
    void SetAiState(FEntityId Entity, uint32_t State);
    void SetAiTarget(FEntityId Entity, FEntityId Target);
    void SetAiTargetPosition(FEntityId Entity, const FVector& Position);
    void ClearAiTarget(FEntityId Entity);
    FNavPath FindPath(const FVector& From, const FVector& To);
    bool AiMoveTo(FEntityId Entity, const FVector& Destination);
    void AiStop(FEntityId Entity);
    FCoverPoint FindCover(const FVector& Position, const FVector& ThreatDirection, float MaxDistance);
    void AlertNearby(const FVector& Position, float Radius, FEntityId Source);

    // ========== State (doc 27) ==========
    int64_t GetStateInt(const char* Name);
    void SetStateInt(const char* Name, int64_t Value);
    double GetStateFloat(const char* Name);
    void SetStateFloat(const char* Name, double Value);
    bool GetStateBool(const char* Name);
    void SetStateBool(const char* Name, bool Value);
    bool SaveGame(const char* SlotName);
    bool LoadGame(const char* SlotName);
    FQuestInfo GetQuestInfo(uint32_t QuestId);
    void StartQuest(uint32_t QuestId);
    void CompleteQuest(uint32_t QuestId);
    void FailQuest(uint32_t QuestId);
    void UpdateObjective(uint32_t QuestId, uint32_t ObjectiveId, uint32_t Progress);
    void CompleteObjective(uint32_t QuestId, uint32_t ObjectiveId);
    void UnlockAchievement(uint32_t AchievementId);
    bool IsAchievementUnlocked(uint32_t AchievementId);

    // ========== UI/HUD (doc 28) ==========
    void ShowHudElement(const char* ElementId);
    void HideHudElement(const char* ElementId);
    void SetHudVisibility(const char* ElementId, bool bVisible);
    void SetHudValue(const char* ElementId, float Value);
    void SetHudText(const char* ElementId, const char* Text);
    void ShowNotification(const char* Message, float Duration = 3.0f);
    void ShowDamageNumber(const FVector& Position, float Damage, bool bCritical = false, const FColor& Color = FColor::White);
    void ShowInteractionPrompt(const char* Prompt, const char* Key = nullptr);
    void HideInteractionPrompt();
    void OpenMenu(const char* MenuId);
    void CloseMenu(const char* MenuId);
    void StartDialogue(const char* DialogueId, FEntityId Speaker = FEntityId::Invalid);
    void EndDialogue();

    // ========== Triggers (doc 22) ==========
    bool IsEntityInTrigger(FEntityId Trigger, FEntityId Entity);
    void EnableTrigger(FEntityId Trigger);
    void DisableTrigger(FEntityId Trigger);
    void ResetTrigger(FEntityId Trigger);

    // ========== Time/Game ==========
    float GetDeltaTime();
    double GetTotalTime();
    float GetTimeScale();
    void SetTimeScale(float Scale);
    void LoadScene(const char* SceneName);

    // ========== Logging ==========
    void Log(int32_t Level, const char* Message);
    void LogInfo(const char* Message) { Log(1, Message); }
    void LogWarning(const char* Message) { Log(2, Message); }
    void LogError(const char* Message) { Log(3, Message); }

private:
    friend class VoidActor;
    void* m_WorldPtr = nullptr;
    void* m_Functions = nullptr;
};

// ============================================================================
// Base Classes
// ============================================================================

/**
 * Base class for all game actors (entities with game logic).
 *
 * Inherit from this class and override lifecycle methods.
 */
class VOID_API VoidActor {
public:
    VoidActor() = default;
    virtual ~VoidActor() = default;

    // ========== Lifecycle Methods ==========

    /// Called when the actor is spawned into the world
    virtual void BeginPlay() {}

    /// Called every frame
    virtual void Tick(float DeltaTime) {}

    /// Called when the actor is being destroyed
    virtual void EndPlay() {}

    /// Called at fixed timestep (for physics)
    virtual void FixedTick(float DeltaTime) {}

    // ========== Collision Events ==========

    /// Called when collision starts
    virtual void OnCollisionEnter(FEntityId Other, const FHitResult& Hit) {}

    /// Called when collision ends
    virtual void OnCollisionExit(FEntityId Other) {}

    /// Called when entering a trigger volume
    virtual void OnTriggerEnter(FEntityId Other) {}

    /// Called when exiting a trigger volume
    virtual void OnTriggerExit(FEntityId Other) {}

    // ========== Combat Events ==========

    /// Called when taking damage
    virtual void OnDamage(const FDamageInfo& DamageInfo) {}

    /// Called when health reaches zero
    virtual void OnDeath(FEntityId Killer) {}

    // ========== Interaction Events ==========

    /// Called when another actor interacts with this one
    virtual void OnInteract(FEntityId Interactor) {}

    /// Called when an input action is triggered
    virtual void OnInputAction(const FInputAction& Action) {}

    // ========== AI Events (doc 29) ==========

    /// Called when AI state changes
    virtual void OnAiStateChange(uint32_t OldState, uint32_t NewState) {}

    /// Called when AI acquires a target
    virtual void OnAiTargetAcquired(FEntityId Target) {}

    /// Called when AI loses its target
    virtual void OnAiTargetLost() {}

    // ========== Inventory Events (doc 25) ==========

    /// Called when inventory changes
    virtual void OnInventoryChange(uint32_t ItemId, uint32_t OldCount, uint32_t NewCount) {}

    /// Called when picking up an item
    virtual void OnItemPickup(uint32_t ItemId, uint32_t Count) {}

    /// Called when using an item
    virtual void OnItemUse(uint32_t ItemId) {}

    // ========== Weapon Events (doc 24) ==========

    /// Called when weapon fires
    virtual void OnWeaponFire() {}

    /// Called when weapon reloads
    virtual void OnWeaponReload() {}

    // ========== Combat Events Extended (doc 24) ==========

    /// Called when a status effect is applied
    virtual void OnStatusEffectApplied(uint32_t EffectId, FEntityId Source) {}

    /// Called when a status effect is removed
    virtual void OnStatusEffectRemoved(uint32_t EffectId) {}

    /// Called when healed
    virtual void OnHeal(float Amount, FEntityId Source) {}

    // ========== Quest Events (doc 27) ==========

    /// Called when a quest starts
    virtual void OnQuestStart(uint32_t QuestId) {}

    /// Called when quest objective progress updates
    virtual void OnQuestProgress(uint32_t QuestId, uint32_t ObjectiveId, uint32_t Progress) {}

    /// Called when a quest is completed
    virtual void OnQuestComplete(uint32_t QuestId) {}

    /// Called when an achievement is unlocked
    virtual void OnAchievementUnlocked(uint32_t AchievementId) {}

    // ========== Dialogue Events (doc 28) ==========

    /// Called when dialogue starts
    virtual void OnDialogueStart(uint32_t DialogueId, FEntityId Speaker) {}

    /// Called when dialogue choice is made
    virtual void OnDialogueChoice(uint32_t DialogueId, uint32_t ChoiceId) {}

    // ========== Audio Events (doc 26) ==========

    /// Called when a sound finishes playing
    virtual void OnSoundFinished(FSoundHandle SoundHandle) {}

    // ========== Transform ==========

    FVector GetPosition() const;
    void SetPosition(const FVector& Position);
    FQuat GetRotation() const;
    void SetRotation(const FQuat& Rotation);
    FVector GetForwardVector() const;
    FVector GetRightVector() const;
    FVector GetUpVector() const;

    // ========== Entity Info ==========

    FEntityId GetEntityId() const { return m_EntityId; }
    VoidWorldContext* GetWorld() { return &m_World; }

    // ========== Serialization (for hot-reload) ==========

    /// Override to serialize custom state
    virtual size_t Serialize(uint8_t* Buffer, size_t MaxSize) { return 0; }

    /// Override to deserialize custom state
    virtual bool Deserialize(const uint8_t* Buffer, size_t Size) { return true; }

    /// Override to return serialized size
    virtual size_t GetSerializedSize() { return 0; }

protected:
    FEntityId m_EntityId;
    VoidWorldContext m_World;

    // Internal - set by engine
    friend void void_set_entity_id(void*, FEntityId);
    friend void void_set_world_context(void*, const void*);
};

/**
 * Base class for components that can be attached to actors.
 */
class VOID_API VoidComponent {
public:
    VoidComponent() = default;
    virtual ~VoidComponent() = default;

    virtual void OnAttach(VoidActor* Owner) { m_Owner = Owner; }
    virtual void OnDetach() { m_Owner = nullptr; }
    virtual void Tick(float DeltaTime) {}

    VoidActor* GetOwner() const { return m_Owner; }

protected:
    VoidActor* m_Owner = nullptr;
};

// ============================================================================
// Class Registration Macros
// ============================================================================

/// Class information structure
struct VoidClassInfo {
    const char* Name;
    size_t Size;
    size_t Alignment;
    uint32_t ApiVersion;
    void* (*CreateFn)();
    void (*DestroyFn)(void*);
};

/// VTable for class methods
struct VoidClassVTable {
    // Lifecycle
    void (*BeginPlay)(void*);
    void (*Tick)(void*, float);
    void (*EndPlay)(void*);
    void (*FixedTick)(void*, float);
    // Collision
    void (*OnCollisionEnter)(void*, FEntityId, FHitResult);
    void (*OnCollisionExit)(void*, FEntityId);
    void (*OnTriggerEnter)(void*, FEntityId);
    void (*OnTriggerExit)(void*, FEntityId);
    // Combat
    void (*OnDamage)(void*, FDamageInfo);
    void (*OnDeath)(void*, FEntityId);
    // Interaction
    void (*OnInteract)(void*, FEntityId);
    void (*OnInputAction)(void*, FInputAction);
    // Serialization
    size_t (*Serialize)(void*, uint8_t*, size_t);
    bool (*Deserialize)(void*, const uint8_t*, size_t);
    size_t (*GetSerializedSize)(void*);
};

/// Extended VTable with all game system callbacks
struct VoidExtendedVTable {
    // Base vtable
    VoidClassVTable Base;
    // AI callbacks
    void (*OnAiStateChange)(void*, uint32_t, uint32_t);
    void (*OnAiTargetAcquired)(void*, FEntityId);
    void (*OnAiTargetLost)(void*);
    // Inventory callbacks
    void (*OnInventoryChange)(void*, uint32_t, uint32_t, uint32_t);
    void (*OnItemPickup)(void*, uint32_t, uint32_t);
    void (*OnItemUse)(void*, uint32_t);
    // Weapon callbacks
    void (*OnWeaponFire)(void*);
    void (*OnWeaponReload)(void*);
    // Combat extended callbacks
    void (*OnStatusEffectApplied)(void*, uint32_t, FEntityId);
    void (*OnStatusEffectRemoved)(void*, uint32_t);
    void (*OnHeal)(void*, float, FEntityId);
    // Quest callbacks
    void (*OnQuestStart)(void*, uint32_t);
    void (*OnQuestProgress)(void*, uint32_t, uint32_t, uint32_t);
    void (*OnQuestComplete)(void*, uint32_t);
    void (*OnAchievementUnlocked)(void*, uint32_t);
    // Dialogue callbacks
    void (*OnDialogueStart)(void*, uint32_t, FEntityId);
    void (*OnDialogueChoice)(void*, uint32_t, uint32_t);
    // Audio callbacks
    void (*OnSoundFinished)(void*, FSoundHandle);
};

/// Macro to export a class
#define VOID_EXPORT_CLASS(ClassName) \
    extern "C" VOID_EXPORT void* VOID_CALL ClassName##_Create() { \
        return new ClassName(); \
    } \
    extern "C" VOID_EXPORT void VOID_CALL ClassName##_Destroy(void* ptr) { \
        delete static_cast<ClassName*>(ptr); \
    } \
    static VoidClassInfo ClassName##_ClassInfo = { \
        #ClassName, \
        sizeof(ClassName), \
        alignof(ClassName), \
        VOID_CPP_API_VERSION, \
        &ClassName##_Create, \
        &ClassName##_Destroy \
    }; \
    static VoidClassVTable ClassName##_VTable = { \
        [](void* p) { static_cast<ClassName*>(p)->BeginPlay(); }, \
        [](void* p, float dt) { static_cast<ClassName*>(p)->Tick(dt); }, \
        [](void* p) { static_cast<ClassName*>(p)->EndPlay(); }, \
        [](void* p, float dt) { static_cast<ClassName*>(p)->FixedTick(dt); }, \
        [](void* p, FEntityId o, FHitResult h) { static_cast<ClassName*>(p)->OnCollisionEnter(o, h); }, \
        [](void* p, FEntityId o) { static_cast<ClassName*>(p)->OnCollisionExit(o); }, \
        [](void* p, FEntityId o) { static_cast<ClassName*>(p)->OnTriggerEnter(o); }, \
        [](void* p, FEntityId o) { static_cast<ClassName*>(p)->OnTriggerExit(o); }, \
        [](void* p, FDamageInfo d) { static_cast<ClassName*>(p)->OnDamage(d); }, \
        [](void* p, FEntityId k) { static_cast<ClassName*>(p)->OnDeath(k); }, \
        [](void* p, FEntityId i) { static_cast<ClassName*>(p)->OnInteract(i); }, \
        [](void* p, FInputAction a) { static_cast<ClassName*>(p)->OnInputAction(a); }, \
        [](void* p, uint8_t* b, size_t s) -> size_t { return static_cast<ClassName*>(p)->Serialize(b, s); }, \
        [](void* p, const uint8_t* b, size_t s) -> bool { return static_cast<ClassName*>(p)->Deserialize(b, s); }, \
        [](void* p) -> size_t { return static_cast<ClassName*>(p)->GetSerializedSize(); } \
    }; \
    static VoidExtendedVTable ClassName##_ExtendedVTable = { \
        ClassName##_VTable, \
        [](void* p, uint32_t o, uint32_t n) { static_cast<ClassName*>(p)->OnAiStateChange(o, n); }, \
        [](void* p, FEntityId t) { static_cast<ClassName*>(p)->OnAiTargetAcquired(t); }, \
        [](void* p) { static_cast<ClassName*>(p)->OnAiTargetLost(); }, \
        [](void* p, uint32_t i, uint32_t o, uint32_t n) { static_cast<ClassName*>(p)->OnInventoryChange(i, o, n); }, \
        [](void* p, uint32_t i, uint32_t c) { static_cast<ClassName*>(p)->OnItemPickup(i, c); }, \
        [](void* p, uint32_t i) { static_cast<ClassName*>(p)->OnItemUse(i); }, \
        [](void* p) { static_cast<ClassName*>(p)->OnWeaponFire(); }, \
        [](void* p) { static_cast<ClassName*>(p)->OnWeaponReload(); }, \
        [](void* p, uint32_t e, FEntityId s) { static_cast<ClassName*>(p)->OnStatusEffectApplied(e, s); }, \
        [](void* p, uint32_t e) { static_cast<ClassName*>(p)->OnStatusEffectRemoved(e); }, \
        [](void* p, float a, FEntityId s) { static_cast<ClassName*>(p)->OnHeal(a, s); }, \
        [](void* p, uint32_t q) { static_cast<ClassName*>(p)->OnQuestStart(q); }, \
        [](void* p, uint32_t q, uint32_t o, uint32_t pr) { static_cast<ClassName*>(p)->OnQuestProgress(q, o, pr); }, \
        [](void* p, uint32_t q) { static_cast<ClassName*>(p)->OnQuestComplete(q); }, \
        [](void* p, uint32_t a) { static_cast<ClassName*>(p)->OnAchievementUnlocked(a); }, \
        [](void* p, uint32_t d, FEntityId s) { static_cast<ClassName*>(p)->OnDialogueStart(d, s); }, \
        [](void* p, uint32_t d, uint32_t c) { static_cast<ClassName*>(p)->OnDialogueChoice(d, c); }, \
        [](void* p, FSoundHandle h) { static_cast<ClassName*>(p)->OnSoundFinished(h); } \
    };

/// Start class registration block
#define VOID_BEGIN_CLASS_REGISTRY() \
    static std::vector<VoidClassInfo*> g_RegisteredClasses; \
    static std::vector<VoidClassVTable*> g_RegisteredVTables;

/// Register a class
#define VOID_REGISTER_CLASS(ClassName) \
    g_RegisteredClasses.push_back(&ClassName##_ClassInfo); \
    g_RegisteredVTables.push_back(&ClassName##_VTable);

/// End class registration and export library info
#define VOID_END_CLASS_REGISTRY(LibName, LibVersion) \
    static const char* g_LibraryName = LibName; \
    static const char* g_LibraryVersion = LibVersion; \
    extern "C" VOID_EXPORT void VOID_CALL void_register_classes() { \
        /* Classes registered via VOID_REGISTER_CLASS */ \
    } \
    extern "C" VOID_EXPORT VoidLibraryInfo VOID_CALL void_get_library_info() { \
        VoidLibraryInfo info; \
        info.ApiVersion = VOID_CPP_API_VERSION; \
        info.ClassCount = static_cast<uint32_t>(g_RegisteredClasses.size()); \
        info.Name = g_LibraryName; \
        info.Version = g_LibraryVersion; \
        return info; \
    } \
    extern "C" VOID_EXPORT const VoidClassInfo* VOID_CALL void_get_class_info(uint32_t index) { \
        if (index < g_RegisteredClasses.size()) { \
            return g_RegisteredClasses[index]; \
        } \
        return nullptr; \
    } \
    extern "C" VOID_EXPORT const VoidClassVTable* VOID_CALL void_get_class_vtable(const char* name) { \
        for (size_t i = 0; i < g_RegisteredClasses.size(); ++i) { \
            if (strcmp(g_RegisteredClasses[i]->Name, name) == 0) { \
                return g_RegisteredVTables[i]; \
            } \
        } \
        return nullptr; \
    } \
    extern "C" VOID_EXPORT void VOID_CALL void_set_entity_id(void* actor, FEntityId id) { \
        static_cast<VoidActor*>(actor)->m_EntityId = id; \
    } \
    extern "C" VOID_EXPORT void VOID_CALL void_set_world_context(void* actor, const void* context) { \
        /* Copy world context */ \
        memcpy(&static_cast<VoidActor*>(actor)->m_World, context, sizeof(VoidWorldContext)); \
    }

/// Library info structure
struct VoidLibraryInfo {
    uint32_t ApiVersion;
    uint32_t ClassCount;
    const char* Name;
    const char* Version;
};

#endif // VOID_API_H
