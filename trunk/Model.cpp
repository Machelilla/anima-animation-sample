#include "Model.h"
#include <iostream>
#include "RenderContext.h"
#include "DxErr.h"
#include "SkeletonBuilder.h"
#include "AnimationBuilder.h"
#include "MeshBuilder.h"
#include "Skeleton.h"

using namespace std;
Model::Model( const std::string& fileName )
	: mFileName( fileName )
	, mIsLoaded( false )
{
	mPoseBuffer.resize( Skeleton::MAX_BONES_PER_MESH );
}


Model::~Model(void)
{
	mMeshes.clear();
}

void Model::SetRoot( const Math::Matrix& root )
{
	aiMatrix4x4 m;
	memcpy( &m[0][0], root.data, sizeof(Math::Matrix) );
	m.Transpose();

	mSkeleton.setLocalTransform( 0, m );
}

void Model::AcquireResources( RenderContext* context )
{
	for( unsigned int m=0; m<mMeshes.size(); ++m )
	{
		
		Mesh& mesh = mMeshes[m];
		context->Device()->CreateVertexDeclaration( &mesh.Data.mVertexElements[0], &mesh.mVertexDeclaration );

		// now create vertex buffer
		{
			DX_CHECK( context->Device()->CreateVertexBuffer( mesh.Data.mVertexData.size(), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &mesh.mVertexBuffer, NULL ) );

			BYTE* vertexData;
			DX_CHECK( mesh.mVertexBuffer->Lock( 0, 0, reinterpret_cast<void**>( &vertexData ), 0 ) );
				memcpy( vertexData, &mesh.Data.mVertexData[0], mesh.Data.mVertexData.size() );
			DX_CHECK( mesh.mVertexBuffer->Unlock() );
		}


		// build index buffer
		{

			DX_CHECK( context->Device()->CreateIndexBuffer( mesh.Data.mIndexData.size(), D3DUSAGE_WRITEONLY, mesh.Data.mIndexFormat, D3DPOOL_DEFAULT, &mesh.mIndexBuffer, NULL ) );

			BYTE* indexData;
			DX_CHECK( mesh.mIndexBuffer->Lock( 0, 0, reinterpret_cast<void**>( &indexData ), 0 ) );
				memcpy( indexData, &mesh.Data.mIndexData[0], mesh.Data.mIndexData.size() );
			DX_CHECK( mesh.mIndexBuffer->Unlock() );

		}

		// load shaders
		{
			char maxBoneCount[8];
			sprintf_s( maxBoneCount, sizeof(maxBoneCount), "%d", Skeleton::MAX_BONES_PER_MESH );
			D3DXMACRO macros[] = { { "MAX_BONES_PER_MESH", maxBoneCount }, NULL };

			LPD3DXBUFFER buf;			
			HRESULT hr = D3DXCreateEffectFromFile( context->Device(), "../Shaders/Model.fx", macros, NULL, NULL, NULL, &mesh.mEffect, &buf );

			if( FAILED(hr) && buf)
			{
				const char* text = reinterpret_cast<char*>( buf->GetBufferPointer() );
				DXTRACE_ERR( text, hr );
				buf->Release();
			}
		}		
	}
}

void Model::ReleaseResources( RenderContext* context )
{
	for( unsigned int m=0; m<mMeshes.size(); ++m )
	{
		Mesh& mesh = mMeshes[m];

		if( mesh.mEffect )
		{
			mesh.mEffect->Release();
			mesh.mEffect = NULL;
		}

		if( mesh.mIndexBuffer )
		{
			mesh.mIndexBuffer->Release();
			mesh.mIndexBuffer = NULL;
		}

		if( mesh.mVertexBuffer )
		{
			mesh.mVertexBuffer->Release();
			mesh.mVertexBuffer = NULL;
		}

		if( mesh.mVertexDeclaration )
		{
			mesh.mVertexDeclaration->Release();
			mesh.mVertexDeclaration = NULL;
		}
	}
}

bool Model::load( RenderContext* context )
{
	// Create an instance of the Importer class
	Assimp::Importer modelImporter;

	// And have it read the given file with some example postprocessing
	// Usually - if speed is not the most important aspect for you - you'll 
	// propably to request more postprocessing than we do in this example.
	const aiScene* scene = modelImporter.ReadFile( mFileName, 
		aiProcess_CalcTangentSpace       | 
		aiProcess_Triangulate            |
		aiProcess_JoinIdenticalVertices  |
		aiProcess_MakeLeftHanded		 |
		aiProcess_FlipWindingOrder		 |
		aiProcess_SortByPType);

	// If the import failed, report it
	if( !scene)
	{
		std::cout << modelImporter.GetErrorString() << std::endl;
		MessageBox( NULL,  modelImporter.GetErrorString(), "import failed", MB_OK );

		return false;
	}

	// import skeleton, animation and mesh data
	{
		SkeletonBuilder skeletonBuilder( scene );
		AnimationBuilder animationBuilder( scene, skeletonBuilder );
		MeshBuilder meshBuilder( scene, skeletonBuilder );

		skeletonBuilder.BuildSkeleton( mSkeleton );
		animationBuilder.BuildAnimations( mAnimations );
		meshBuilder.BuildMeshes( mMeshes );
	}
	
	// upload to gpu
	AcquireResources( context );

	return true;

}


void Model::Render( RenderContext* context )
{
	context->Device()->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
	context->Device()->SetRenderState( D3DRS_ZWRITEENABLE, D3DZB_TRUE);
	context->Device()->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE);

	for( unsigned int m=0; m<mMeshes.size(); ++m )
	{
		const Mesh& mesh = mMeshes[m];
		D3DXHANDLE hTechnique = mesh.mEffect->GetTechniqueByName( "Model" );
		D3DXHANDLE hViewProjection = mesh.mEffect->GetParameterBySemantic( NULL, "VIEWPROJECTION" );

		D3DXHANDLE hLightDirection = mesh.mEffect->GetParameterBySemantic( NULL, "LIGHTDIRECTION" );
		D3DXHANDLE hBoneTransforms = mesh.mEffect->GetParameterBySemantic( NULL, "BONE_TRANSFORMS" );
		
		DX_CHECK( context->Device()->SetVertexDeclaration( mesh.mVertexDeclaration ) );
		DX_CHECK( context->Device()->SetStreamSource( 0, mesh.mVertexBuffer, 0, mesh.Data.mVertexSize ) );
		DX_CHECK( context->Device()->SetIndices( mesh.mIndexBuffer ) );

		Math::Matrix viewProjection = context->GetViewMatrix() * context->GetProjectionMatrix();
		DX_CHECK( mesh.mEffect->SetMatrix( hViewProjection, &viewProjection.data ) );


		D3DXVECTOR4 lightDirection = Math::Vector( 1, 1, 0 ).Normal();
		DX_CHECK( mesh.mEffect->SetVector( hLightDirection, &lightDirection ) );

		DX_CHECK( mesh.mEffect->SetTechnique( hTechnique ) );
		DX_CHECK( mesh.mEffect->SetFloatArray( hBoneTransforms, mPoseBuffer[0], 12 * mSkeleton.getBoneCount() ) );
	
		UINT cPasses;
		DX_CHECK( mesh.mEffect->Begin( &cPasses, 0 ) );
		for( unsigned int iPass = 0; iPass < cPasses; iPass++ )
		{
			DX_CHECK( mesh.mEffect->BeginPass( iPass ) );
			DX_CHECK( context->Device()->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, mesh.Data.mVertexCount, 0, mesh.Data.mTriangleCount ) );
			DX_CHECK( mesh.mEffect->EndPass() );
		}

		DX_CHECK( mesh.mEffect->End() );
	}

	DX_CHECK( context->Device()->SetStreamSource( 0, NULL, 0, 0 ) );
	DX_CHECK( context->Device()->SetIndices( NULL ) );
}

void Model::Update( float dt )
{
	mAnimations.front()->Update( dt );
	mAnimations.front()->EvaluatePose( mSkeleton );

	for( int i=0; i<mSkeleton.getBoneCount(); ++i )
	{
		mPoseBuffer[i] = mSkeleton.getWorldTransform( i );
	}

}
