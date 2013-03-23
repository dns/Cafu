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

#include "Window.hpp"
#include "WindowCreateParams.hpp"
#include "CompBase.hpp"
#include "GuiImpl.hpp"
#include "GuiResources.hpp"
#include "ConsoleCommands/Console.hpp"
#include "MaterialSystem/Renderer.hpp"
#include "TypeSys.hpp"

extern "C"
{
    #include <lua.h>
    #include <lualib.h>
    #include <lauxlib.h>
}

#include <cassert>


using namespace cf::GuiSys;


// Note that we cannot simply replace this method with a global TypeInfoManT instance,
// because it is called during global static initialization time. The TIM instance being
// embedded in the function guarantees that it is properly initialized before first use.
cf::TypeSys::TypeInfoManT& cf::GuiSys::GetWindowTIM()
{
    static cf::TypeSys::TypeInfoManT TIM;

    return TIM;
}


/*** Begin of TypeSys related definitions for this class. ***/

void* WindowT::CreateInstance(const cf::TypeSys::CreateParamsT& Params)
{
    return new WindowT(*static_cast<const cf::GuiSys::WindowCreateParamsT*>(&Params));
}

const luaL_reg WindowT::MethodsList[]=
{
    { "AddChild",        WindowT::AddChild },
    { "RemoveChild",     WindowT::RemoveChild },
    { "GetParent",       WindowT::GetParent },
    { "GetChildren",     WindowT::GetChildren },
    { "GetTime",         WindowT::GetTime },
    { "GetBasics",       WindowT::GetBasics },
    { "GetTransform",    WindowT::GetTransform },
    { "AddComponent",    WindowT::AddComponent },
    { "RemoveComponent", WindowT::RmvComponent },
    { "GetComponents",   WindowT::GetComponents },
    { "GetComponent",    WindowT::GetComponent },
    { "__tostring",      WindowT::toString },
    { NULL, NULL }
};

const cf::TypeSys::TypeInfoT WindowT::TypeInfo(GetWindowTIM(), "WindowT", NULL /*No base class.*/, WindowT::CreateInstance, MethodsList);

/*** End of TypeSys related definitions for this class. ***/


WindowT::WindowT(const WindowCreateParamsT& Params)
    : m_Gui(Params.Gui),
      m_Parent(NULL),
      m_Children(),
      m_Time(0.0f),
      m_App(NULL),
      m_Basics(new ComponentBasicsT()),
      m_Transform(new ComponentTransformT()),
      m_Components()
{
    m_Basics->UpdateDependencies(this);
    m_Transform->UpdateDependencies(this);

    m_Transform->SetSize(Vector2fT(80, 60));
}


WindowT::WindowT(const WindowT& Window, bool Recursive)
    : m_Gui(Window.m_Gui),
      m_Parent(NULL),
      m_Children(),
      m_Time(Window.m_Time),
      m_App(NULL),
      m_Basics(Window.GetBasics()->Clone()),
      m_Transform(Window.GetTransform()->Clone()),
      m_Components()
{
    if (Window.GetApp() != NULL)
    {
        m_App = Window.GetApp()->Clone();
        m_App->UpdateDependencies(this);
    }

    m_Basics->UpdateDependencies(this);
    m_Transform->UpdateDependencies(this);

    // Copy-create all components first.
    m_Components.PushBackEmptyExact(Window.GetComponents().Size());

    for (unsigned int CompNr = 0; CompNr < Window.GetComponents().Size(); CompNr++)
        m_Components[CompNr] = Window.GetComponents()[CompNr]->Clone();

    // Now that all components have been copied, have them resolve their dependencies among themselves.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        m_Components[CompNr]->UpdateDependencies(this);

    // Recursively copy the children.
    if (Recursive)
    {
        for (unsigned long ChildNr=0; ChildNr<Window.m_Children.Size(); ChildNr++)
        {
            m_Children.PushBack(Window.m_Children[ChildNr]->Clone(Recursive));
            m_Children[ChildNr]->m_Parent=this;
        }
    }
}


WindowT* WindowT::Clone(bool Recursive) const
{
    return new WindowT(*this, Recursive);
}


WindowT::~WindowT()
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
        // To be safe, make sure that the component no longer refers back to this window.
        m_Components[CompNr]->UpdateDependencies(NULL);
    }

    m_Components.Clear();

    if (!m_App.IsNull()) m_App->UpdateDependencies(NULL);
    m_Basics->UpdateDependencies(NULL);
    m_Transform->UpdateDependencies(NULL);
}


bool WindowT::AddChild(IntrusivePtrT<WindowT> Child, unsigned long Pos)
{
    assert(Child->m_Parent == NULL);
    if (Child->m_Parent != NULL)    // A child window must be a root node...
        return false;   // luaL_argerror(LuaState, 2, "child window already has a parent, use RemoveChild() first");

    assert(Child != GetRoot());
    if (Child == GetRoot())         // ... but not the root of the hierarchy it is inserted into.
        return false;   // luaL_argerror(LuaState, 2, "a window cannot be made a child of itself");

    m_Children.InsertAt(std::min(m_Children.Size(), Pos), Child);
    Child->m_Parent = this;

    // Make sure that the childs name is unique among its siblings.
    Child->GetBasics()->SetWindowName(Child->GetBasics()->GetWindowName());
    return true;
}


bool WindowT::RemoveChild(IntrusivePtrT<WindowT> Child)
{
    assert(Child->m_Parent == this);
    if (Child->m_Parent != this)
        return false;   // luaL_argerror(LuaState, 2, "window is the child of another parent");

    const int Index = m_Children.Find(Child);

    assert(Index >= 0);
    if (Index < 0)
        return false;   // return luaL_argerror(LuaState, 2, "window not found among the children of its parent");

    m_Children.RemoveAtAndKeepOrder(Index);
    Child->m_Parent = NULL;
    return true;
}


void WindowT::GetChildren(ArrayT< IntrusivePtrT<WindowT> >& Chld, bool Recurse) const
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


IntrusivePtrT<WindowT> WindowT::GetRoot()
{
    WindowT* Root = this;

    while (Root->m_Parent)
        Root = Root->m_Parent;

    return Root;
}


void WindowT::SetApp(IntrusivePtrT<ComponentBaseT> App)
{
    if (m_App == App) return;

    if (!m_App.IsNull()) m_App->UpdateDependencies(NULL);
    m_App = App;
    if (!m_App.IsNull()) m_App->UpdateDependencies(this);
}


IntrusivePtrT<ComponentBaseT> WindowT::GetComponent(const std::string& TypeName, unsigned int n) const
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


bool WindowT::AddComponent(IntrusivePtrT<ComponentBaseT> Comp, unsigned long Index)
{
    if (Comp->GetWindow()) return false;
    assert(m_Components.Find(Comp) < 0);

    m_Components.InsertAt(std::min(Index, m_Components.Size()), Comp);

    // Have the components re-resolve their dependencies among themselves.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        m_Components[CompNr]->UpdateDependencies(this);

    return true;
}


void WindowT::DeleteComponent(unsigned long CompNr)
{
    // Let the component know that it is no longer a part of this window.
    m_Components[CompNr]->UpdateDependencies(NULL);

    m_Components.RemoveAtAndKeepOrder(CompNr);

    // Have the remaining components re-resolve their dependencies among themselves.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        m_Components[CompNr]->UpdateDependencies(this);
}


Vector2fT WindowT::GetAbsolutePos() const
{
#if 1
    Vector2fT Pos = m_Transform->GetPos();

    for (const WindowT* P = m_Parent; P; P = P->m_Parent)
        Pos += P->m_Transform->GetPos();

    return Pos;
#else
    // Recursive implementation:
    if (m_Parent==NULL)
        return m_Transform->GetPos();

    // We have a parent, so get it's absolute position first, then add our relative position.
    return m_Parent->GetAbsolutePos() + m_Transform->GetPos();
#endif
}


IntrusivePtrT<WindowT> WindowT::Find(const std::string& WantedName)
{
    if (WantedName == m_Basics->GetWindowName()) return this;

    // Recursively see if any of the children has the desired name.
    for (unsigned long ChildNr=0; ChildNr<m_Children.Size(); ChildNr++)
    {
        IntrusivePtrT<WindowT> Win=m_Children[ChildNr]->Find(WantedName);

        if (Win!=NULL) return Win;
    }

    return NULL;
}


IntrusivePtrT<WindowT> WindowT::Find(const Vector2fT& Pos, bool OnlyVisible)
{
    if (OnlyVisible && !m_Basics->IsShown()) return NULL;

    // Children are on top of their parents and (currently) not clipped to the parent rectangle, so we should search them first.
    for (unsigned long ChildNr=0; ChildNr<m_Children.Size(); ChildNr++)
    {
        // Search the children in reverse order, because if they overlap,
        // those that are drawn last appear topmost, and so we want to find them first.
        IntrusivePtrT<WindowT> Found = m_Children[m_Children.Size()-1-ChildNr]->Find(Pos, OnlyVisible);

        if (Found!=NULL) return Found;
    }

    // Okay, Pos is not inside any of the children, now check this window.
    const Vector2fT Abs1 = GetAbsolutePos();
    const Vector2fT Abs2 = Abs1 + m_Transform->GetSize();

    return (Pos.x < Abs1.x || Pos.y < Abs1.y || Pos.x > Abs2.x || Pos.y > Abs2.y) ? NULL : this;
}


void WindowT::Render() const
{
    if (!m_Basics->IsShown()) return;

    MatSys::Renderer->PushMatrix(MatSys::RendererI::MODEL_TO_WORLD);
    {
        const Vector2fT Pos  = m_Transform->GetPos();
        const Vector2fT Size = m_Transform->GetSize();

        // Set the coordinate origin to the top-left corner of our window.
        if (m_Transform->GetRotAngle() == 0)
        {
            MatSys::Renderer->Translate(MatSys::RendererI::MODEL_TO_WORLD, Pos.x, Pos.y, 0.0f);
        }
        else
        {
            MatSys::Renderer->Translate(MatSys::RendererI::MODEL_TO_WORLD, Pos.x + Size.x/2.0f, Pos.y + Size.y/2.0f, 0.0f);
            MatSys::Renderer->RotateZ  (MatSys::RendererI::MODEL_TO_WORLD, m_Transform->GetRotAngle());
            MatSys::Renderer->Translate(MatSys::RendererI::MODEL_TO_WORLD,        -Size.x/2.0f,        -Size.y/2.0f, 0.0f);
        }

        // Render the "custom" components in the proper order -- bottom-up.
        for (unsigned long CompNr = m_Components.Size(); CompNr > 0; CompNr--)
            m_Components[CompNr-1]->Render();

        // Render the "fixed" components.
        // m_Transform->Render();
        // m_Basics->Render();
        if (m_App != NULL) m_App->Render();

        // Render the children.
        for (unsigned long ChildNr = 0; ChildNr < m_Children.Size(); ChildNr++)
            m_Children[ChildNr]->Render();
    }
    MatSys::Renderer->PopMatrix(MatSys::RendererI::MODEL_TO_WORLD);
}


bool WindowT::OnInputEvent(const CaKeyboardEventT& KE)
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


bool WindowT::OnInputEvent(const CaMouseEventT& ME, float PosX, float PosY)
{
    // Derived classes that do *not* handle this event should return WindowT::OnInputEvent(ME)
    // (the base class result) rather than simply false. This gives the base class a chance to handle the event.

#if 0
    // This should only hold for (in wxWidgets-terms) "command events", not for events specific to the window (like mouse events):

    // If the base class has no use for the event itself either, it will propagate it to the parent windows.
    // Here, in WindowT, being at the root of the inheritance hierachy, we have no use for the ME event,
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


bool WindowT::OnClockTickEvent(float t)
{
    m_Time += t;

    // Forward the event to the "fixed" components (or else they cannot interpolate).
    if (m_App != NULL) m_App->OnClockTickEvent(t);
    m_Basics->OnClockTickEvent(t);
    m_Transform->OnClockTickEvent(t);

    // Forward the event to the "custom" components.
    for (unsigned int CompNr = 0; CompNr < m_Components.Size(); CompNr++)
        m_Components[CompNr]->OnClockTickEvent(t);

    return true;
}


bool WindowT::CallLuaMethod(const char* MethodName, const char* Signature, ...)
{
    va_list vl;

    va_start(vl, Signature);
    const bool Result=m_Gui.m_ScriptState.CallMethod(IntrusivePtrT<WindowT>(this), MethodName, Signature, vl);
    va_end(vl);

    return Result;
}


/**********************************************/
/*** Impementation of Lua binding functions ***/
/**********************************************/

int WindowT::AddChild(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT> Win  =Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);
    IntrusivePtrT<WindowT> Child=Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(2);

    if (Child->m_Parent!=NULL)      // A child window must be a root node...
        return luaL_argerror(LuaState, 2, "child window already has a parent, use RemoveChild() first");

    if (Child==Win->GetRoot())      // ... but not the root of the hierarchy it is inserted into.
        return luaL_argerror(LuaState, 2, "a window cannot be made a child of itself");

    Win->m_Children.PushBack(Child);
    Child->m_Parent = Win.get();

    // Make sure that the childs name is unique among its siblings.
    Child->GetBasics()->SetWindowName(Child->GetBasics()->GetWindowName());
    return 0;
}


int WindowT::RemoveChild(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT> Parent=Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);
    IntrusivePtrT<WindowT> Child =Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(2);

    if (Child->m_Parent != Parent.get())
        return luaL_argerror(LuaState, 2, "window is the child of another parent");

    const int Index=Parent->m_Children.Find(Child);

    if (Index<0)
        return luaL_argerror(LuaState, 2, "window not found among the children of its parent");

    Parent->m_Children.RemoveAtAndKeepOrder(Index);
    Child->m_Parent=NULL;

    return 0;
}


int WindowT::GetParent(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT> Win=Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);

    if (Win->m_Parent)
    {
        // Be careful not to push the raw Win->m_Parent pointer here.
        Binder.Push(IntrusivePtrT<WindowT>(Win->m_Parent));
    }
    else
    {
        lua_pushnil(LuaState);
    }

    return 1;
}


int WindowT::GetChildren(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT> Win=Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);

    lua_newtable(LuaState);

    for (unsigned long ChildNr=0; ChildNr<Win->m_Children.Size(); ChildNr++)
    {
        Binder.Push(Win->m_Children[ChildNr]);
        lua_rawseti(LuaState, -2, ChildNr+1);
    }

    return 1;
}


int WindowT::GetTime(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT> Win = Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);

    lua_pushnumber(LuaState, Win->m_Time);
    return 1;
}


int WindowT::GetBasics(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT> Win = Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);

    Binder.Push(Win->GetBasics());
    return 1;
}


int WindowT::GetTransform(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT> Win = Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);

    Binder.Push(Win->GetTransform());
    return 1;
}


int WindowT::AddComponent(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT> Win = Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);

    for (int i = 2; i <= lua_gettop(LuaState); i++)
    {
        IntrusivePtrT<ComponentBaseT> Comp = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentBaseT> >(i);

        if (Comp->GetWindow())
            return luaL_argerror(LuaState, i, "the component is already a part of a window");

        assert(Win->m_Components.Find(Comp) < 0);

        Win->m_Components.PushBack(Comp);
    }

    // Now that the whole set of components has been added,
    // have the components re-resolve their dependencies among themselves.
    for (unsigned int CompNr = 0; CompNr < Win->m_Components.Size(); CompNr++)
        Win->m_Components[CompNr]->UpdateDependencies(Win.get());

    return 0;
}


int WindowT::RmvComponent(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT>        Win  = Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);
    IntrusivePtrT<ComponentBaseT> Comp = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentBaseT> >(2);

    const int Index = Win->m_Components.Find(Comp);

    if (Index < 0)
        return luaL_argerror(LuaState, 2, "component not found in this window");

    Win->DeleteComponent(Index);
    return 0;
}


int WindowT::GetComponents(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT> Win = Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);

    lua_newtable(LuaState);

    for (unsigned long CompNr = 0; CompNr < Win->m_Components.Size(); CompNr++)
    {
        Binder.Push(Win->m_Components[CompNr]);
        lua_rawseti(LuaState, -2, CompNr+1);
    }

    return 1;
}


int WindowT::GetComponent(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT>        Win  = Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);
    IntrusivePtrT<ComponentBaseT> Comp = Win->GetComponent(luaL_checkstring(LuaState, 2), lua_tointeger(LuaState, 3));

    if (Comp == NULL) lua_pushnil(LuaState);
                 else Binder.Push(Comp);

    return 1;
}


int WindowT::toString(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<WindowT> Win=Binder.GetCheckedObjectParam< IntrusivePtrT<WindowT> >(1);

    lua_pushfstring(LuaState, "A gui window with name \"%s\".", Win->GetBasics()->GetWindowName().c_str());
    return 1;
}
