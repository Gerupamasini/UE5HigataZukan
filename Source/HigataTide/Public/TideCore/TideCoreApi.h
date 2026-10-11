#pragma once

// Unreal defines HIGATATIDE_API for the HigataTide module (dllexport inside it, dllimport in modules that use it),
// so the core's functions can be called from the game module. The standalone tests build without Unreal: empty.
#ifndef HIGATATIDE_API
#define HIGATATIDE_API
#endif
