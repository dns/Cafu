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

#include "Rotate.hpp"

#include "../GuiDocument.hpp"

#include "GuiSys/Window.hpp"


using namespace GuiEditor;


CommandRotateT::CommandRotateT(GuiDocumentT* GuiDocument, const ArrayT<cf::GuiSys::WindowT*>& Windows, float Rotation, bool Done)
    : m_GuiDocument(GuiDocument),
      m_Windows(Windows)
{
    m_Done=Done;

    if (m_Done)
    {
        for (unsigned long WinNr=0; WinNr<m_Windows.Size(); WinNr++)
        {
            float OldRotation=m_Windows[WinNr]->RotAngle-Rotation;

            while (OldRotation<  0.0f) OldRotation=360.0f+OldRotation;
            while (OldRotation>360.0f) OldRotation=OldRotation-360.0f;

            m_OldRotations.PushBack(OldRotation);
            m_NewRotations.PushBack(m_Windows[WinNr]->RotAngle);
        }
    }
    else
    {
        for (unsigned long WinNr=0; WinNr<m_Windows.Size(); WinNr++)
        {
            float NewRotation=m_Windows[WinNr]->RotAngle+Rotation;

            while (NewRotation<  0.0f) NewRotation=360.0f+NewRotation;
            while (NewRotation>360.0f) NewRotation=NewRotation-360.0f;

            m_OldRotations.PushBack(m_Windows[WinNr]->RotAngle);
            m_NewRotations.PushBack(NewRotation);
        }
    }
}


bool CommandRotateT::Do()
{
    wxASSERT(!m_Done);

    if (m_Done) return false;

    for (unsigned long WinNr=0; WinNr<m_Windows.Size(); WinNr++)
        m_Windows[WinNr]->RotAngle=m_NewRotations[WinNr];

    m_GuiDocument->UpdateAllObservers_Modified(m_Windows, WMD_TRANSFORMED);

    m_Done=true;

    return true;
}


void CommandRotateT::Undo()
{
    wxASSERT(m_Done);

    if (!m_Done) return;

    for (unsigned long WinNr=0; WinNr<m_Windows.Size(); WinNr++)
        m_Windows[WinNr]->RotAngle=m_NewRotations[WinNr];

    m_GuiDocument->UpdateAllObservers_Modified(m_Windows, WMD_TRANSFORMED);

    m_Done=false;
}


wxString CommandRotateT::GetName() const
{
    return "Rotate windows";
}