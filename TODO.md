fix submit commandbuffer function
add a way to get the config from a handle
test double buffering
a game kinda like krunker but every gun has knowckback

TODO: separete renderer cpp and hpp

each resource when created should get a handle
if the first pass that accesses a resource is read, the resource is thought to be already shader readable state (buffers are prefilled, images are undefined or are textures)
if the first pass that accesses a resource is write, the resource data is thought to be undefined state

this is done to create a resource table, the resource table will be used to insert barriers and transitions

if a pass that accesses a resource to write to it, if the resource is not already in writing state,     
    for image:
        if the previous known state was writing, nothing will happen
        if the previous known state was reading, barrier inserted + transition 
        if the previous known state was undefined, transition
