/*
new rendering system:
every renderable must provide a depthmap-only shader
system produces a map of object ids via depth-only and then renders specific objects to specific pixels
any object update can call render (async) i.e. reflection map

problems:
lighting
common shared resource read/write timing

maybe:
always renders to offscreen image and then blit to presentation

*/
