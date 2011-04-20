/*
=================================================================================
This file is part of Cafu, the open-source game engine and graphics engine
for multiplayer, cross-platform, real-time 3D action.
Copyright (C) 2002-2011 Carsten Fuchs Software.

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

#include "ElementsList.hpp"
#include "ChildFrame.hpp"
#include "ModelDocument.hpp"
#include "Commands/Select.hpp"
#include "Models/Model_cmdl.hpp"


using namespace ModelEditor;


BEGIN_EVENT_TABLE(ElementsListT, wxListView)
 // EVT_CONTEXT_MENU(ElementsListT::OnContextMenu)
 // EVT_LIST_KEY_DOWN       (wxID_ANY, ElementsListT::OnKeyDown)
    EVT_LIST_ITEM_SELECTED  (wxID_ANY, ElementsListT::OnSelectionChanged)
    EVT_LIST_ITEM_DESELECTED(wxID_ANY, ElementsListT::OnSelectionChanged)
 // EVT_LIST_END_LABEL_EDIT (wxID_ANY, ElementsListT::OnEndLabelEdit)
END_EVENT_TABLE()


ElementsListT::ElementsListT(ChildFrameT* Parent, const wxSize& Size, ModelElementTypeT Type)
    : wxListView(Parent, wxID_ANY, wxDefaultPosition, Size, wxLC_REPORT /*| wxLC_EDIT_LABELS*/),
      m_TYPE(Type),
      m_ModelDoc(Parent->GetModelDoc()),
      m_Parent(Parent),
      m_IsRecursiveSelfNotify(false)
{
    wxASSERT(m_TYPE==ANIM || m_TYPE==MESH);

    // TODO: Make it up to the caller code to call this?
    // // As we are now a wxAUI pane rather than a wxDialog, explicitly set that events are not propagated to our parent.
    // SetExtraStyle(wxWS_EX_BLOCK_EVENTS);

    InsertColumn(0, "#");
    InsertColumn(1, "Name");

    m_ModelDoc->RegisterObserver(this);
    InitListItems();
}


ElementsListT::~ElementsListT()
{
    if (m_ModelDoc)
        m_ModelDoc->UnregisterObserver(this);
}


void ElementsListT::Notify_SelectionChanged(SubjectT* Subject, ModelElementTypeT Type, const ArrayT<unsigned int>& OldSel, const ArrayT<unsigned int>& NewSel)
{
    if (m_IsRecursiveSelfNotify) return;
    if (Type!=m_TYPE) return;

    m_IsRecursiveSelfNotify=true;
    Freeze();

    for (long SelNr=GetFirstSelected(); SelNr!=-1; SelNr=GetNextSelected(SelNr))
        Select(SelNr, false);

    for (unsigned long SelNr=0; SelNr<NewSel.Size(); SelNr++)
        Select(NewSel[SelNr]);

    Thaw();
    m_IsRecursiveSelfNotify=false;
}


void ElementsListT::Notify_SubjectDies(SubjectT* dyingSubject)
{
    wxASSERT(dyingSubject==m_ModelDoc);

    m_ModelDoc=NULL;

    DeleteAllItems();
}


void ElementsListT::InitListItems()
{
    const unsigned long         NumElems=(m_TYPE==MESH) ? m_ModelDoc->GetModel()->GetMeshes().Size() : m_ModelDoc->GetModel()->GetAnims().Size();
    const ArrayT<unsigned int>& Sel     =m_ModelDoc->GetSelection(MESH);

    DeleteAllItems();

    for (unsigned long ElemNr=0; ElemNr<NumElems; ElemNr++)
    {
        InsertItem(ElemNr, wxString::Format("%lu", ElemNr));
        SetItem(ElemNr, 1, wxString(m_TYPE==MESH ? "Mesh" : "Anim") + wxString::Format(" %lu", ElemNr));

        if (Sel.Find(ElemNr)!=-1) Select(ElemNr);
    }
}


/*void ElementsListT::OnContextMenu(wxContextMenuEvent& CE)
{
}*/


/*void ElementsListT::OnKeyDown(wxListEvent& LE)
{
    switch (LE.GetKeyCode())
    {
        case WXK_F2:
        {
            const long SelNr=LE.GetIndex();

            if (SelNr!=-1) EditLabel(SelNr);
            break;
        }

        default:
            LE.Skip();
            break;
    }
}*/


void ElementsListT::OnSelectionChanged(wxListEvent& LE)
{
    if (m_ModelDoc==NULL) return;
    if (m_IsRecursiveSelfNotify) return;

    m_IsRecursiveSelfNotify=true;

    // Get the currently selected list items and update the document selection accordingly.
    ArrayT<unsigned int> NewSel;

    for (long SelNr=GetFirstSelected(); SelNr!=-1; SelNr=GetNextSelected(SelNr))
        NewSel.PushBack(SelNr);

    m_Parent->SubmitCommand(CommandSelectT::Set(m_ModelDoc, m_TYPE, NewSel));

    m_IsRecursiveSelfNotify=false;
}


/*void ElementsListT::OnEndLabelEdit(wxListEvent& LE)
{
    const unsigned long Index=LE.GetIndex();

    if (LE.IsEditCancelled()) return;
    // if (Index>=m_ModelDoc->GetGroups().Size()) return;

    m_IsRecursiveSelfNotify=true;
    // m_ModelDoc->GetHistory().SubmitCommand(new CommandGroupSetPropT(*MapDoc, MapDoc->GetGroups()[Index], LE.GetLabel()));
    m_IsRecursiveSelfNotify=false;
}*/