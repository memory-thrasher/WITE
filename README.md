# WITE
A multi-thread 3D game engine using vulkan and optomized for precedural graphics.

I'm making this for myself. If it's useful to you, that's great (see separate license file). I never had any intention on supporting anyone else's use of this engine in other software. You're welcome to try, but you're on your own.

At the time of this writing, the engine is definitely not ready for distribution to consumers.

The Waffle Iron Transactional Engine (WITE) engine has evolved quite a bit since it's initial conception. The transactional aspect has been reduced to how host memory and save files are handled. The Database object manages that. Synchronization of graphical operations uses memory barriers and double-buffering (called frameswapping to allow for numbers of buffers other than 2).


