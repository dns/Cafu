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

#include "DialogInspector.hpp"
#include "DialogInsp-PrimitiveProps.hpp"
#include "DialogEntityInspector.hpp"
#include "MapDocument.hpp"
#include "MapEntRepres.hpp"

#include "wx/notebook.h"


using namespace MapEditor;


InspectorDialogT::InspectorDialogT(wxWindow* Parent, MapDocumentT* MapDoc)
    : wxPanel(Parent, -1),
      Notebook(NULL),
      m_EntityInspectorDialog(NULL)
{
    wxSizer* mainSizer=new wxBoxSizer(wxVERTICAL);

    Notebook                = new wxNotebook(this, -1, wxDefaultPosition, wxSize(350, 450));
    m_EntityInspectorDialog = new EntityInspectorDialogT(Notebook, MapDoc->GetChildFrame(), wxSize(300, 200));
    PrimitiveProps          = new InspDlgPrimitivePropsT(Notebook, MapDoc);

    Notebook->AddPage(m_EntityInspectorDialog, "Entity");
    Notebook->AddPage(PrimitiveProps,          "Primitive");

    mainSizer->Add(Notebook, 1, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 2);

    this->SetSizer(mainSizer);
    mainSizer->SetSizeHints(this);
}


void InspectorDialogT::ChangePage(int Page)
{
    Notebook->ChangeSelection(Page);
}


int InspectorDialogT::GetBestPage(const ArrayT<MapElementT*>& Selection) const
{
    if (Selection.Size()==0)
    {
        // Nothing is selected, so just show the scene graph.
        return 0;
    }
    else if (Selection.Size()==1)
    {
        MapElementT* MapElement=Selection[0];

        return (MapElement->GetType() == &MapEntRepresT::TypeInfo) ? 0 : 1;
    }
    else
    {
        // Multiple map elements are selected.
        // If we have only map primitives, open the Primitive Properties tab
        // (even though it doesn't make much sense - primitive properties can be edited for a single item only).
        // One or more entities in the selection cause us to open the Entitiy Properties tab.
        bool HaveEntities=false;

        for (unsigned long SelNr=0; SelNr<Selection.Size(); SelNr++)
        {
            MapElementT* MapElement=Selection[SelNr];

            if (MapElement->GetType() == &MapEntRepresT::TypeInfo)
            {
                HaveEntities=true;
                break;
            }
        }

        return HaveEntities ? 0 : 1;
    }
}
