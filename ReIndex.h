#ifndef _RE_INDEX_H_
#define _RE_INDEX_H_

#include <vector>
#include <algorithm>

namespace stupid
{

struct	indexref
{
	unsigned int		vert_index;
	unsigned int		norm_index;
	unsigned int		tangent_index;
	std::vector<int>	uv_index; //array of 'nuvsets' new indices
	
	int newindex;	//the FINAL index of this vertex
};

#define	merge_eps	0.001f

// 0 - not equal
// 1 - partially equal
// 2 - fully equal
int	compare( const indexref& a, const indexref& b, MFloatVectorArray& normals )
{
	bool vmatch = (a.vert_index == b.vert_index);
	
	bool nmatch = (a.norm_index == b.norm_index);
	if( !nmatch )
	{
		MFloatVector anorm = normals[a.norm_index];
		MFloatVector bnorm = normals[b.norm_index];
		
		nmatch =	( fabsf(anorm.x-bnorm.x) < merge_eps ) &&
					( fabsf(anorm.y-bnorm.y) < merge_eps ) &&
					( fabsf(anorm.z-bnorm.z) < merge_eps );
	}
	
	bool btmatch = (a.tangent_index == b.tangent_index);
	//TODO: does btmatch need the same kind of epsilon comparison as the normals use?
	
	bool tmatch = true;
	for( unsigned i=0; i<a.uv_index.size(); ++i )
	{ tmatch = tmatch && (a.uv_index[i] == b.uv_index[i]); }
	
	if( tmatch && nmatch && vmatch && btmatch )
	{ return 2; }
	
	if( tmatch || nmatch || vmatch || btmatch )
	{ return 1; }
	
	return 0;
}

//returns new dimension of all arrays
int	reIndex(	MFloatVectorArray& normals,
				MItMeshPolygon& polyitr,
				std::vector<indexref>& refs,
				MStringArray& uvsetnames,
				bool usetangents = true	)
{
	int uvsets = uvsetnames.length();
	int s = -1;
	int maxindex1=0;
	refs.clear();
	for( ; !polyitr.isDone(); polyitr.next() )
	{
		for( int v=0; v < (int)polyitr.polygonVertexCount(); ++v )
		{
			indexref r;
			r.vert_index = polyitr.vertexIndex(v);
			r.norm_index = polyitr.normalIndex(v);
			r.tangent_index = usetangents ? polyitr.tangentIndex(v) : 0;
			for( int i=0; i<uvsets; ++i )
			{
				int ass;
				polyitr.getUVIndex(v, ass, &(uvsetnames[i]));
				r.uv_index.push_back( ass );
			}
			
			bool match=false, partial=false;
			for( unsigned i=0; i<refs.size(); ++i )
			{
				int result = compare(r,refs[i], normals);
				match = match || (result == 2);
				if( result == 2 )
				{ r.newindex = refs[i].newindex; }
				partial = partial || (result == 1);
			}
			
			if( partial && !match ) //partial match, need to split (give it a new index)
			{ r.newindex = s; --s; }
			else if( !match )
			{ r.newindex = r.vert_index; }
			
			refs.push_back(r);
			if( r.newindex > maxindex1 )
			{ maxindex1 = r.newindex; }
		}
	}
	
	int maxindex2 = maxindex1;
	for( unsigned i=0; i<refs.size(); ++i )
	{
		if( refs[i].newindex < 0 )
		{ refs[i].newindex = maxindex1 - refs[i].newindex; }
		
		if( refs[i].newindex > maxindex2 )
		{ maxindex2 = refs[i].newindex; }
	}
	
	return maxindex2+1;
	/*
	int s = -1
	iterate through polygons
	iterate through vertices
	{
		create an indexref
		if this indexref has (only) a partial match to any existing ones:
			then it needs to be split:
				set newindex to s; --s;
		else if there is a full match
			add it with newindex = other.vert_index
		else
			just add it with newindex = vert_index
			
		keep track of max index
	}
	
	go through indexrefs
	if( newindex < 0 )
	{ newindex = max index - newindex; again keep track of max index and return it as vertex count }
	*/
}

struct index_handle 
{
	int*	index;
	
	inline bool	operator<( const index_handle& other ) const
	{ return *index < *(other.index); }
	
	inline int&	get( void ) const
	{ return *index; }
};


// Fix dead space in the mesh arrays so that we don't waste all this goddamn space.
// I am so disgusted that I even have to do this - jdr
void	finalCollapse(	std::vector<indexref>& indices,
						float* positions,
						float* normals,
						float* tangents,
						float* bitangents,
						float* colors,
						float* boneweights,
						int* boneindices,
						float** uvs,
						int& vertex_count,
						int uvsets,
						int bone_w_count = 4	)
{
	if( indices.empty() ) { return; }

	//build a sorted list of handles to the original indices
	std::vector<index_handle>	sorted_indices;
	sorted_indices.reserve( indices.size() );
	for( unsigned i=0; i<indices.size(); ++i )
	{
		index_handle ih; ih.index = &(indices[i].newindex);
		sorted_indices.push_back(ih);
	}
	std::sort( sorted_indices.begin(), sorted_indices.end() );
	
	//check the sorted list for holes, and fill them if needed
	int old_last_index = -1;
	int new_last_index = -1;
	for( unsigned i=0; i<sorted_indices.size(); ++i )
	{
		int& this_index = sorted_indices[i].get();
		int di = this_index - new_last_index;
		
		// If there's a hole then copy the element from its right side
		// into the first 'empty' spot on the left, then fix the index.
		// In this way holes are moved and merged into one big one at the
		// end of the array.
		//std::cout << this_index << ":";
		if( di > 1 || old_last_index != new_last_index )
		{
			const int new_index = (old_last_index == this_index ? new_last_index : new_last_index + 1);
			
			if( positions )
			{ memcpy( positions + 3*new_index, positions + 3*this_index, 3*sizeof(float) ); }
			if( normals )
			{ memcpy( normals + 3*new_index, normals + 3*this_index, 3*sizeof(float) ); }
			if( tangents )
			{ memcpy( tangents + 3*new_index, tangents + 3*this_index, 3*sizeof(float) ); }
			if( bitangents )
			{ memcpy( bitangents + 3*new_index, bitangents + 3*this_index, 3*sizeof(float) ); }
			if( colors )
			{ memcpy( colors + 3*new_index, colors + 3*this_index, 3*sizeof(float) ); }
			if( boneweights )
			{ memcpy( boneweights + bone_w_count*new_index, boneweights + bone_w_count*this_index, bone_w_count*sizeof(float) ); }
			if( boneindices )
			{ memcpy( boneindices + bone_w_count*new_index, boneindices + bone_w_count*this_index, bone_w_count*sizeof(int) ); }
			if( uvs )
			{
				for( int u=0; u<uvsets; ++u )
				{ memcpy( uvs[u] + 2*new_index, uvs[u] + 2*this_index, 2*sizeof(float) ); }
			}
			
			old_last_index = this_index;
			this_index = new_index;
		}
		else { old_last_index = this_index; }
		//std::cout << this_index << std::endl;
		new_last_index = this_index;
	}
	
	//new vertex count is simply the largest index
	int oldvcount = vertex_count;
	vertex_count = sorted_indices.back().get() + 1;
	std::cout << "vertex count of " << oldvcount << " reduced to " << vertex_count << std::endl;
}

}
#endif
