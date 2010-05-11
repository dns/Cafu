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

#ifndef _MODELEDITOR_MODEL_DOCUMENT_HPP_
#define _MODELEDITOR_MODEL_DOCUMENT_HPP_

#include "wx/wx.h"


class CafuModelT;
class GameConfigT;


namespace ModelEditor
{
    class ModelDocumentT
    {
        public:

        /// The constructor.
        /// @throws   ModelT::LoadError if the model could not be loaded or imported.
        ModelDocumentT(GameConfigT* GameConfig, const wxString& ModelFileName="");

        /// The destructor.
        ~ModelDocumentT();

        CafuModelT*  GetModel() const { return m_Model; }
        GameConfigT* GetGameConfig() const { return m_GameConfig; }


        private:

        ModelDocumentT(const ModelDocumentT&);      ///< Use of the Copy    Constructor is not allowed.
        void operator = (const ModelDocumentT&);    ///< Use of the Assignment Operator is not allowed.

        CafuModelT*  m_Model;
        GameConfigT* m_GameConfig;
    };
}

#endif