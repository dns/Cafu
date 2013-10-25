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

#include "Entity.hpp"
#include "AllComponents.hpp"
#include "CompBase.hpp"
#include "EntityCreateParams.hpp"
#include "World.hpp"

#include "Network/State.hpp"
#include "TypeSys.hpp"

extern "C"
{
    #include <lua.h>
    #include <lualib.h>
    #include <lauxlib.h>
}

#include <cassert>

#if defined(_WIN32) && _MSC_VER<1600
#include "pstdint.h"            // Paul Hsieh's portable implementation of the stdint.h header.
#else
#include <stdint.h>
#endif


using namespace cf::GameSys;


// Note that we cannot simply replace this method with a global TypeInfoManT instance,
// because it is called during global static initialization time. The TIM instance being
// embedded in the function guarantees that it is properly initialized before first use.
cf::TypeSys::TypeInfoManT& cf::GameSys::GetGameSysEntityTIM()
{
    static cf::TypeSys::TypeInfoManT TIM;

    return TIM;
}


const char* EntityT::DocClass =
    "An entity is the basic element in a game world.\n"
    "\n"
    "Entity can be hierarchically arranged in parent/child relationships, e.g. a player that rides a car.\n"
    "\n"
    "An entity is a separate unit that is self-contained and has its own identity, but has very little own features.\n"
    "Instead, an entity contains a set of components, each of which implements a specific feature for the entity.";


EntityT::EntityT(const EntityCreateParamsT& Params)
    : m_World(Params.World),
      m_ID(m_World.GetNextEntityID(Params.GetID())),
      m_Parent(NULL),
      m_Children(),
      m_App(NULL),
      m_Basics(new ComponentBasicsT()),
      m_Transform(new ComponentTransformT()),
      m_Components()
{
    m_Basics->UpdateDependencies(this);
    m_Transform->UpdateDependencies(this);

    // m_Transform->SetOrigin(Vector3fT(0.0f, 0.0f, 0.0f));
}


EntityT::EntityT(const EntityT& Entity, bool Recursive)
    : m_World(Entity.m_World),
      m_ID(m_World.GetNextEntityID()),
      m_Parent(NULL),
      m_Children(),
      m_App(NULL),
      m_Basics(Entity.GetBasics()->Clone()),
      m_Transform(Entity.GetTransform()->Clone()),
      m_Components()
{
    if (Entity.GetApp() != NULL)
    {
        m_App = Entity.GetApp()->Clone();
        m_App->UpdateDependencies(this);
    }

    m_Basics->UpdateDependencies(this);
    m_Transform->UpdateDependencies(this);

    // Copy-create all components first.
    m_Components.PushBackEmptyExact(Entity.GetComponents().Size());

    for (unsigned int CompNr = 0; CompNr < Entity.GetComponents().Size(); CompNr++)
        m_Components[CompNr] = Entity.GetComponents()[CompNr]->Clone();

    // Now that all components have been copied, have them resolve their dependencies among themselves.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        m_Components[CompNr]->UpdateDependencies(this);

    // Recursively copy the children.
    if (Recursive)
    {
        for (unsigned long ChildNr=0; ChildNr<Entity.m_Children.Size(); ChildNr++)
        {
            m_Children.PushBack(Entity.m_Children[ChildNr]->Clone(Recursive));
            m_Children[ChildNr]->m_Parent=this;
        }
    }
}


EntityT* EntityT::Clone(bool Recursive) const
{
    return new EntityT(*this, Recursive);
}


EntityT::~EntityT()
{
    // Cannot have a parent any more. Otherwise, the parent still had
    // us as a child, and we should not have gotten here for destruction.
    assert(m_Parent == NULL);

    // Cleanly disconnect our children.
    for (unsigned long ChildNr=0; ChildNr<m_Children.Size(); ChildNr++)
        m_Children[ChildNr]->m_Parent = NULL;

    m_Children.Clear();

    // Delete the components.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
    {
        // No one else should still have a pointer to m_Components[CompNr] at this point.
        // Nevertheless, it is still possible that m_Components[CompNr]->GetRefCount() > 1,
        // for example if a script still keeps a reference to the component, or has kept
        // one that is not yet garbage collected.
        // To be safe, make sure that the component no longer refers back to this entity.
        m_Components[CompNr]->UpdateDependencies(NULL);
    }

    m_Components.Clear();

    if (!m_App.IsNull()) m_App->UpdateDependencies(NULL);
    m_Basics->UpdateDependencies(NULL);
    m_Transform->UpdateDependencies(NULL);
}


bool EntityT::AddChild(IntrusivePtrT<EntityT> Child, unsigned long Pos)
{
    assert(Child->m_Parent == NULL);
    if (Child->m_Parent != NULL)    // A child entity must be a root node...
        return false;   // luaL_argerror(LuaState, 2, "child entity already has a parent, use RemoveChild() first");

    assert(Child != GetRoot());
    if (Child == GetRoot())         // ... but not the root of the hierarchy it is inserted into.
        return false;   // luaL_argerror(LuaState, 2, "an entity cannot be made a child of itself");

    m_Children.InsertAt(std::min(m_Children.Size(), Pos), Child);
    Child->m_Parent = this;

    // Make sure that the childs name is unique among its siblings.
    Child->GetBasics()->SetEntityName(Child->GetBasics()->GetEntityName());
    return true;
}


bool EntityT::RemoveChild(IntrusivePtrT<EntityT> Child)
{
    assert(Child->m_Parent == this);
    if (Child->m_Parent != this)
        return false;   // luaL_argerror(LuaState, 2, "entity is the child of another parent");

    const int Index = m_Children.Find(Child);

    assert(Index >= 0);
    if (Index < 0)
        return false;   // return luaL_argerror(LuaState, 2, "entity not found among the children of its parent");

    m_Children.RemoveAtAndKeepOrder(Index);
    Child->m_Parent = NULL;
    return true;
}


void EntityT::GetChildren(ArrayT< IntrusivePtrT<EntityT> >& Chld, bool Recurse) const
{
#ifdef DEBUG
    // Make sure that there are no cycles in the hierarchy of children.
    for (unsigned long ChildNr=0; ChildNr<m_Children.Size(); ChildNr++)
        assert(Chld.Find(m_Children[ChildNr]) == -1);
#endif

    Chld.PushBack(m_Children);

    if (!Recurse) return;

    for (unsigned long ChildNr=0; ChildNr<m_Children.Size(); ChildNr++)
        m_Children[ChildNr]->GetChildren(Chld, Recurse);
}


void EntityT::GetAll(ArrayT< IntrusivePtrT<EntityT> >& List)
{
    List.PushBack(this);

    for (unsigned int ChildNr = 0; ChildNr < m_Children.Size(); ChildNr++)
        m_Children[ChildNr]->GetAll(List);
}


IntrusivePtrT<EntityT> EntityT::GetRoot()
{
    EntityT* Root = this;

    while (Root->m_Parent)
        Root = Root->m_Parent;

    return Root;
}


void EntityT::SetApp(IntrusivePtrT<ComponentBaseT> App)
{
    if (m_App == App) return;

    if (!m_App.IsNull()) m_App->UpdateDependencies(NULL);
    m_App = App;
    if (!m_App.IsNull()) m_App->UpdateDependencies(this);
}


IntrusivePtrT<ComponentBaseT> EntityT::GetComponent(const std::string& TypeName, unsigned int n) const
{
    if (m_App != NULL && TypeName == m_App->GetName())
    {
        if (n == 0) return m_App;
        n--;
    }

    if (TypeName == m_Basics->GetName())
    {
        if (n == 0) return m_Basics;
        n--;
    }

    if (TypeName == m_Transform->GetName())
    {
        if (n == 0) return m_Transform;
        n--;
    }

    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        if (m_Components[CompNr]->GetName() == TypeName)
        {
            if (n == 0) return m_Components[CompNr];
            n--;
        }

    return NULL;
}


bool EntityT::AddComponent(IntrusivePtrT<ComponentBaseT> Comp, unsigned long Index)
{
    if (Comp->GetEntity()) return false;
    assert(m_Components.Find(Comp) < 0);

    m_Components.InsertAt(std::min(Index, m_Components.Size()), Comp);

    // Have the components re-resolve their dependencies among themselves.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        m_Components[CompNr]->UpdateDependencies(this);

    return true;
}


void EntityT::DeleteComponent(unsigned long CompNr)
{
    // Let the component know that it is no longer a part of this entity.
    m_Components[CompNr]->UpdateDependencies(NULL);

    m_Components.RemoveAtAndKeepOrder(CompNr);

    // Have the remaining components re-resolve their dependencies among themselves.
    for (CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        m_Components[CompNr]->UpdateDependencies(this);
}


IntrusivePtrT<EntityT> EntityT::FindID(unsigned int WantedID)
{
    if (WantedID == m_ID) return this;

    // Recursively see if any of the children has the desired name.
    for (unsigned int ChildNr = 0; ChildNr < m_Children.Size(); ChildNr++)
    {
        IntrusivePtrT<EntityT> Ent = m_Children[ChildNr]->FindID(WantedID);

        if (Ent != NULL) return Ent;
    }

    return NULL;
}


IntrusivePtrT<EntityT> EntityT::Find(const std::string& WantedName)
{
    if (WantedName == m_Basics->GetEntityName()) return this;

    // Recursively see if any of the children has the desired name.
    for (unsigned long ChildNr=0; ChildNr<m_Children.Size(); ChildNr++)
    {
        IntrusivePtrT<EntityT> Ent = m_Children[ChildNr]->Find(WantedName);

        if (Ent != NULL) return Ent;
    }

    return NULL;
}


// Note that this method is the twin of Deserialize(), whose implementation it must match.
void EntityT::Serialize(cf::Network::OutStreamT& Stream, bool WithChildren) const
{
 // m_App->Serialize(Stream);       // Don't serialize anything that is application-specific.
    m_Basics->Serialize(Stream);
    m_Transform->Serialize(Stream);

    // Serialize the "custom" components.
    assert(m_Components.Size() < 256);
    Stream << uint8_t(m_Components.Size());

    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
    {
        assert(m_Components[CompNr]->GetType()->TypeNr < 256);
        Stream << uint8_t(m_Components[CompNr]->GetType()->TypeNr);

        m_Components[CompNr]->Serialize(Stream);
    }

    // Recursively serialize the children (if requested).
    Stream << WithChildren;

    if (WithChildren)
    {
        Stream << uint32_t(m_Children.Size());

        for (unsigned int ChildNr = 0; ChildNr < m_Children.Size(); ChildNr++)
            m_Children[ChildNr]->Serialize(Stream, WithChildren);
    }
}


// Note that this method is the twin of Serialize(), whose implementation it must match.
void EntityT::Deserialize(cf::Network::InStreamT& Stream, bool IsIniting)
{
 // m_App->Deserialize(Stream, IsIniting);
    m_Basics->Deserialize(Stream, IsIniting);
    m_Transform->Deserialize(Stream, IsIniting);

    // Deserialize the "custom" components.
    uint8_t NumComponents = 0;
    Stream >> NumComponents;

    while (m_Components.Size() > NumComponents)
        m_Components.DeleteBack();

    for (unsigned int CompNr = 0; CompNr < NumComponents; CompNr++)
    {
        uint8_t CompTypeNr = 0;
        Stream >> CompTypeNr;

        if (CompNr >= m_Components.Size())
        {
            // Note that if `TI == NULL`, there really is not much that we can do.
            const cf::TypeSys::TypeInfoT* TI = GetComponentTIM().FindTypeInfoByNr(CompTypeNr);
            IntrusivePtrT<ComponentBaseT> Comp(static_cast<ComponentBaseT*>(TI->CreateInstance(cf::TypeSys::CreateParamsT())));

            m_Components.PushBack(Comp);
        }

        if (m_Components[CompNr]->GetType()->TypeNr != CompTypeNr)
        {
            // Note that if `TI == NULL`, there really is not much that we can do.
            const cf::TypeSys::TypeInfoT* TI = GetComponentTIM().FindTypeInfoByNr(CompTypeNr);
            IntrusivePtrT<ComponentBaseT> Comp(static_cast<ComponentBaseT*>(TI->CreateInstance(cf::TypeSys::CreateParamsT())));

            m_Components[CompNr] = Comp;
        }

        m_Components[CompNr]->Deserialize(Stream, IsIniting);
    }

    // Recursively deserialize the children (if requested).
    bool WithChildren = false;
    Stream >> WithChildren;

    if (WithChildren)
    {
        uint32_t NumChildren = 0;
        Stream >> NumChildren;

        while (m_Children.Size() > NumChildren)
            m_Children.DeleteBack();

        for (unsigned int ChildNr = 0; ChildNr < NumChildren; ChildNr++)
        {
            if (ChildNr >= m_Children.Size())
            {
                IntrusivePtrT<EntityT> NewEnt = new EntityT(EntityCreateParamsT(m_World));

                m_Children.PushBack(NewEnt);
            }

            m_Children[ChildNr]->Deserialize(Stream, IsIniting);
        }
    }
}


MatrixT EntityT::GetModelToWorld() const
{
    MatrixT ModelToWorld(m_Transform->GetOrigin(), m_Transform->GetQuat());

    for (const EntityT* P = m_Parent; P; P = P->m_Parent)
        if (!P->m_Transform->IsIdentity())
            ModelToWorld = MatrixT(P->m_Transform->GetOrigin(), P->m_Transform->GetQuat()) * ModelToWorld;

    return ModelToWorld;
}


void EntityT::RenderComponents(float LodDist) const
{
    if (!m_Basics->IsShown()) return;

    // Render the "custom" components in the proper order -- bottom-up.
    for (unsigned long CompNr = m_Components.Size(); CompNr > 0; CompNr--)
        m_Components[CompNr-1]->Render(LodDist);

    // Render the "fixed" components.
    // m_Transform->Render(LodDist);
    // m_Basics->Render(LodDist);
    if (m_App != NULL) m_App->Render(LodDist);
}


bool EntityT::OnInputEvent(const CaKeyboardEventT& KE)
{
    if (m_App != NULL) m_App->OnInputEvent(KE);
    // m_Basics->OnInputEvent(KE);
    // m_Transform->OnInputEvent(KE);

    // Forward the event to the "custom" components.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        if (m_Components[CompNr]->OnInputEvent(KE))
            return true;

    return false;
}


bool EntityT::OnInputEvent(const CaMouseEventT& ME, float PosX, float PosY)
{
    // Derived classes that do *not* handle this event should return EntityT::OnInputEvent(ME)
    // (the base class result) rather than simply false. This gives the base class a chance to handle the event.

#if 0
    // This should only hold for (in wxWidgets-terms) "command events", not for events specific to the entity (like mouse events):

    // If the base class has no use for the event itself either, it will propagate it to the parent entities.
    // Here, in EntityT, being at the root of the inheritance hierachy, we have no use for the ME event,
    // so just propagate it up to the parent(s).
    if (Parent==NULL) return false;
    return Parent->OnInputEvent(ME, PosX, PosY);
#else
    if (m_App != NULL) m_App->OnInputEvent(ME, PosX, PosY);
    // m_Basics->OnInputEvent(ME, PosX, PosY);
    // m_Transform->OnInputEvent(ME, PosX, PosY);

    // Forward the event to the "custom" components.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        if (m_Components[CompNr]->OnInputEvent(ME, PosX, PosY))
            return true;

    return false;
#endif
}


void EntityT::OnServerFrame(float t)
{
    // Forward the event to the "fixed" components (or else they cannot interpolate).
    if (m_App != NULL) m_App->OnServerFrame(t);
    m_Basics->OnServerFrame(t);
    m_Transform->OnServerFrame(t);

    // Forward the event to the "custom" components.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        m_Components[CompNr]->OnServerFrame(t);
}


void EntityT::OnClientFrame(float t)
{
    // Forward the event to the "fixed" components (or else they cannot interpolate).
    if (m_App != NULL) m_App->OnClientFrame(t);
    m_Basics->OnClientFrame(t);
    m_Transform->OnClientFrame(t);

    // Forward the event to the "custom" components.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        m_Components[CompNr]->OnClientFrame(t);
}


bool EntityT::CallLuaMethod(const char* MethodName, int NumExtraArgs, const char* Signature, ...)
{
    va_list vl;

    va_start(vl, Signature);
    const bool Result=m_World.GetScriptState().CallMethod_Impl(IntrusivePtrT<EntityT>(this), MethodName, NumExtraArgs, Signature, vl);
    va_end(vl);

    return Result;
}


/***********************************************/
/*** Implementation of Lua binding functions ***/
/***********************************************/

static const cf::TypeSys::MethsDocT META_GetID =
{
    "GetID",
    "Returns the ID of this entity.\n"
    "The ID is unique in the world, and is used (in C++ code) to unambiguously identify\n"
    "the entity in network messages and as entity index number into `.cw` world files.\n"
    "number", "()"
};

int EntityT::GetID(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT> Ent = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);

    lua_pushinteger(LuaState, Ent->GetID());
    return 1;
}


static const cf::TypeSys::MethsDocT META_AddChild =
{
    "AddChild",
    "This method adds the given entity to the children of this entity.",
    "", "(entity child)"
};

int EntityT::AddChild(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT> Ent   = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);
    IntrusivePtrT<EntityT> Child = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(2);

    if (Child->m_Parent != NULL)    // A child entity must be a root node...
        return luaL_argerror(LuaState, 2, "child entity already has a parent, use RemoveChild() first");

    if (Child == Ent->GetRoot())    // ... but not the root of the hierarchy it is inserted into.
        return luaL_argerror(LuaState, 2, "an entity cannot be made a child of itself");

    Ent->m_Children.PushBack(Child);
    Child->m_Parent = Ent.get();

    // Make sure that the childs name is unique among its siblings.
    Child->GetBasics()->SetEntityName(Child->GetBasics()->GetEntityName());
    return 0;
}


static const cf::TypeSys::MethsDocT META_RemoveChild =
{
    "RemoveChild",
    "This method removes the given entity from the children of this entity.\n"
    "@param child   The entity that is to be removed from the children of this entity.",
    "", "(entity child)"
};

int EntityT::RemoveChild(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT> Parent=Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);
    IntrusivePtrT<EntityT> Child =Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(2);

    if (Child->m_Parent != Parent.get())
        return luaL_argerror(LuaState, 2, "entity is the child of another parent");

    const int Index=Parent->m_Children.Find(Child);

    if (Index<0)
        return luaL_argerror(LuaState, 2, "entity not found among the children of its parent");

    Parent->m_Children.RemoveAtAndKeepOrder(Index);
    Child->m_Parent=NULL;

    return 0;
}


static const cf::TypeSys::MethsDocT META_GetParent =
{
    "GetParent",
    "This method returns the parent of this entity (or `nil` if there is no parent).",
    "EntityT", "()"
};

int EntityT::GetParent(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT> Ent = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);

    if (Ent->m_Parent)
    {
        // Be careful not to push the raw Ent->m_Parent pointer here.
        Binder.Push(IntrusivePtrT<EntityT>(Ent->m_Parent));
    }
    else
    {
        lua_pushnil(LuaState);
    }

    return 1;
}


static const cf::TypeSys::MethsDocT META_GetChildren =
{
    "GetChildren",
    "This method returns an array of the children of this entity.",
    "table", "()"
};

int EntityT::GetChildren(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT> Ent = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);

    lua_newtable(LuaState);

    for (unsigned long ChildNr=0; ChildNr<Ent->m_Children.Size(); ChildNr++)
    {
        Binder.Push(Ent->m_Children[ChildNr]);
        lua_rawseti(LuaState, -2, ChildNr+1);
    }

    return 1;
}


static const cf::TypeSys::MethsDocT META_GetBasics =
{
    "GetBasics",
    "This method returns the \"Basics\" component of this entity.",
    "ComponentBasicsT", "()"
};

int EntityT::GetBasics(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT> Ent = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);

    Binder.Push(Ent->GetBasics());
    return 1;
}


static const cf::TypeSys::MethsDocT META_GetTransform =
{
    "GetTransform",
    "This method returns the \"Transform\" component of this entity.",
    "ComponentTransformT", "()"
};

int EntityT::GetTransform(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT> Ent = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);

    Binder.Push(Ent->GetTransform());
    return 1;
}


static const cf::TypeSys::MethsDocT META_AddComponent =
{
    "AddComponent",
    "This method adds a component to this entity.",
    "", "(ComponentBaseT component)"
};

int EntityT::AddComponent(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT> Ent = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);

    for (int i = 2; i <= lua_gettop(LuaState); i++)
    {
        IntrusivePtrT<ComponentBaseT> Comp = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentBaseT> >(i);

        if (Comp->GetEntity())
            return luaL_argerror(LuaState, i, "the component is already a part of an entity");

        assert(Ent->m_Components.Find(Comp) < 0);

        Ent->m_Components.PushBack(Comp);
    }

    // Now that the whole set of components has been added,
    // have the components re-resolve their dependencies among themselves.
    for (unsigned int CompNr = 0; CompNr < Ent->m_Components.Size(); CompNr++)
        Ent->m_Components[CompNr]->UpdateDependencies(Ent.get());

    return 0;
}


static const cf::TypeSys::MethsDocT META_RemoveComponent =
{
    "RemoveComponent",
    "This method removes a component from this entity.",
    "", "(ComponentBaseT component)"
};

int EntityT::RmvComponent(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT>        Ent  = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);
    IntrusivePtrT<ComponentBaseT> Comp = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentBaseT> >(2);

    const int Index = Ent->m_Components.Find(Comp);

    if (Index < 0)
        return luaL_argerror(LuaState, 2, "component not found in this entity");

    Ent->DeleteComponent(Index);
    return 0;
}


static const cf::TypeSys::MethsDocT META_GetComponents =
{
    "GetComponents",
    "This method returns an array of the components of this entity.",
    "table", "()"
};

int EntityT::GetComponents(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT> Ent = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);

    lua_newtable(LuaState);

    for (unsigned long CompNr = 0; CompNr < Ent->m_Components.Size(); CompNr++)
    {
        Binder.Push(Ent->m_Components[CompNr]);
        lua_rawseti(LuaState, -2, CompNr+1);
    }

    return 1;
}


static const cf::TypeSys::MethsDocT META_GetComponent =
{
    "GetComponent",
    "This method returns the (n-th) component of the given (type) name.\n"
    "Covers the \"custom\" components as well as the application components, \"Basics\" and \"Transform\".\n"
    "That is, `GetComponent(\"Basics\") == GetBasics()` and `GetComponent(\"Transform\") == GetTransform()`.\n"
    "@param type_name   The (type) name of the component to get, e.g. \"Image\".\n"
    "@param n           This parameter is optional, it defaults to 0 if not given.",
    "ComponentBaseT", "(string type_name, number n)"
};

int EntityT::GetComponent(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT>        Ent  = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);
    IntrusivePtrT<ComponentBaseT> Comp = Ent->GetComponent(luaL_checkstring(LuaState, 2), lua_tointeger(LuaState, 3));

    if (Comp == NULL) lua_pushnil(LuaState);
                 else Binder.Push(Comp);

    return 1;
}


static const cf::TypeSys::MethsDocT META_toString =
{
    "__toString",
    "This method returns a readable string representation of this object.",
    "string", "()"
};

int EntityT::toString(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<EntityT> Ent = Binder.GetCheckedObjectParam< IntrusivePtrT<EntityT> >(1);

    lua_pushfstring(LuaState, "A game entity with name \"%s\".", Ent->GetBasics()->GetEntityName().c_str());
    return 1;
}


/***********************************/
/*** TypeSys-related definitions ***/
/***********************************/

void* EntityT::CreateInstance(const cf::TypeSys::CreateParamsT& Params)
{
    return new EntityT(*static_cast<const cf::GameSys::EntityCreateParamsT*>(&Params));
}

const luaL_Reg EntityT::MethodsList[]=
{
    { "GetID",           EntityT::GetID },
    { "AddChild",        EntityT::AddChild },
    { "RemoveChild",     EntityT::RemoveChild },
    { "GetParent",       EntityT::GetParent },
    { "GetChildren",     EntityT::GetChildren },
    { "GetBasics",       EntityT::GetBasics },
    { "GetTransform",    EntityT::GetTransform },
    { "AddComponent",    EntityT::AddComponent },
    { "RemoveComponent", EntityT::RmvComponent },
    { "GetComponents",   EntityT::GetComponents },
    { "GetComponent",    EntityT::GetComponent },
    { "__tostring",      EntityT::toString },
    { NULL, NULL }
};

const cf::TypeSys::MethsDocT EntityT::DocMethods[] =
{
    META_GetID,
    META_AddChild,
    META_RemoveChild,
    META_GetParent,
    META_GetChildren,
    META_GetBasics,
    META_GetTransform,
    META_AddComponent,
    META_RemoveComponent,
    META_GetComponents,
    META_GetComponent,
    META_toString,
    { NULL, NULL, NULL, NULL }
};

const cf::TypeSys::TypeInfoT EntityT::TypeInfo(GetGameSysEntityTIM(), "EntityT", NULL /*No base class.*/, EntityT::CreateInstance, MethodsList, DocClass, DocMethods);