/*
=================================================================================
This file is part of Cafu, the open-source game and graphics engine for
multiplayer, cross-platform, real-time 3D action.
$Id$

Copyright (C) 2002-2010 Carsten Fuchs Software.

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

#ifndef _GUIEDITOR_COMMAND_ALIGN_TEXT_HPP_
#define _GUIEDITOR_COMMAND_ALIGN_TEXT_HPP_

#include "../CommandPattern.hpp"

#include "Templates/Array.hpp"


namespace cf { namespace GuiSys { class WindowT; } }


namespace GuiEditor
{
    class GuiDocumentT;

    class CommandAlignTextHorT : public CommandT
    {
        public:

        CommandAlignTextHorT(GuiDocumentT* GuiDocument, const ArrayT<cf::GuiSys::WindowT*>& Windows, int Alignment);

        // CommandT implementation.
        bool Do();
        void Undo();
        wxString GetName() const;


        private:

        GuiDocumentT*                m_GuiDocument;
        ArrayT<cf::GuiSys::WindowT*> m_Windows;
        ArrayT<int>                  m_OldAlignments;
        int                          m_NewAlignment;
    };
}

#endif