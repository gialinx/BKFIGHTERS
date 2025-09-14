#pragma once
#include <memory>
#include <functional>
#include "../../Utilities/Math.h"
#include "Object.h"
#include "AnimationManager.h"
#include "WallCollision.h"

class Camera;

class EnergyOrbProjectile {
private:
    std::unique_ptr<Object> m_object;
    std::shared_ptr<AnimationManager> m_animManager;
    
    bool m_isActive;
    Vector3 m_position;
    Vector3 m_velocity;
    float m_speed;
    float m_lifetime;
    float m_maxLifetime;
    
    bool m_isExploding;
    float m_explosionTimer;
    float m_explosionDuration;
    
    // Animation states
    int m_currentAnimation;
    bool m_animationLoop;
    
    // Wall collision system
    WallCollision* m_wallCollision;
    
    int m_ownerId;
    
    std::function<void(float)> m_explosionCallback;
    
public:
    EnergyOrbProjectile();
    ~EnergyOrbProjectile();
    
    void Initialize();
    void SetWallCollision(WallCollision* wallCollision) { m_wallCollision = wallCollision; }
    void SetExplosionCallback(std::function<void(float)> callback) { m_explosionCallback = callback; }
    void Spawn(const Vector3& position, const Vector3& direction, float speed, int ownerId);
    void Update(float deltaTime);
    void Draw(class Camera* camera);
    
    bool IsActive() const { return m_isActive; }
    void SetActive(bool active) { m_isActive = active; }
    
    const Vector3& GetPosition() const { return m_position; }
    const Vector3& GetVelocity() const { return m_velocity; }
    
    int GetOwnerId() const { return m_ownerId; }
    
    void TriggerExplosion();
    bool IsExploding() const { return m_isExploding; }
    
private:
    void HandleCollision();
    bool CheckWallCollision() const;
};
