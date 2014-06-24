/*
=================================================================================
This file is part of Cafu, the open-source game engine and graphics engine
for multiplayer, cross-platform, real-time 3D action.
Copyright (C) 2002-2014 Carsten Fuchs Software.

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

#ifndef CAFU_GUIEDITOR_LIVE_PREVIEW_HPP_INCLUDED
#define CAFU_GUIEDITOR_LIVE_PREVIEW_HPP_INCLUDED

#include "Templates/Pointer.hpp"
#include "wx/wx.h"


namespace cf { class UniScriptStateT; }
namespace cf { namespace GuiSys { class GuiImplT; } }


namespace GuiEditor
{
    class PreviewCanvasT;


    class LivePreviewT : public wxDialog
    {
        public:

        LivePreviewT(wxWindow* Parent, cf::UniScriptStateT* ScriptState, IntrusivePtrT<cf::GuiSys::GuiImplT> Gui, const wxString& ScriptFileName);
        ~LivePreviewT();

        IntrusivePtrT<cf::GuiSys::GuiImplT> GetGui() { return m_Gui; }


        private:

        cf::UniScriptStateT*                m_ScriptState;
        IntrusivePtrT<cf::GuiSys::GuiImplT> m_Gui;
        PreviewCanvasT*                     m_Canvas;

        void OnClose(wxCloseEvent& CE);

        DECLARE_EVENT_TABLE()
    };
}

#endif
