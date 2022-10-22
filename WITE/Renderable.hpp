/*
new rendering system:
any object update can call render (async) i.e. reflection map

maybe:
always renders to offscreen image and then blit to presentation
every renderable must provide a depthmap-only shader
system produces a map of object ids via depth-only and then renders specific objects to specific pixels

*/
