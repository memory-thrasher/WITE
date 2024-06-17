# WITE
A multi-thread 3D game engine using vulkan and optomized for precedural graphics.

I'm making this for myself. If it's useful to you, that's great (see separate license file). I never had any intention on supporting anyone else's use of this engine in other software. You're welcome to try, but you're on your own.

The Waffle Iron Transactional Engine (WITE) engine has evolved quite a bit since it's initial conception. The transactional aspect has been reduced to how host memory and save files are handled. The Database object manages that. Synchronization of graphical operations uses memory barriers and double-buffering (called frameswapping to include for numbers of buffers other than 2).







# Legal
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
