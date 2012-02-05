#include "Camera.h"
#include "Input.h"
#include <iostream>

#define KEY_DOWN 0x8000 
#define MOVEMENT_SPEED 1.f
#define ROTATION_SPEED 1.5f

Camera::Camera( const Input& input )
	: mAzimuth( Math::Pi/2.f )
	, mZenith( Math::Pi/2.f + 25.f / 180.f * Math::Pi )
	, mInput( input )
{
	mProjection = Math::Matrix::Perspective( 45.0f, 1024.f / 768.f, 1.0f, 1000.0f );
	mPosition = Math::Vector( 0, 1.5, -2 );
	GetCursorPos( &mLastCursorPos );
}


Camera::~Camera(void)
{
}


void Camera::update( float dt )
{
	
	int _up = GetKeyState( VK_UP ) & KEY_DOWN;
	int _down = GetKeyState( VK_DOWN ) & KEY_DOWN;
	int _left = GetKeyState( VK_LEFT ) & KEY_DOWN;
	int _right = GetKeyState( VK_RIGHT ) & KEY_DOWN;
 
/*		
	bool _up = mInput.GetKey( VK_UP );
	bool _down =  mInput.GetKey(VK_DOWN );
	bool _left =  mInput.GetKey( VK_LEFT );
	bool _right =  mInput.GetKey( VK_RIGHT );
 */

	float dAzimuth = -mInput.GetMouse().x * dt * ROTATION_SPEED;
	float dZenith = mInput.GetMouse().y * dt * ROTATION_SPEED;;
	

	mAzimuth += dAzimuth;
	mZenith += dZenith;

	
	const float cA = cos( mAzimuth );
	const float sA = sin( mAzimuth );

	const float cZ = cos( mZenith );
	const float sZ = sin( mZenith );

	Math::Vector forward(
		cA * sZ,  
		cZ,
		sA * sZ
	);

	Math::Vector up(
		-cA * cZ,
		sZ,
		-sA * cZ
	);

	Math::Vector left = forward.Cross( up );

	Math::Vector s = left.Scale( _left ? 1.0f : ( _right ? -1.0f : 0.0f ) ).Scale( dt * MOVEMENT_SPEED );
	Math::Vector f = forward.Scale( _up ? 1.0f : ( _down ? -1.0f : 0.0f ) ).Scale( dt * MOVEMENT_SPEED );	
	mPosition += s+f;
	
	mView =  Math::Matrix::LookAt( mPosition, mPosition + forward, up );
}

