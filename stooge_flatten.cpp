/*
	Stooge Flatten for Maya 6.5+
	2008 8Monkey Labs
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

stoogeFlatten::stoogeFlatten()
{
}

void*	stoogeFlatten::creator()
{
	return new boneSaw;
}

#define oh_shit()	{ std::cout << "Something went wrong on line " << __LINE__ << " of file " << __FILE__ << "!" << std::endl; return false; }

void	recursive_flatten( MFnTransform& xform, double* scale )
{
	/*double ident[] = { 1.0, 1.0, 1.0 };
	MVector pos = xform.transformation().getTranslation( MSpace::kTransform );
	pos.x *= scale[0];
	pos.y *= scale[1];
	pos.z *= scale[2];
	xform.transformation.setTranslation( pos, MSpace::kTransform );*/
}

bool	do_flatten( std::string node_name )
{
	return false; //NOT FINISHED, AND MAY NEVER BE - jdr
	
	//look for the bones
	MDagPath node_path;
	MItDag dagIterator( MItDag::kBreadthFirst, MFn::kInvalid );
	for( ; !dagIterator.isDone(); dagIterator.next() )
	{
		MDagPath	dagPath;
		dagIterator.getPath(dagPath);
		
		if( dagPath.partialPathName().asChar() == node_name )
		{ node_path = dagPath; }
	}
	
	if( !node_path.isValid() )
	{
		std::cout << "Cannot find object " << node_name << " to flatten!" << std::endl;
		return false;
	}
	
	std::cout << "Flattening scale on object " << node_path.fullPathName().asChar() << std::endl;
	
	MStatus ret;
	
	MFnTransform	parent( node_path, &ret );
	if( ret != MStatus::kSuccess ) { oh_shit(); }
	
	double the_scale[3];
	parent.transformation().getScale( the_scale, MSpace::kTransform );
	
	std::cout << "Scale is " << the_scale[0] << ',' << the_scale[1] << ',' << the_scale[2] << std::endl;
	
	recursive_flatten( parent, the_scale );
}

MStatus stoogeFlatten::doIt( const MArgList& args )
{
	std::cout << "\n\n-------- Stooge Flatten -------------\n";
	std::cout << "-------------------------------\n";

	std::string parent;
	for( unsigned int i=0; i<args.length(); ++i )
	{
		std::string	s = args.asString(i).asChar();
		
		parent = s;
	}
	
	
	//first find the selected object ... shit
	bool result = do_flatten( parent );
	
	if( result )
	{
		setResult( "Flatten completed.\n" );
		return MS::kSuccess;
	}
	else
	{
		setResult( "Flatten failed!\n" );
		return MS::kFailure;
	}
}

