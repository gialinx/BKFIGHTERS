#pragma once
#include <memory>
#include "../../Utilities/Math.h"
#include "WallCollision.h"
#include "PlatformCollision.h"
#include "LadderCollision.h"
#include "TeleportCollision.h"

enum class CharState {
    Idle,
    MoveLeft,
    MoveRight,
    Die
};

struct PlayerInputConfig {
    int moveLeftKey;
    int moveRightKey;
    int jumpKey;
    int sitKey;
    int rollKey;
    int punchKey;
    int axeKey;
    int kickKey;
    int dieKey;
    
    int rollLeftKey1;
    int rollLeftKey2;
    int rollRightKey1;
    int rollRightKey2;
    
    PlayerInputConfig() : moveLeftKey(0), moveRightKey(0), jumpKey(0), sitKey(0), 
                         rollKey(0), punchKey(0), axeKey(0), kickKey(0), dieKey(0),
                         rollLeftKey1(0), rollLeftKey2(0), rollRightKey1(0), rollRightKey2(0) {}
    
    PlayerInputConfig(int left, int right, int jump, int sit, int roll, int punch, int axe, int kick, int die,
                     int rollLeft1, int rollLeft2, int rollRight1, int rollRight2)
        : moveLeftKey(left), moveRightKey(right), jumpKey(jump), sitKey(sit), 
          rollKey(roll), punchKey(punch), axeKey(axe), kickKey(kick), dieKey(die),
          rollLeftKey1(rollLeft1), rollLeftKey2(rollLeft2), rollRightKey1(rollRight1), rollRightKey2(rollRight2) {}
};

class CharacterMovement {
private:
    float m_posX, m_posY;
    float m_groundY;
    bool m_facingLeft;
    CharState m_state;
    
    bool m_isJumping;
    float m_jumpVelocity;
    float m_jumpStartY;
    bool m_wasJumping;
    
    bool m_isSitting;
    bool m_isRolling = false;
    
    bool m_isDying;
    bool m_isDead;
    float m_dieTimer;
    float m_knockdownTimer;
    bool m_knockdownComplete;
    bool m_attackerFacingLeft;
    float m_dieBaseY = 0.0f;
    bool m_dieLanded = false;
    float m_dieVerticalVelocity = 0.0f;
    
    PlayerInputConfig m_inputConfig;

    float m_lastLeftPressTime = -1.0f;
    float m_lastRightPressTime = -1.0f;
    bool m_isRunningLeft = false;
    bool m_isRunningRight = false;
    bool m_prevLeftKey = false;
    bool m_prevRightKey = false;
    static constexpr float DOUBLE_TAP_THRESHOLD = 0.2f;

    // Double-tap xuống platform
    float m_lastDownPressTime = -1.0f;
    bool m_prevDownKey = false;
    bool m_prevUpKey = false;
    bool m_isFallingThroughPlatform = false;
    float m_fallThroughTimer = 0.0f;
    static constexpr float FALL_THROUGH_DURATION = 0.5f;
    
    // Double-tap vào thang khi đang đứng đất và thấp hơn đáy thang
    float m_lastUpTapTimeForLadder = -1.0f;
    float m_lastDownTapTimeForLadder = -1.0f;
    bool m_prevDownKeyForLadder = false;
    int m_upTapCountForLadder = 0;
    int m_downTapCountForLadder = 0;
    
    // Platform collision
    std::unique_ptr<PlatformCollision> m_platformCollision;
    float m_characterWidth;
    float m_characterHeight;
    bool m_isOnPlatform;
    float m_currentPlatformY;
    int m_currentMovingPlatformId = -1;
    
    // Wall collision
    std::unique_ptr<WallCollision> m_wallCollision;

    // Ladder collision
    std::unique_ptr<LadderCollision> m_ladderCollision;
    bool m_isOnLadder = false;
    float m_ladderCenterX = 0.0f;
    float m_ladderTop = 0.0f;
    float m_ladderBottom = 0.0f;

    // Teleport collision
    std::unique_ptr<TeleportCollision> m_teleportCollision;
    float m_teleportLockTimer = 0.0f;
    int m_lastTeleportFromId = -1;
    bool m_invertHorizontal = false;

    bool m_inputLocked = false;

    bool m_justStartedUpwardJump = false;

    // Hard landing detection
    float m_highestYInAir = 0.0f;
    bool  m_hardLandingRequested = false;
    static constexpr float HARD_LANDING_DROP_THRESHOLD = 0.6f;

    bool  m_rollCadenceActive = false;
    bool  m_rollPhaseIsRoll = true;
    float m_rollPhaseTimer = 0.0f;
    static constexpr float ROLL_PHASE_DURATION = 0.3f;
    static constexpr float WALK_PHASE_DURATION = 0.7f;

    bool  m_hasPendingFallDamage = false;
    float m_pendingFallDamage = 0.0f;
    static constexpr float FALL_DAMAGE_MIN = 10.0f;
    static constexpr float FALL_DAMAGE_MAX = 25.0f;
    static constexpr float FALL_DAMAGE_MIN_DROP = HARD_LANDING_DROP_THRESHOLD;
    static constexpr float FALL_DAMAGE_MAX_DROP = 1.0f;

    void QueueFallDamageFromDrop(float dropDistance);

    bool m_noClipNoGravity = false;
    void UpdateNoClip(float deltaTime, const bool* keyStates);

    bool m_allowLadderDoubleTap = true;
    bool m_ladderEnabled = true;

    // Constants
    static const float JUMP_FORCE;
    static const float GRAVITY;
    static const float GROUND_Y;
    static const float MOVE_SPEED;
    static const float CLIMB_SPEED;
    static const float CLIMB_DOWN_SPEED;
    static const float DIE_KNOCKBACK_SPEED;
    static const float DIE_SLOWMO_DURATION;
    static const float DIE_SLOWMO_SCALE;

    // Speed multipliers
    float m_moveSpeedMultiplier = 1.0f; 
    float m_runSpeedMultiplier  = 1.0f; 
    float m_jumpForceMultiplier = 1.0f;

    bool m_allowRun = true;

public:
    CharacterMovement();
    CharacterMovement(const PlayerInputConfig& inputConfig);
    ~CharacterMovement();
    
    void Initialize(float startX, float startY, float groundY);
    void SetInputConfig(const PlayerInputConfig& config) { m_inputConfig = config; }
    const PlayerInputConfig& GetInputConfig() const { return m_inputConfig; }
    void SetInputLocked(bool locked) { m_inputLocked = locked; }
    bool IsInputLocked() const { return m_inputLocked; }
    void SetLadderDoubleTapEnabled(bool enabled) { m_allowLadderDoubleTap = enabled; }
    void SetLadderEnabled(bool enabled) { m_ladderEnabled = enabled; if (!enabled) m_isOnLadder = false; }
    
    void Update(float deltaTime, const bool* keyStates);
    void UpdateWithHurtbox(float deltaTime, const bool* keyStates, float hurtboxWidth, float hurtboxHeight, float hurtboxOffsetX, float hurtboxOffsetY, bool allowPlatformFallThrough = true);
    bool HandleLadderWithHurtbox(float deltaTime, const bool* keyStates, float hurtboxWidth, float hurtboxHeight, float hurtboxOffsetX, float hurtboxOffsetY);
    
    void HandleMovement(float deltaTime, const bool* keyStates);
    void HandleJump(float deltaTime, const bool* keyStates);
    void HandleJumpWithHurtbox(float deltaTime, const bool* keyStates, float hurtboxWidth, float hurtboxHeight, float hurtboxOffsetX, float hurtboxOffsetY, bool allowPlatformFallThrough = true);
    void HandleLanding(const bool* keyStates);
    void HandleLandingWithHurtbox(const bool* keyStates, float hurtboxWidth, float hurtboxHeight, float hurtboxOffsetX, float hurtboxOffsetY);
    void HandleDie(float deltaTime);
    
    void SetPosition(float x, float y);
    void TriggerDie(bool attackerFacingLeft = false);
    void ResetDieState();
    Vector3 GetPosition() const { return Vector3(m_posX, m_posY, 0.0f); }
    float GetPosX() const { return m_posX; }
    float GetPosY() const { return m_posY; }
    bool IsFacingLeft() const { return m_facingLeft; }
    CharState GetState() const { return m_state; }
    bool IsJumping() const { return m_isJumping; }
    bool IsSitting() const { return m_isSitting; }
    bool IsRolling() const { return m_isRolling; }
    void ForceSit(bool sit) { m_isSitting = sit; }
    bool IsDying() const { return m_isDying; }
    bool IsDead() const { return m_isDead; }
    float GetDieTimer() const { return m_dieTimer; }
    void SetFacingLeft(bool facingLeft) { m_facingLeft = facingLeft; }
    bool HasDieLanded() const { return m_dieLanded; }
    bool IsRunningLeft() const { return m_isRunningLeft; }
    bool IsRunningRight() const { return m_isRunningRight; }
    
    // Platform collision methods
    void AddPlatform(float x, float y, float width, float height);
    void ClearPlatforms();
    void AddMovingPlatformById(int objectId);
    void ClearMovingPlatforms();
    void SetCharacterSize(float width, float height);
    bool CheckPlatformCollision(float& newY);
    bool CheckPlatformCollisionWithHurtbox(float& newY, float hurtboxWidth, float hurtboxHeight, float hurtboxOffsetX, float hurtboxOffsetY);
    bool IsOnPlatform() const { return m_isOnPlatform; }
    float GetCurrentPlatformY() const { return m_currentPlatformY; }
    PlatformCollision* GetPlatformCollision() const { return m_platformCollision.get(); }
    
    // Wall collision methods
    void InitializeWallCollision();
    WallCollision* GetWallCollision() const { return m_wallCollision.get(); }

    // Ladder collision methods
    void InitializeLadderCollision();
    LadderCollision* GetLadderCollision() const { return m_ladderCollision.get(); }
    bool IsOnLadder() const { return m_isOnLadder; }

    // Teleport collision methods
    void InitializeTeleportCollision();
    TeleportCollision* GetTeleportCollision() const { return m_teleportCollision.get(); }

    bool ConsumeJustStartedUpwardJump() {
        bool v = m_justStartedUpwardJump;
        m_justStartedUpwardJump = false;
        return v;
    }

    bool ConsumeHardLandingRequested() {
        bool v = m_hardLandingRequested;
        m_hardLandingRequested = false;
        return v;
    }

    float ConsumePendingFallDamage() {
        if (!m_hasPendingFallDamage) return 0.0f;
        m_hasPendingFallDamage = false;
        float d = m_pendingFallDamage;
        m_pendingFallDamage = 0.0f;
        return d;
    }

    // Static input configurations
    static const PlayerInputConfig PLAYER1_INPUT;
    static const PlayerInputConfig PLAYER2_INPUT;

    // Multipliers control
    void SetMoveSpeedMultiplier(float mul) { m_moveSpeedMultiplier = mul; }
    void SetRunSpeedMultiplier(float mul)  { m_runSpeedMultiplier  = mul; }
    void SetJumpForceMultiplier(float mul) { m_jumpForceMultiplier = mul; }
    float GetMoveSpeedMultiplier() const { return m_moveSpeedMultiplier; }
    float GetRunSpeedMultiplier()  const { return m_runSpeedMultiplier; }
    float GetJumpForceMultiplier() const { return m_jumpForceMultiplier; }

    void SetAllowRun(bool allow) { m_allowRun = allow; }
    bool IsRunAllowed() const { return m_allowRun; }

    void SetNoClipNoGravity(bool enabled) { m_noClipNoGravity = enabled; m_isJumping = false; m_isOnPlatform = false; }
    bool IsNoClipNoGravity() const { return m_noClipNoGravity; }
}; 