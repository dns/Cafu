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

#include "Window.hpp"
#include "EditorData.hpp" // Needed because of deletion of undefined type warning.
#include "Coroutines.hpp"
#include "WindowCreateParams.hpp"
#include "GuiMan.hpp"
#include "GuiImpl.hpp"      // For GuiImplT::GetCheckedObjectParam().
#include "ConsoleCommands/Console.hpp"
#include "MaterialSystem/MaterialManager.hpp"
#include "MaterialSystem/Renderer.hpp"
#include "MaterialSystem/Mesh.hpp"
#include "Fonts/FontTT.hpp"
#include "TypeSys.hpp"

extern "C"
{
    #include <lua.h>
    #include <lualib.h>
    #include <lauxlib.h>
}

#include <assert.h>

#if defined(_WIN32) && defined (_MSC_VER)
    #if (_MSC_VER<1300)
        #define for if (false) ; else for

        // Turn off warning 4786: "Bezeichner wurde auf '255' Zeichen in den Debug-Informationen reduziert."
        #pragma warning(disable:4786)
    #endif
#endif


using namespace cf::GuiSys;


static const std::string DEFAULT_FONT_NAME="Fonts/Arial";


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
    { "set",         WindowT::Set },
    { "get",         WindowT::Get },
    { "interpolate", WindowT::Interpolate },
    { "SetName",     WindowT::SetName },
    { "GetName",     WindowT::GetName },
    { "AddChild",    WindowT::AddChild },
    { "RemoveChild", WindowT::RemoveChild },
    { "__gc",        WindowT::Destruct },
    { "__tostring",  WindowT::toString },
    { NULL, NULL }
};

const cf::TypeSys::TypeInfoT WindowT::TypeInfo(GetWindowTIM(), "WindowT", NULL /*No base class.*/, WindowT::CreateInstance, MethodsList);

/*** End of TypeSys related definitions for this class. ***/


WindowT::WindowT(const WindowCreateParamsT& Params)
    : Gui(Params.Gui),
      EditorData(NULL),
   // Children(),
      Parent(NULL),
      Name(""),
      Time(0.0f),
      ShowWindow(true),
   // Rect(),
      RotAngle(0.0f),
      BackRenderMat(NULL),
      BackRenderMatName(""),
   // BackColor(),
      BorderWidth(0.0f),
   // BorderColor(),
      Font(GuiMan->GetFont(DEFAULT_FONT_NAME)),
      Text(""),
      TextScale(1.0f),
   // TextColor(),
      TextAlignHor(left),
      TextAlignVer(top),
      CppReferencesCount(0)
{
    for (unsigned long c=0; c<4; c++)
    {
        Rect       [c]=0.0f;
        BackColor  [c]=0.5f;
        BorderColor[c]=1.0f;
        TextColor  [c]=(c<2) ? 0.0f : 1.0f;
    }


    // This is currently required, because this ctor is also used now for windows created
    // by Lua script (not only for windows created in C++, using "new WindowT()")...
    FillMemberVars();
}


static cf::GuiSys::GuiImplT& NullGui=*((cf::GuiSys::GuiImplT*)NULL);


WindowT::WindowT(const WindowT& Window, bool Recursive)
    : Gui(NullGui), // Intentionally set NULL Gui since copying of windows is only used in the editor where the Gui is not used.
      EditorData(NULL), // We leave the creation of editor data to the caller. Otherwise we would need to implemented cloning of editor data.
      Parent(NULL),
      Name(Window.Name),
      Time(Window.Time),
      ShowWindow(Window.ShowWindow),
      RotAngle(Window.RotAngle),
      BackRenderMat(Window.BackRenderMatName.empty() ? NULL : MatSys::Renderer->RegisterMaterial(MaterialManager->GetMaterial(Window.BackRenderMatName))),
      BackRenderMatName(Window.BackRenderMatName),
      BorderWidth(Window.BorderWidth),
      Font(GuiMan->GetFont(Window.Font->GetName())),
      Text(Window.Text),
      TextScale(Window.TextScale),
      TextAlignHor(Window.TextAlignHor),
      TextAlignVer(Window.TextAlignVer),
      CppReferencesCount(0)
{
    for (unsigned long i=0; i<4; i++)
    {
        Rect       [i]=Window.Rect[i];
        BackColor  [i]=Window.BackColor[i];
        BorderColor[i]=Window.BorderColor[i];
        TextColor  [i]=Window.TextColor[i];
    }

    FillMemberVars();

    if (Recursive)
    {
        for (unsigned long ChildNr=0; ChildNr<Window.Children.Size(); ChildNr++)
        {
            Children.PushBack(Window.Children[ChildNr]->Clone(Recursive));
            Children[ChildNr]->Parent=this;
        }
    }
}


WindowT* WindowT::Clone(bool Recursive) const
{
    return new WindowT(*this, Recursive);
}


WindowT::~WindowT()
{
    // Note that WindowT instances are only ever deleted by Lua, which is responsible for their lifetime.
    // Thus, we only get here by the call to "delete Win;" in WindowT::Destruct().
    // See the general documentation about the WindowT class and the comments in WindowT::Destruct() for additional details!

    // Now, note that manipulating the LuaState here is OK, even if we got here as the result of a call to lua_close(),
    // because in both cases (normal garbage collection cycle and call to lua_close()), the __gc handlers are called
    // first, before the LuaState is changed.
    // In this regard, also see my posting "Condition of the lua_State in __gc handler after call to lua_close()?"
    // to the Lua mailling-list on 2008-04-09, and the answer by Roberto:
    // http://thread.gmane.org/gmane.comp.lang.lua.general/47004
    // (When I wrote this posting, I still thought some manual un-anchoring would be necessary here. However, it isn't,
    //  as explained in the next paragraph.)

    // Note that there is no need to do something here with our Parent and Children members, such as telling our parent
    // that we're not among its children anymore and telling our children that we're not available as parent anymore.
    // The same is true for the synchron __parent_cf and __children_cf fields in our alter ego.
    // This is because when Lua removes us even though we're still connected (that is, we have a parent and/or children),
    // the entiry hierarchy is removed (or otherwise Lua wouldn't remove us).


    // If the reference count is not 0 at this point, this means that one or more WindowPtrTs to this window still exists.
    // When the next of them is destructed, it will try to decrease the value of this CppReferencesCount instance,
    // which however is in already deleted memory then.
    assert(CppReferencesCount==0);

    delete EditorData;

    assert(BackRenderMat!=GuiMan->GetDefaultRM());
    MatSys::Renderer->FreeMaterial(BackRenderMat);
}


const std::string& WindowT::GetName() const
{
    return Name;
}


EditorDataT* WindowT::GetEditorData()
{
    return EditorData;
}


void WindowT::GetChildren(ArrayT<WindowT*>& Chld, bool Recurse)
{
    Chld.PushBack(Children);

    if (!Recurse) return;

    for (unsigned long ChildNr=0; ChildNr<Children.Size(); ChildNr++)
        Children[ChildNr]->GetChildren(Chld, Recurse);
}


WindowT* WindowT::GetRoot()
{
    WindowT* Root=this;

    while (Root->Parent!=NULL)
        Root=Root->Parent;

    return Root;
}


void WindowT::GetAbsolutePos(float& x, float& y) const
{
#if 1
    x=Rect[0];
    y=Rect[1];

    for (const WindowT* P=Parent; P!=NULL; P=P->Parent)
    {
        x+=P->Rect[0];
        y+=P->Rect[1];
    }
#else
    // Recursive implementation:
    if (Parent==NULL)
    {
        x=Rect[0];
        y=Rect[1];
        return;
    }

    float px;
    float py;

    // We have a parent, so get it's absolute position first, then add our relative position.
    Parent->GetAbsolutePos(px, py);

    x=px+Rect[0];
    y=py+Rect[1];
#endif
}


WindowT* WindowT::Find(const std::string& WantedName)
{
    if (WantedName==Name) return this;

    // Recursively see if any of the children has the desired name.
    for (unsigned long ChildNr=0; ChildNr<Children.Size(); ChildNr++)
    {
        WindowT* Win=Children[ChildNr]->Find(WantedName);

        if (Win!=NULL) return Win;
    }

    return NULL;
}


WindowT* WindowT::Find(float x, float y, bool OnlyVisible)
{
    if (OnlyVisible && !ShowWindow) return NULL;

    // Children are on top of their parents and (currently) not clipped to the parent rectangle, so we should search them first.
    for (unsigned long ChildNr=0; ChildNr<Children.Size(); ChildNr++)
    {
        // Search the children in reverse order, because if they overlap,
        // those that are drawn last appear topmost, and so we want to find them first.
        WindowT* Found=Children[Children.Size()-1-ChildNr]->Find(x, y, OnlyVisible);

        if (Found!=NULL) return Found;
    }

    // Okay, the point (x, y) is not inside any of the children, now check this window.
    float AbsX1, AbsY1;

    GetAbsolutePos(AbsX1, AbsY1);

    const float SizeX=Rect[2];
    const float SizeY=Rect[3];

    return (x<AbsX1 || y<AbsY1 || x>AbsX1+SizeX || y>AbsY1+SizeY) ? NULL : this;
}


void WindowT::Render() const
{
    if (!ShowWindow) return;

    float x1;
    float y1;

    GetAbsolutePos(x1, y1);


    // Save the current matrices.
    if (RotAngle!=0)
    {
        MatSys::Renderer->PushMatrix(MatSys::RendererI::MODEL_TO_WORLD);

        MatSys::Renderer->Translate(MatSys::RendererI::MODEL_TO_WORLD, x1+Rect[2]/2.0f, y1+Rect[3]/2.0f, 0.0f);
        MatSys::Renderer->RotateZ  (MatSys::RendererI::MODEL_TO_WORLD, RotAngle);
        MatSys::Renderer->Translate(MatSys::RendererI::MODEL_TO_WORLD, -(x1+Rect[2]/2.0f), -(y1+Rect[3]/2.0f), 0.0f);
    }


    // TODO !!!!!!!!!!!
    // All meshes should be setup ONCE in the constructor!!
    // (Should they? Even if we plan to add scripting? Yes, I think they should, Render() is called far more often than anything else.)

    // Render the background.
 // MatSys::Renderer->SetCurrentAmbientLightColor(BackColor);
    MatSys::Renderer->SetCurrentMaterial(BackRenderMat!=NULL ? BackRenderMat : GuiMan->GetDefaultRM());

    static MatSys::MeshT BackMesh(MatSys::MeshT::Quads);
    BackMesh.Vertices.Overwrite();
    BackMesh.Vertices.PushBackEmpty(4);     // Just a single quad for the background rectangle.

    for (unsigned long VertexNr=0; VertexNr<BackMesh.Vertices.Size(); VertexNr++)
    {
        for (unsigned long i=0; i<4; i++)
            BackMesh.Vertices[VertexNr].Color[i]=BackColor[i];
    }

    const float x2=x1+Rect[2];
    const float y2=y1+Rect[3];

    const float b=BorderWidth;

    BackMesh.Vertices[0].SetOrigin(x1+b, y1+b); BackMesh.Vertices[0].SetTextureCoord(0.0f, 0.0f);
    BackMesh.Vertices[1].SetOrigin(x2-b, y1+b); BackMesh.Vertices[1].SetTextureCoord(1.0f, 0.0f);
    BackMesh.Vertices[2].SetOrigin(x2-b, y2-b); BackMesh.Vertices[2].SetTextureCoord(1.0f, 1.0f);
    BackMesh.Vertices[3].SetOrigin(x1+b, y2-b); BackMesh.Vertices[3].SetTextureCoord(0.0f, 1.0f);

    MatSys::Renderer->RenderMesh(BackMesh);


    // Render the border.
    if (b>0.0f)
    {
     // MatSys::Renderer->SetCurrentAmbientLightColor(BorderColor);
        MatSys::Renderer->SetCurrentMaterial(GuiMan->GetDefaultRM() /*BorderMaterial*/);

        static MatSys::MeshT BorderMesh(MatSys::MeshT::Quads);
        BorderMesh.Vertices.Overwrite();
        BorderMesh.Vertices.PushBackEmpty(4*4);     // One rectangle for each side of the background.

        for (unsigned long VertexNr=0; VertexNr<BorderMesh.Vertices.Size(); VertexNr++)
        {
            for (unsigned long i=0; i<4; i++)
                BorderMesh.Vertices[VertexNr].Color[i]=BorderColor[i];
        }

        // Left border rectangle.
        BorderMesh.Vertices[ 0].SetOrigin(x1,   y1); BorderMesh.Vertices[ 0].SetTextureCoord(0.0f, 0.0f);
        BorderMesh.Vertices[ 1].SetOrigin(x1+b, y1); BorderMesh.Vertices[ 1].SetTextureCoord(1.0f, 0.0f);
        BorderMesh.Vertices[ 2].SetOrigin(x1+b, y2); BorderMesh.Vertices[ 2].SetTextureCoord(1.0f, 1.0f);
        BorderMesh.Vertices[ 3].SetOrigin(x1,   y2); BorderMesh.Vertices[ 3].SetTextureCoord(0.0f, 1.0f);

        // Top border rectangle.
        BorderMesh.Vertices[ 4].SetOrigin(x1+b, y1  ); BorderMesh.Vertices[ 4].SetTextureCoord(0.0f, 0.0f);
        BorderMesh.Vertices[ 5].SetOrigin(x2-b, y1  ); BorderMesh.Vertices[ 5].SetTextureCoord(1.0f, 0.0f);
        BorderMesh.Vertices[ 6].SetOrigin(x2-b, y1+b); BorderMesh.Vertices[ 6].SetTextureCoord(1.0f, 1.0f);
        BorderMesh.Vertices[ 7].SetOrigin(x1+b, y1+b); BorderMesh.Vertices[ 7].SetTextureCoord(0.0f, 1.0f);

        // Right border rectangle.
        BorderMesh.Vertices[ 8].SetOrigin(x2-b, y1); BorderMesh.Vertices[ 8].SetTextureCoord(0.0f, 0.0f);
        BorderMesh.Vertices[ 9].SetOrigin(x2,   y1); BorderMesh.Vertices[ 9].SetTextureCoord(1.0f, 0.0f);
        BorderMesh.Vertices[10].SetOrigin(x2,   y2); BorderMesh.Vertices[10].SetTextureCoord(1.0f, 1.0f);
        BorderMesh.Vertices[11].SetOrigin(x2-b, y2); BorderMesh.Vertices[11].SetTextureCoord(0.0f, 1.0f);

        // Bottom border rectangle.
        BorderMesh.Vertices[12].SetOrigin(x1+b, y2-b); BorderMesh.Vertices[12].SetTextureCoord(0.0f, 0.0f);
        BorderMesh.Vertices[13].SetOrigin(x2-b, y2-b); BorderMesh.Vertices[13].SetTextureCoord(1.0f, 0.0f);
        BorderMesh.Vertices[14].SetOrigin(x2-b, y2  ); BorderMesh.Vertices[14].SetTextureCoord(1.0f, 1.0f);
        BorderMesh.Vertices[15].SetOrigin(x1+b, y2  ); BorderMesh.Vertices[15].SetTextureCoord(0.0f, 1.0f);

        MatSys::Renderer->RenderMesh(BorderMesh);
    }


    // Render the text (the foreground).
    if (Font!=NULL)
    {
        int LineCount=1;
        const size_t TextLength=Text.length();

        for (size_t i=0; i+1<TextLength; i++)
            if (Text[i]=='\n')
                LineCount++;

        unsigned long r_=(unsigned long)(TextColor[0]*255.0f);
        unsigned long g_=(unsigned long)(TextColor[1]*255.0f);
        unsigned long b_=(unsigned long)(TextColor[2]*255.0f);
        unsigned long a_=(unsigned long)(TextColor[3]*255.0f);

        const float MaxTop     =Font->GetAscender(TextScale);
        const float LineSpacing=Font->GetLineSpacing(TextScale);
        float       LineOffsetY=0.0;

        size_t LineStart=0;

        while (true)
        {
            const size_t LineEnd=Text.find('\n', LineStart);
            std::string  Line   =(LineEnd==std::string::npos) ? Text.substr(LineStart) : Text.substr(LineStart, LineEnd-LineStart);

            float AlignX=0.0f;
            float AlignY=0.0f;

            switch (TextAlignHor)
            {
                case left:  AlignX=b; break;
                case right: AlignX=x2-x1-Font->GetWidth(Line, TextScale)-b; break;
                default:    AlignX=(x2-x1-Font->GetWidth(Line, TextScale))/2.0f; break;
            }

            switch (TextAlignVer)
            {
                case top:    AlignY=b+MaxTop; break;   // Without the +MaxTop, the text baseline ("___") is at the top border of the window.
                case bottom: AlignY=y2-y1-b-(LineCount-1)*LineSpacing; break;
                default:     AlignY=(y2-y1-LineCount*LineSpacing)/2.0f+MaxTop; break;
            }

            Font->Print(x1+AlignX, y1+AlignY+LineOffsetY, TextScale, (a_ << 24) | (r_ << 16) | (g_ << 8) | (b_ << 0), "%s", Line.c_str());

            if (LineEnd==std::string::npos) break;
            LineStart=LineEnd+1;
            LineOffsetY+=LineSpacing;
        }
    }


    // Render the children.
    for (unsigned long ChildNr=0; ChildNr<Children.Size(); ChildNr++)
    {
        Children[ChildNr]->Render();
    }

    // Render the editor part of this window.
    EditorRender();

    if (RotAngle!=0)
    {
        // Restore the previously active matrices.
        MatSys::Renderer->PopMatrix(MatSys::RendererI::MODEL_TO_WORLD);
    }
}


bool WindowT::OnInputEvent(const CaKeyboardEventT& KE)
{
    return false;
}


bool WindowT::OnInputEvent(const CaMouseEventT& ME, float /*PosX*/, float /*PosY*/)
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
    return false;
#endif
}


bool WindowT::OnClockTickEvent(float t)
{
    // float OldTime=Time;

    Time+=t;

    // See if we have to run any OnTime scripts.
    // ;

    // Run the pending value interpolations.
    for (unsigned long INr=0; INr<PendingInterpolations.Size(); INr++)
    {
        InterpolationT* I=PendingInterpolations[INr];

        // Run this interpolation only if there is no other interpolation that addresses the same target value.
        unsigned long OldINr;

        for (OldINr=0; OldINr<INr; OldINr++)
            if (PendingInterpolations[OldINr]->Value == I->Value)
                break;

        if (OldINr<INr) continue;


        // Actually run the interpolation I.
        I->CurrentTime+=t;

        if (I->CurrentTime >= I->TotalTime)
        {
            // This interpolation reached its end value, so drop it from the pending queue.
            I->Value[0]=I->EndValue;

            delete I;
            PendingInterpolations.RemoveAtAndKeepOrder(INr);
            INr--;
        }
        else
        {
            I->UpdateValue();
        }
    }

    return true;
}


bool WindowT::CallLuaMethod(const char* MethodName, const char* Signature, ...)
{
    va_list vl;

    va_start(vl, Signature);
    const bool Result=CallLuaMethod(MethodName, Signature, vl);
    va_end(vl);

    return Result;
}


bool WindowT::CallLuaMethod(const char* MethodName, const char* Signature, va_list vl)
{
    lua_State* LuaState=Gui.LuaState;

    // Note that when re-entrancy occurs, we do usually NOT have an empty stack here!
    // That is, when we first call a Lua function the stack is empty, but when the called Lua function
    // in turn calls back into our C++ code (e.g. a console function), and the C++ code in turn gets here,
    // we have a case of re-entrancy and the stack is not empty!
    // That is, the assert() statement in the next line does not generally hold.
    // assert(lua_gettop(LuaState)==0);

    // Push our alter ego onto the stack.
    if (!PushAlterEgo()) return false;

    // Push the desired method (from the alter ego) onto the stack.
    lua_pushstring(LuaState, MethodName);
    lua_rawget(LuaState, -2);

    if (!lua_isfunction(LuaState, -1))
    {
        // If we get here, this usually means that the value at -1 is just nil, i.e. the
        // function that we would like to call was just not defined in the Lua script.
        lua_pop(LuaState, 2);   // Pop whatever is not a function, and the alter ego.
        return false;
    }

    // Swap the alter ego and the function, because the alter ego is not needed any more but for the
    // first argument to the function (the "self" or "this" value for the object-oriented method call).
    lua_insert(LuaState, -2);   // Inserting the function at index -2 shifts the alter ego to index -1.

    // The stack is now prepared according to the requirements/specs of the StartNewCoroutine() method.
    return Gui.StartNewCoroutine(1, Signature, vl, std::string("method ")+MethodName+"() of window with name \""+Name+"\"");
}


bool WindowT::PushAlterEgo()
{
    lua_getfield(Gui.LuaState, LUA_REGISTRYINDEX, "__windows_list_cf");

#ifdef DEBUG
    if (!lua_istable(Gui.LuaState, -1))
    {
        // This should never happen, the __windows_list_cf table was created when the first WindowT was created!
        assert(false);
        Console->Warning("Registry table \"__windows_list_cf\" does not exist.\n");
        lua_pop(Gui.LuaState, 1);
        return false;
    }
#endif

    lua_pushlightuserdata(Gui.LuaState, this);
    lua_rawget(Gui.LuaState, -2);

    // Remove the __windows_list_cf table from the stack, so that only the window table is left.
    lua_remove(Gui.LuaState, -2);

#ifdef DEBUG
    if (!lua_istable(Gui.LuaState, -1))
    {
        // This should never happen, or otherwise we had a WindowT instance here that was already garbage collected by Lua!
        assert(false);
        Console->Warning("Lua table for WindowT instance does not exist!\n");
        lua_pop(Gui.LuaState, 1);
        return false;
    }
#endif

    return true;
}


void WindowT::FillMemberVars()
{
    MemberVars["time"]=MemberVarT(Time);
    MemberVars["show"]=MemberVarT(ShowWindow);

    MemberVars["rect"]=MemberVarT(MemberVarT::TYPE_FLOAT4, &Rect[0]);
    MemberVars["pos"]=MemberVarT(MemberVarT::TYPE_FLOAT2, &Rect[0]);
    MemberVars["size"]=MemberVarT(MemberVarT::TYPE_FLOAT2, &Rect[2]);
    MemberVars["pos.x"]=MemberVarT(Rect[0]);
    MemberVars["pos.y"]=MemberVarT(Rect[1]);
    MemberVars["size.x"]=MemberVarT(Rect[2]);
    MemberVars["size.y"]=MemberVarT(Rect[3]);

    MemberVars["backColor"]=MemberVarT(MemberVarT::TYPE_FLOAT4, &BackColor[0]);
    MemberVars["backColor.r"]=MemberVarT(BackColor[0]);
    MemberVars["backColor.g"]=MemberVarT(BackColor[1]);
    MemberVars["backColor.b"]=MemberVarT(BackColor[2]);
    MemberVars["backColor.a"]=MemberVarT(BackColor[3]);

    MemberVars["borderColor"]=MemberVarT(MemberVarT::TYPE_FLOAT4, &BorderColor[0]);
    MemberVars["borderColor.r"]=MemberVarT(BorderColor[0]);
    MemberVars["borderColor.g"]=MemberVarT(BorderColor[1]);
    MemberVars["borderColor.b"]=MemberVarT(BorderColor[2]);
    MemberVars["borderColor.a"]=MemberVarT(BorderColor[3]);

    MemberVars["textColor"]=MemberVarT(MemberVarT::TYPE_FLOAT4, &TextColor[0]);
    MemberVars["textColor.r"]=MemberVarT(TextColor[0]);
    MemberVars["textColor.g"]=MemberVarT(TextColor[1]);
    MemberVars["textColor.b"]=MemberVarT(TextColor[2]);
    MemberVars["textColor.a"]=MemberVarT(TextColor[3]);

    MemberVars["rotAngle"]=MemberVarT(RotAngle);
    MemberVars["borderWidth"]=MemberVarT(BorderWidth);
    MemberVars["text"]=MemberVarT(Text);
    MemberVars["textScale"]=MemberVarT(TextScale);
    MemberVars["textAlignHor"]=MemberVarT(MemberVarT::TYPE_INT, &TextAlignHor);
    MemberVars["textAlignVer"]=MemberVarT(MemberVarT::TYPE_INT, &TextAlignVer);
}


/**********************************************/
/*** Impementation of Lua binding functions ***/
/**********************************************/

int WindowT::Set(lua_State* LuaState)
{
    WindowT*          Win    =(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 1, TypeInfo);
    std::string       VarName=luaL_checkstring(LuaState, 2);
    const MemberVarT& Var    =Win->MemberVars[VarName];

    if (Var.Member==NULL)
    {
        // Special-case treatment of the background material and font (write-only values).
        if (VarName=="backMaterial")
        {
            const std::string NewMaterialName=luaL_checkstring(LuaState, 3);

            if (NewMaterialName!=Win->BackRenderMatName)
            {
                // This code has intentionally *NOT* been made fail-safe(r), so that the script can "clear" the BackRenderMat
                // back to NULL again by specifying an invalid material name, e.g. "", "none", "NULL", "default", etc.
                Win->BackRenderMatName=NewMaterialName;
                MatSys::Renderer->FreeMaterial(Win->BackRenderMat);
                Win->BackRenderMat=Win->BackRenderMatName.empty() ? NULL : MatSys::Renderer->RegisterMaterial(MaterialManager->GetMaterial(Win->BackRenderMatName));
            }
            return 0;
        }
        else if (VarName=="font")
        {
            const std::string FontName=luaL_checkstring(LuaState, 3);

            Win->Font=GuiMan->GetFont(FontName);
            return 0;
        }

        // Bad argument "VarName".
        luaL_argerror(LuaState, 2, (std::string("unknown field '")+VarName+"'").c_str());
        return 0;
    }

    switch (Var.Type)
    {
        case MemberVarT::TYPE_FLOAT:
            ((float*)Var.Member)[0]=float(lua_tonumber(LuaState, 3));
            break;

        case MemberVarT::TYPE_FLOAT2:
            ((float*)Var.Member)[0]=float(lua_tonumber(LuaState, 3));
            ((float*)Var.Member)[1]=float(lua_tonumber(LuaState, 4));
            break;

        case MemberVarT::TYPE_FLOAT4:
            ((float*)Var.Member)[0]=float(lua_tonumber(LuaState, 3));
            ((float*)Var.Member)[1]=float(lua_tonumber(LuaState, 4));
            ((float*)Var.Member)[2]=float(lua_tonumber(LuaState, 5));
            ((float*)Var.Member)[3]=float(lua_tonumber(LuaState, 6));
            break;

        case MemberVarT::TYPE_INT:
            ((int*)Var.Member)[0]=lua_tointeger(LuaState, 3);
            break;

        case MemberVarT::TYPE_BOOL:
            // I also want to treat the number 0 as false, not just "false" and "nil".
            if (lua_isnumber(LuaState, 3)) ((bool*)Var.Member)[0]=lua_tointeger(LuaState, 3)!=0;
                                      else ((bool*)Var.Member)[0]=lua_toboolean(LuaState, 3)!=0;
            break;

        case MemberVarT::TYPE_STRING:
        {
            const char* s=lua_tostring(LuaState, 3);

            ((std::string*)Var.Member)[0]=(s!=NULL) ? s : "";
            break;
        }
    }

    return 0;
}


int WindowT::Get(lua_State* LuaState)
{
    WindowT*          Win    =(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 1, TypeInfo);
    std::string       VarName=luaL_checkstring(LuaState, 2);
    const MemberVarT& Var    =Win->MemberVars[VarName];

    if (Var.Member==NULL)
    {
        // Bad argument "VarName".
        luaL_argerror(LuaState, 2, (std::string("unknown field '")+VarName+"'").c_str());
        return 0;
    }

    switch (Var.Type)
    {
        case MemberVarT::TYPE_FLOAT:
            lua_pushnumber(LuaState, ((float*)Var.Member)[0]);
            return 1;

        case MemberVarT::TYPE_FLOAT2:
            lua_pushnumber(LuaState, ((float*)Var.Member)[0]);
            lua_pushnumber(LuaState, ((float*)Var.Member)[1]);
            return 2;

        case MemberVarT::TYPE_FLOAT4:
            lua_pushnumber(LuaState, ((float*)Var.Member)[0]);
            lua_pushnumber(LuaState, ((float*)Var.Member)[1]);
            lua_pushnumber(LuaState, ((float*)Var.Member)[2]);
            lua_pushnumber(LuaState, ((float*)Var.Member)[3]);
            return 4;

        case MemberVarT::TYPE_INT:
            lua_pushinteger(LuaState, ((int*)Var.Member)[0]);
            return 1;

        case MemberVarT::TYPE_BOOL:
            lua_pushboolean(LuaState, ((bool*)Var.Member)[0]);
            return 1;

        case MemberVarT::TYPE_STRING:
            lua_pushstring(LuaState, ((std::string*)Var.Member)[0].c_str());
            return 1;
    }

    return 0;
}


int WindowT::Interpolate(lua_State* LuaState)
{
    WindowT*          Win    =(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 1, TypeInfo);
    std::string       VarName=luaL_checkstring(LuaState, 2);
    const MemberVarT& Var    =Win->MemberVars[VarName];

    if (Var.Member==NULL)
    {
        // Bad argument "VarName".
        luaL_argerror(LuaState, 2, (std::string("unknown field '")+VarName+"'").c_str());
        return 0;
    }

    switch (Var.Type)
    {
        case MemberVarT::TYPE_FLOAT:
        {
            const unsigned long MAX_INTERPOLATIONS=10;
            unsigned long       InterpolationCount=0;

            // Make sure that there are no more than MAX_INTERPOLATIONS interpolations pending for Var already.
            // If so, just delete the oldest ones, which effectively means to skip them (the next youngest interpolation will take over).
            // The purpose is of course to prevent anything from adding arbitrarily many interpolations, eating up memory,
            // which could happen from bad user code (e.g. if the Cafu game code doesn't protect multiple human players from using
            // a GUI simultaneously, mouse cursor "position flickering" might occur on the server, which in turn might trigger the
            // permanent addition of interpolations from OnFocusLose()/OnFocusGain() scripts).
            for (unsigned long INr=Win->PendingInterpolations.Size(); INr>0; INr--)
            {
                InterpolationT* I=Win->PendingInterpolations[INr-1];

                if (I->Value==(float*)Var.Member) InterpolationCount++;

                if (InterpolationCount>MAX_INTERPOLATIONS)
                {
                    delete I;
                    Win->PendingInterpolations.RemoveAtAndKeepOrder(INr-1);
                    break;
                }
            }

            // Now add the new interpolation to the pending list.
            InterpolationT* I=new InterpolationT;

            I->Value      =(float*)Var.Member;
            I->StartValue =float(lua_tonumber(LuaState, 3));
            I->EndValue   =float(lua_tonumber(LuaState, 4));
            I->CurrentTime=0.0f;
            I->TotalTime  =float(lua_tonumber(LuaState, 5)/1000.0);

            Win->PendingInterpolations.PushBack(I);
            break;
        }

        default:
        {
            luaL_argerror(LuaState, 2, (std::string("Cannot interpolate over field '")+VarName+"', it is not of type 'float'.").c_str());
            break;
        }
    }

    return 0;
}


int WindowT::GetName(lua_State* LuaState)
{
    WindowT* Win=(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 1, TypeInfo);

    lua_pushstring(LuaState, Win->Name.c_str());
    return 1;
}


int WindowT::SetName(lua_State* LuaState)
{
    WindowT* Win=(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 1, TypeInfo);

    Win->Name=luaL_checkstring(LuaState, 2);
    return 0;
}


int WindowT::AddChild(lua_State* LuaState)
{
    WindowT* Win  =(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 1, TypeInfo);
    WindowT* Child=(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 2, TypeInfo);

    if (Child->Parent!=NULL)
        return luaL_argerror(LuaState, 2, "child window already has a parent, use RemoveChild() first");

    Win->Children.PushBack(Child);
    Child->Parent=Win;


    // We must do the equivalent to   Win->Children.PushBack(Child);   now in Lua:
    // Add the child instance to the list of children of the parent window.
    // This is important in order to make sure that the child is not garbage-collected early
    // if the caller chooses to not keep a reference to it after the call to AddChild().
    // In practice, this is very likely to happen, just consider this example:
    //     function createMyNewWindow() local myWin=gui:new("WindowT"); ...; return myWin; end
    //     Parent:AddChild(createMyNewWindow());
    //
    // This is achieved by placing (a reference to) the table that represents the child
    // into the __children_cf array of the parent. As key for indexing the array we can
    // use almost anything we want, like a globally unique ID number of the child window,
    // or something like a[a#+1]. Here, I use the window pointer itself as a unique ID.
    //
    // In summary, we do:   ParentTable.__children_cf[Child]=ChildTable
    lua_pushstring(LuaState, "__children_cf");
    lua_rawget(LuaState, 1);

    if (!lua_istable(LuaState, -1))
    {
        lua_pop(LuaState, 1);       // Remove whatever was not a table.
        lua_newtable(LuaState);     // Push a new table instead.

        lua_pushstring(LuaState, "__children_cf");
        lua_pushvalue(LuaState, -2);
        lua_rawset(LuaState, 1);    // ParentTable.__children_cf = new_table;
    }

    lua_pushlightuserdata(LuaState, Child);
    lua_pushvalue(LuaState, 2);
    lua_rawset(LuaState, -3);

    // And finally the equivalent of   Child->Parent=Win;
    // This can not be omitted even though we have the __children_cf already,
    // because otherwise Lua might garbage collect our parent, as for example in:
    //     a=gui:new("WindowT"); b=gui:new("WindowT"); a:AddChild(b); gui:SetRootWindow(b); a=nil;    -- Yes, b is set as the root window here.
    lua_pushstring(LuaState, "__parent_cf");
    lua_pushvalue(LuaState, 1);
    lua_rawset(LuaState, 2);
    return 0;
}


int WindowT::RemoveChild(lua_State* LuaState)
{
    WindowT* Parent=(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 1, TypeInfo);
    WindowT* Child =(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 2, TypeInfo);

    if (Child->Parent!=Parent)
        return luaL_argerror(LuaState, 2, "window is the child of another parent");

    const int Index=Parent->Children.Find(Child);

    if (Index<0)
        return luaL_argerror(LuaState, 2, "window not found among the children of its parent");

    Parent->Children.RemoveAtAndKeepOrder(Index);
    Child->Parent=NULL;


    // We must do the equivalent to   Win->Children.RemoveAtAndKeepOrder(Index);   now in Lua:
    // Remove the child instance from the list of children of the window.
    // This is important in order to make sure that the child can now be garbage-collected again
    // (as far as the parent window is concerned).
    //
    // Analogous to AddChild(), we do:   ParentTable.__children_cf[Child]=nil
    lua_pushstring(LuaState, "__children_cf");
    lua_rawget(LuaState, 1);

#ifdef DEBUG
    if (!lua_istable(LuaState, -1))
    {
        // Should never get here:
        // AddChild() should have correctly created the __children_cf table
        // and children that are not ours should have caused Index<0 above.
        // So the only way is if the user script fiddled with the __children_cf directly...
        return luaL_error(LuaState, "WARNING: Could not obtain the list of children of the parent window.");
    }
#endif

    lua_pushlightuserdata(LuaState, Child);
    lua_pushnil(LuaState);
    lua_rawset(LuaState, -3);

    // And finally the equivalent of   Child->Parent=NULL;
    lua_pushstring(LuaState, "__parent_cf");
    lua_pushnil(LuaState);
    lua_rawset(LuaState, 2);
    return 0;
}


int WindowT::Destruct(lua_State* LuaState)
{
    // This doesn't work, because we're getting the userdata item here directly,
    // not the table that represents our windows normally and has the userdata embedded.
 // WindowT* Win=(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 1, TypeInfo);

    // TODO: Do we need a stricter type-check here? Contrary to the example in the PiL2 29.1,
    //       I think there *are* ways for the Lua script to call this function.
    WindowT** UserData=(WindowT**)lua_touserdata(LuaState, 1); if (UserData==NULL) luaL_error(LuaState, "NULL userdata in __gc handler.");
    WindowT*  Win     =(*UserData);

    // Console->DevPrint(cf::va("#################### Deleting window instance at %p of type \"%s\" and name \"%s\".\n",
    //     Win, Win->GetType()->ClassName, Win->Name.c_str()));

    delete Win;
    return 0;
}


int WindowT::toString(lua_State* LuaState)
{
    WindowT* Win=(WindowT*)cf::GuiSys::GuiImplT::GetCheckedObjectParam(LuaState, 1, TypeInfo);

    lua_pushfstring(LuaState, "A gui window with name \"%s\".", Win->Name.c_str());
    return 1;
}
