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

#ifndef CAFU_CLIENT_STATE_IDLE_HPP_INCLUDED
#define CAFU_CLIENT_STATE_IDLE_HPP_INCLUDED

#include "ClientState.hpp"
#include "Fonts/Font.hpp"


class ClientT;


/// This class implements the idle state of the client.
class ClientStateIdleT : public ClientStateT
{
    public:

    ClientStateIdleT(ClientT& Client_);

    // Implement the ClientStateT interface.
    int GetID() const override;
    bool ProcessInputEvent(const CaKeyboardEventT& KE) override;
    bool ProcessInputEvent(const CaMouseEventT& ME) override;
    void Render(float FrameTime) override;
    void MainLoop(float FrameTime) override;


    private:

    ClientT& Client;    ///< The context this state is a part of.
};

#endif
