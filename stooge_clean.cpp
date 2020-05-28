/*
	Stooge Cleaner for Maya 6.5+
	2006 Annex Labs
	Jeff Russell
*/

//needed to placate the deprecated iostream madness that maya uses
#ifndef REQUIRE_IOSTREAM
#define  REQUIRE_IOSTREAM
#endif

//#include <maya/MSimple.h>
#include <maya/MStatus.h>

#include <maya/MPxCommand.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MItSelectionList.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItMeshEdge.h>
#include <maya/MFloatVector.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MObjectArray.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MFnDagNode.h>
#include <maya/MItDag.h>
#include <maya/MDistance.h>
#include <maya/MIntArray.h>
#include <maya/MIOStream.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MItGeometry.h>
#include <maya/MMatrix.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFnIkJoint.h>
#include <maya/MQuaternion.h>
#include <maya/MItKeyframe.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MAnimControl.h>
#include <maya/MTime.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

#include "stooge_plugin.h"

#define	CM_TO_FEET	0.03280839895013123

#define	clearPtr(p)	{if(p){delete p; p=0;}}
#define clearArray(a) {if(a){delete[] a; a=0;}}

struct merge_t
{
	MString	dst_name;
	MString	src_objects;
};

MString	materialNameOfMesh( MFnMesh& mesh, MDagPath& dagPath );

bool	cleanStuff( bool remerge );

stoogeClean::stoogeClean()
{
}

void*	stoogeClean::creator()
{
	return new stoogeClean;
}

MStatus stoogeClean::doIt( const MArgList& args )
{
	std::cout << "\n\n-------- Stooge Cleaner -------------\n";
	std::cout << "-------------------------------------\n";

	bool		remerge = true;

	for( unsigned int i=0; i<args.length(); ++i )
	{
		std::string	s = args.asString(i).asChar();

		if( s == "-nm" || s == "-nomerge" )
		{
			remerge = false;
		}
	}
	
	bool result = cleanStuff( remerge );
	
	setResult( "Stooge clean completed.\n" );
	return result ? MS::kSuccess : MS::kFailure;
}


bool	cleanStuff( bool remerge )
{
	//clear the selection
	MGlobal::executeCommand( "select -clear" );

	///// STEP1 : triangulate & disconnect triangles by material
	for( MItDag dagIterator( MItDag::kBreadthFirst, MFn::kInvalid ); !dagIterator.isDone(); dagIterator.next() )
	{
		MDagPath dagPath;
		dagIterator.getPath(dagPath);
		
		MFnDagNode dagNode( dagPath );
		
		//skip these
		if( dagNode.isIntermediateObject() || !dagPath.hasFn(MFn::kMesh) || dagPath.hasFn(MFn::kTransform) )
		{ continue; }
		
		//triangulate this guy
		MGlobal::executeCommand( "polyTriangulate "+dagPath.fullPathName() );
		
		//find connected materials & disconnect faces accordingly
		MFnMesh			mesh( dagPath );
		unsigned		instanceNumber = dagPath.instanceNumber();
		MObjectArray	shaders;
		MIntArray		indices;
		mesh.getConnectedShaders( instanceNumber, shaders, indices );
		for( unsigned i=1; i<shaders.length(); ++i )
		{
			MFnDependencyNode	theshader( shaders[i] );
			MString chipcmd( "polyChipOff -kft 1 -dup 0 -ch 0 " );
			bool dochip=false;
			for( unsigned j=0; j<indices.length(); ++j )
			{
				if( indices[j] == i )
				{
					chipcmd += (dagPath.fullPathName() + ".f[" + j + "] ");
					dochip = true;
				}
			}
			if( dochip ) { MGlobal::executeCommand( chipcmd ); }
		}
	}
	
	////// STEP2 : separate meshes
	for( MItDag dagIterator( MItDag::kBreadthFirst, MFn::kInvalid ); !dagIterator.isDone(); dagIterator.next() )
	{
		MDagPath dagPath;
		dagIterator.getPath(dagPath);
		
		MFnDagNode dagNode( dagPath );
		
		//skip these
		if( dagNode.isIntermediateObject() || !dagPath.hasFn(MFn::kMesh) || dagPath.hasFn(MFn::kTransform) )
		{ continue; }
		
		//split this guy
		MGlobal::executeCommand( "polySeparate " + dagPath.fullPathName() );
	}
	
	////// STEP3 : rejoin meshes by material
	//std::vector<merge_t>	merges;
	RESTART_MERGES:
	for( MItDag dagIterator( MItDag::kBreadthFirst, MFn::kInvalid ); !dagIterator.isDone(); dagIterator.next() )
	{
		MDagPath dagPath; dagIterator.getPath(dagPath);
		MFnDagNode dagNode( dagPath );
		
		//skip these
		if( dagNode.isIntermediateObject() || !dagPath.hasFn(MFn::kMesh) || dagPath.hasFn(MFn::kTransform) )
		{ continue; }
		
		//find connected materials
		MFnMesh	mesh( dagPath );
		
		merge_t	m;
		m.dst_name = materialNameOfMesh( mesh, dagPath );

		//2ndary for loop
		int tobemerged = 0;
		for( MItDag other( MItDag::kBreadthFirst, MFn::kInvalid ); !other.isDone(); other.next() )
		{
			MDagPath otherDagPath; other.getPath(otherDagPath);
			MFnDagNode otherDagNode( otherDagPath );
			
			//skip these
			if( otherDagNode.isIntermediateObject() || !otherDagPath.hasFn(MFn::kMesh) || otherDagPath.hasFn(MFn::kTransform) )
			{ continue; }
			
			MFnMesh	otherMesh( otherDagPath );
			
			//does this mesh have the same mtl as me?
			if( m.dst_name == materialNameOfMesh( otherMesh, otherDagPath ) )
			{
				//m.src_objects += " " + otherDagPath.fullPathName();
				//tobemerged++;
				if( otherDagPath.fullPathName() != dagPath.fullPathName() )
				{
					MGlobal::executeCommand( "polyUnite -ch 0 -n " + m.dst_name + " "
					+ dagPath.fullPathName() + " " + otherDagPath.fullPathName() );
					goto RESTART_MERGES;
				}
			}
		}
		/*if( tobemerged > 1 )
		{ merges.push_back( m ); }
		else */
		{ MGlobal::executeCommand( "rename " + dagPath.fullPathName() + " " + m.dst_name ); }
	}
	/*for( unsigned i=0; i<merges.size(); ++i )
	{
		MGlobal::executeCommand( "polyUnite -ch 0 -n " + merges[i].dst_name + " " + merges[i].src_objects );
	}*/
	
	//clear the selection *again*
	MGlobal::executeCommand( "select -clear" );
	
	return true;
}

MString	materialNameOfMesh( MFnMesh& mesh, MDagPath& dagPath )
{
	MString	generic = "generic_mtl";
	unsigned instanceNumber = dagPath.instanceNumber();
	MObjectArray	shaders;
	MIntArray		indices;
	mesh.getConnectedShaders( instanceNumber, shaders, indices );

	for( unsigned i=0; i<indices.length(); ++i )
	{
		if( indices[i] >= 0 ) //this poly has a mtl
		{
			MFnDependencyNode thematerial( shaders[ indices[i] ] );
			generic = thematerial.name();
			break;
		}
	}
	return generic;
}

