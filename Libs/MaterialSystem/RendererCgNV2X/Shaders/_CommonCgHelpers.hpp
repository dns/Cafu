/*
=================================================================================
This file is part of Cafu, the open-source game engine and graphics engine
for multiplayer, cross-platform, real-time 3D action.
Copyright (C) 2002-2013 Carsten Fuchs Software.

Cafu is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

Cafu is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Cafu. If not, see <http://www.gnu.org/licenses/>.

For support and more information about Cafu, visit us at <http://www.cafu.de>.
=================================================================================
*/

/*************************/
/*** Common Cg Helpers ***/
/*************************/

#ifndef CAFU_MATSYS_COMMON_CG_HELPERS_HPP_INCLUDED
#define CAFU_MATSYS_COMMON_CG_HELPERS_HPP_INCLUDED

// This is required for cg.h to get the function calling conventions (Win32 import/export/lib) right.
#ifdef _WIN32
#define WIN32 1
#endif

#include <Cg/cg.h>
#include <Cg/cgGL.h>


CGprogram UploadCgProgram(CGcontext CgContext, CGprofile Profile, const char* SourceCode, bool IsObjectCode=false);

#endif