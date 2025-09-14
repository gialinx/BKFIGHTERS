#pragma once
#include "../../Utilities/Math.h"

class Camera {
private:
    Vector3 m_position;
    Vector3 m_target;
    Vector3 m_up;
    
    float m_left, m_right, m_bottom, m_top;
    float m_nearPlane, m_farPlane;
    
    float m_baseLeft, m_baseRight, m_baseBottom, m_baseTop;
    float m_minZoom, m_maxZoom;
    float m_currentZoom;
    float m_zoomSpeed;
    float m_targetZoom;
    bool m_autoZoomEnabled;
    float m_verticalOffset;
    
    float m_characterWidth;
    float m_characterHeight;
    float m_paddingX;
    float m_paddingY;
    
    Vector3 m_initialPosition;
    Vector3 m_initialTarget;
    float m_initialZoom;
    
    Matrix m_viewMatrix;
    Matrix m_projectionMatrix;
    Matrix m_viewProjectionMatrix;
    
    bool m_viewNeedsUpdate;
    bool m_projectionNeedsUpdate;
    bool m_vpMatrixNeedsUpdate;

    // Screen shake
    float m_shakeTimeRemaining = 0.0f;
    float m_shakeDuration = 0.0f;
    float m_shakeAmplitude = 0.0f;
    float m_shakeFrequency = 20.0f;
    Vector3 m_shakeOffset = Vector3(0.0f, 0.0f, 0.0f);
    
    void UpdateViewMatrix();
    void UpdateProjectionMatrix();
    void UpdateViewProjectionMatrix();
    void UpdateShake(float dt);
    
public:
    Camera();
    Camera(const Vector3& position, const Vector3& target, const Vector3& up);
    ~Camera();
    
    void SetPosition(const Vector3& position);
    void SetTarget(const Vector3& target);
    void SetLookAt(const Vector3& position, const Vector3& target, const Vector3& up);
    
    void Move2D(float deltaX, float deltaY);
    void SetPosition2D(float x, float y);
    
    void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    
    void EnableAutoZoom(bool enable) { m_autoZoomEnabled = enable; }
    void SetZoomRange(float minZoom, float maxZoom);
    void SetZoomSpeed(float speed) { m_zoomSpeed = speed; }
    void UpdateZoom(float deltaTime);
    void SetTargetZoom(float targetZoom);
    float GetCurrentZoom() const { return m_currentZoom; }
    bool IsAutoZoomEnabled() const { return m_autoZoomEnabled; }
    void ResetToInitialState();
    void SetVerticalOffset(float offset) { m_verticalOffset = offset; }
    
    void UpdateCameraForCharacters(const Vector3& player1Pos, const Vector3& player2Pos, float deltaTime);
    
    void SetCharacterDimensions(float width, float height);
    void SetCameraPadding(float paddingX, float paddingY);

    void AddShake(float amplitude, float duration, float frequency = 20.0f);
    
    const Matrix& GetViewMatrix();
    const Matrix& GetProjectionMatrix();
    const Matrix& GetViewProjectionMatrix();
    
    const Vector3& GetPosition() const { return m_position; }
    const Vector3& GetTarget() const { return m_target; }
    float GetLeft() const { return m_left; }
    float GetRight() const { return m_right; }
    float GetBottom() const { return m_bottom; }
    float GetTop() const { return m_top; }
    float GetNearPlane() const { return m_nearPlane; }
    float GetFarPlane() const { return m_farPlane; }
}; 