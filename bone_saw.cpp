/*
	Bone Saw for Maya 6.5+
	2006 8Monkey Labs
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

#define	clearPtr(p)	{if(p){delete p; p=0;}}
#define clearArray(a) {if(a){delete[] a; a=0;}}

bool	sawThemBones( const std::string& bone, const std::string& boneparent );
MMatrix	worldTransform( MFnTransform& obj );

boneSaw::boneSaw()
{
}

void*	boneSaw::creator()
{
	return new boneSaw;
}

MStatus boneSaw::doIt( const MArgList& args )
{
	std::cout << "\n\n-------- Bone Saw -------------\n";
	std::cout << "-------------------------------\n";

	bool		remerge = true;

	//TODO: check arg count / print usage string

	std::string	thebone, theparent;
	for( unsigned int i=0; i<args.length(); ++i )
	{
		std::string	s = args.asString(i).asChar();
		
		if( thebone.empty() )
		{ thebone = s; }
		else if( theparent.empty() )
		{ theparent = s; }
	}
	
	bool result = sawThemBones( thebone, theparent );
	std::cout << "All Done!\n\n";
	
	if( result )
	{
		setResult( "Bone sawing completed.\n" );
		return MS::kSuccess;
	}
	else
	{
		setResult( "Bone sawing failed!\n" );
		return MS::kFailure;
	}
}

bool	sawThemBones( const std::string& bone, const std::string& boneparent )
{
	//look for the bones
	MDagPath bone_path;
	MDagPath boneparent_path;
	MItDag dagIterator( MItDag::kBreadthFirst, MFn::kInvalid );
	for( ; !dagIterator.isDone(); dagIterator.next() )
	{
		MDagPath	dagPath;
		dagIterator.getPath(dagPath);
		
		if( dagPath.partialPathName().asChar() == bone )
		{ bone_path = dagPath; }
		else if( dagPath.partialPathName().asChar() == boneparent )
		{ boneparent_path = dagPath; }
	}
	
	//give up if we cant find the child bone
	if( !bone_path.isValid() || !bone_path.hasFn( MFn::kJoint ) )
	{ std::cout << "Cannot find bone \"" << bone << "\"!\n"; return false; }
	
	//give up if we cant find the parent bone
	if( !boneparent_path.isValid() || !boneparent_path.hasFn( MFn::kJoint ) )
	{ std::cout << "Cannot find bone \"" << boneparent << "\"!\n"; return false; }
	
	//print this out just so the user can know whats going down
	std::cout	<< "Attaching bone \"" << bone_path.fullPathName()
				<< "\" under \"" << boneparent_path.fullPathName() << "\"...\n";
	
	//attach ikjoint functions
	MFnIkJoint fn_bone( bone_path );
	MFnIkJoint fn_parent( boneparent_path );
	
	//are they already connected?
	if( fn_bone.isChildOf( fn_parent.object() ) )
	{ std::cout << "These bones are already connected!\n"; return false; }
	
	//remove parents of child bone (TODO: does this properly untransform it?)
	while( fn_bone.parentCount() )
	{
		MFnDagNode theparent( fn_bone.parent(0) );
		theparent.removeChild( fn_bone.object() );
	}
	
	//find the parent, child, and new child xforms
	MMatrix	bmat = worldTransform( fn_bone );
	MMatrix pmat = worldTransform( fn_parent );
	MMatrix pmat_inv = pmat.inverse();
	MMatrix bmat_relative = (bmat * pmat_inv);
	
	//make the new parent connection
	fn_parent.addChild( fn_bone.object() );
	
	//append the transform to keep it in place
	fn_bone.set( MTransformationMatrix(bmat_relative) );
	
	return true;
}

MMatrix	worldTransform( MFnTransform& obj )
{
	MMatrix thismat = obj.transformation().asMatrix();
	if( obj.parentCount() > 0 )
	{
		MObject p = obj.parent(0);
		if( p.hasFn( MFn::kTransform ) )
		{
			MFnTransform parent(p);
			return thismat * worldTransform(parent);
		}
	}
	return thismat;
}

