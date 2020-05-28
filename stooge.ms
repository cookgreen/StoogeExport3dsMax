-- stooge export attempt
-- 2010 jeff russell aka jaffray raskal
-- UNFINISHED: we'll continue in here if we ever want a max .mesh exporter

function performExport =
(
	-- get selected mesh
	cursel = GetCurrentSelection()
	mesh = cursel[1]
	
	-- is it valid?
	if( mesh == undefined ) do
	(
		messagebox "no mesh selected"
		return undefined
	)
	if( ((classof mesh.baseobject) as string) != "Editable_mesh" ) do
	(
		messagebox "selected object is not a mesh"
		return undefined
	)
	
	-- get output file path
	filepath = getsavefilename caption:"Save Marmoset Mesh" types:"Marmoset Mesh (*.mesh)|*.mesh"
	if( filepath == undefined ) do
	( return undefined )
	
	-- open the file
	file = fopen filepath "wb"
	if( filepath == undefined ) do
	( return undefined )
	
	-- write the file header
	chunkcount = 1
	writeLong file 0
	writeLong file chunkcount
	
	-- write the chunks
	for chunk=1 to chunkcount do
	(
		-- chunk name
		writeString file "chunkNamePlaceholder"
	)
	
	-- close the file
	fclose file
)

rollout stoogeExport "8monkey Stooge Export" width:200 height:200
(
	button info_button "About" pos:[10,10] width:165 height:32 toolTip:"Who made this script?"
	button exp_button "Export" pos:[10,50] width:165 height:32 toolTip:"Export the Mesh"
	
	on info_button pressed do
	(
		messagebox "Marmoset Mesh Export for 3ds Max\n2010 8monkey Labs Inc."
	)
	
	on exp_button pressed do
	(
		performExport()
	)
)

-- create the export window with any options
if stoogeWindow != undefined do
(
	closerolloutfloater stoogeWindow 
)
stoogeWindow = newRolloutFloater "Stooge" 200 120
addRollout stoogeExport stoogeWindow