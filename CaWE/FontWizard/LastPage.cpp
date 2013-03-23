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

#include "LastPage.hpp"

#include "FontWizard.hpp"


BEGIN_EVENT_TABLE(LastPageT, wxWizardPageSimple)
    EVT_WIZARD_PAGE_CHANGED(wxID_ANY, LastPageT::OnWizardPageChanged)
    EVT_WIZARD_PAGE_CHANGING(wxID_ANY, LastPageT::OnWizardPageChanging)
END_EVENT_TABLE()


LastPageT::LastPageT(FontWizardT* Parent)
    : wxWizardPageSimple(Parent),
      m_Parent(Parent)
{
    // Note: The following code was generated by wxFormBuilder and copied here manually because wxFormbuilder
    // doesn't support wxWizardPage as base class.

	wxBoxSizer* bSizer12;
	bSizer12 = new wxBoxSizer( wxVERTICAL );

	wxStaticText* m_staticText12;
	m_staticText12 = new wxStaticText( this, wxID_ANY, wxT("The font has been successfully created."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText12->Wrap( -1 );
	bSizer12->Add( m_staticText12, 0, wxALL, 5 );


	bSizer12->Add( 0, 15, 0, 0, 5 );

	wxBoxSizer* bSizer13;
	bSizer13 = new wxBoxSizer( wxVERTICAL );

	wxStaticText* m_staticText13;
	m_staticText13 = new wxStaticText( this, wxID_ANY, wxT("To use the font in the GUI editor specifiy"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText13->Wrap( -1 );
	bSizer13->Add( m_staticText13, 0, wxALL, 5 );

	m_FontName = new wxStaticText( this, wxID_ANY, wxT("***FontName***"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
	m_FontName->Wrap( -1 );
	m_FontName->SetFont( wxFont( 10, 75, 90, 92, false, wxEmptyString ) );

	bSizer13->Add( m_FontName, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 5 );

	wxStaticText* m_staticText15;
	m_staticText15 = new wxStaticText( this, wxID_ANY, wxT("as font name for a GUI element."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText15->Wrap( -1 );
	bSizer13->Add( m_staticText15, 0, wxALL, 5 );

	bSizer12->Add( bSizer13, 1, wxEXPAND, 5 );

	this->SetSizer( bSizer12 );
	this->Layout();
}


void LastPageT::OnWizardPageChanged(wxWizardEvent& WE)
{
    m_FontName->SetLabel("Fonts/"+m_Parent->FontName);

    wxWindow* CancelButton=FindWindowById(wxID_CANCEL, m_Parent);
    wxASSERT(CancelButton!=NULL);
    CancelButton->Enable(false);

    wxWindow* BackButton=FindWindowById(wxID_BACKWARD, m_Parent);
    wxASSERT(BackButton!=NULL);
    BackButton->Enable(false);
}


void LastPageT::OnWizardPageChanging(wxWizardEvent& WE)
{
    // Going back from this page is impossible.
    if (!WE.GetDirection())
    {
        wxASSERT(false); // Should never happen since button is inactive.
        WE.Veto();
        return;
    }
}
