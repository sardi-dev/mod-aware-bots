/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

// Boost.JSON in header-only mode: the image installs Boost headers only (docker/Dockerfile: b2 --with-headers), so
// the library's implementation is compiled here, once, for the whole worldserver.
#include <boost/json/src.hpp>
