# SusEngine

![screenshot](screenshots/ss2.png)

Small game engine written entirely in C using raylib. It has its own Entity Component System and functionality for making simple 3D games.

I made it for fun and to have an easy to use platform for quickly prototyping games.


### Implemented Features
- Lua support: a script can alter their belonging entity or even fully create new ones
- Components lifecycle handling (create, update, pre/post rendering, destroy)
- The user is free to implement their own callback functions to a component instance.
- Frustum culling
- Cascaded Shadow Mapping
- Basic rigid body dynamics system (uses GJK and EPA for collision resolution)
- Collision shapes: convex hull, heightmap
- Raycasting
- Basic rendering system, separated from the rest of the engine
- Components:
	- **Info**: stores general-purpose entity information such as bit mask. For example, determine if the entity is a zombie, human, player, shopkeeper etc.
	- **Transform**: entity position, scale and rotation. Can have another entity as anchor.
	- **Camera**: raylib Camera3D container
	- **Mesh Renderer**: stores the ID of a model and a shader to be renderered with, along used transform ID and other visual parameters such as opacity and color.
	- **Light Source**: ambient, directional or point type. Can also have any transform as reference for the point light.
	- **Script**: stores a Lua state. The create, update and destroy callbacks executed on the Script component are passed to the Lua state.
	- **Rigidbody**: body-related parameters such as position, rotation (overrides Transform attributes), mass, friction, angular velocity etc.
	- **Collider**: can have another transform matrix applied to the Transform component's one when performing collision detection. Also stores a bit mask for itself and other colliders to check against, as well as the contacts with other Collider components (position, normal and entity IDs).
- All attributes of the components can be directly modified without calling additional functions, except the ones starting with underscore, denoting their internal state.
- Inter-entity messaging

### TODO list:
 - Improve Lua bindings. Also add raygui?
 - Lua functions to create meshes and load assets, basically everything needed to make a game entirely using Lua scripts
 - Bring inter-entity messaging to Lua as well
 - Mesh Renderer LOD system
 - AABB and Sphere collision detection
 - Make the front-end even simpler
 - Documentation!!!
 - Demo game, or anything else creative