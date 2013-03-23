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

#ifndef CAFU_GUIEDITOR_WINDOW_INSPECTOR_HPP_INCLUDED
#define CAFU_GUIEDITOR_WINDOW_INSPECTOR_HPP_INCLUDED

#include "ObserverPattern.hpp"

#include "wx/wx.h"
#include "wx/propgrid/manager.h"


namespace cf { namespace GuiSys { class ComponentBaseT; } }
namespace cf { namespace GuiSys { class WindowT; } }
namespace cf { namespace TypeSys { class VarBaseT; } }


namespace GuiEditor
{
    class ChildFrameT;
    class GuiDocumentT;

    class WindowInspectorT : public wxPropertyGridManager, public ObserverT
    {
        public:

        WindowInspectorT(ChildFrameT* Parent, const wxSize& Size);

        // ObserverT implementation.
        void NotifySubjectChanged_Selection(SubjectT* Subject, const ArrayT< IntrusivePtrT<cf::GuiSys::WindowT> >& OldSelection, const ArrayT< IntrusivePtrT<cf::GuiSys::WindowT> >& NewSelection);
        void NotifySubjectChanged_Deleted(SubjectT* Subject, const ArrayT< IntrusivePtrT<cf::GuiSys::WindowT> >& Windows);
        void NotifySubjectChanged_Modified(SubjectT* Subject, const ArrayT< IntrusivePtrT<cf::GuiSys::WindowT> >& Windows, WindowModDetailE Detail);
        void Notify_Changed(SubjectT* Subject, const cf::TypeSys::VarBaseT& Var);
        void NotifySubjectDies(SubjectT* dyingSubject);

        void RefreshPropGrid();


        private:

        GuiDocumentT*                      m_GuiDocument;
        ChildFrameT*                       m_Parent;
        IntrusivePtrT<cf::GuiSys::WindowT> m_SelectedWindow;
        bool                               m_IsRecursiveSelfNotify;

        void AppendComponent(IntrusivePtrT<cf::GuiSys::ComponentBaseT> Comp);

        void OnPropertyGridChanging(wxPropertyGridEvent& Event);
        void OnPropertyGridChanged(wxPropertyGridEvent& Event);
        void OnPropertyGridRightClick(wxPropertyGridEvent& Event);

        DECLARE_EVENT_TABLE()
    };
}

#endif
