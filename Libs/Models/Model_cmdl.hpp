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

#ifndef _CAFU_MODEL_HPP_
#define _CAFU_MODEL_HPP_

#include "Templates/Array.hpp"
#include "MaterialSystem/Mesh.hpp"
#include "Math3D/BoundingBox.hpp"
#include "Math3D/Matrix.hpp"
#include "Model.hpp"


class MaterialT;
class ModelLoaderT;
namespace MatSys { class RenderMaterialT; }


/// This class represents a native Cafu model.
class CafuModelT : public ModelT
{
    public:

    /// A joint is a bone in the hierarchical skeleton of the model.
    struct JointT
    {
        std::string Name;
        int         Parent;     ///< The parent of the root joint is -1.
        Vector3fT   Pos;
        Vector3fT   Qtr;
    };


    /// A mesh consisting of a material and a list of vertices.
    struct MeshT
    {
        /// A single triangle.
        struct TriangleT
        {
            TriangleT(unsigned int v0=0, unsigned int v1=0, unsigned int v2=0);

            unsigned int VertexIdx[3];  ///< The indices to the three vertices that define this triangle.

            int          NeighbIdx[3];  ///< The array indices of the three neighbouring triangles at the edges 01, 12 and 20. -1 indicates no neighbour, -2 indicates more than one neighbour.
            bool         Polarity;      ///< True if this triangle has positive polarity (texture is not mirrored), or false if it has negative polarity (texture is mirrored, SxT points inward).

            Vector3fT    Draw_Normal;   ///< The draw normal for this triangle, required for the shadow-silhouette determination.
        };

        /// A single vertex.
        struct VertexT
        {
            float                u;             ///< Texture coordinate u.
            float                v;             ///< Texture coordinate v.
            unsigned int         FirstWeightIdx;
            unsigned int         NumWeights;

            bool                 Polarity;      ///< True if this vertex belongs to triangles with positive polarity, false if it belongs to triangles with negative polarity. Note that a single vertex cannot belong to triangles of both positive and negative polarity (but a GeoDup of this vertex can belong to the other polarity).
            ArrayT<unsigned int> GeoDups;       ///< This array contains the indices of vertices that are geometrical duplicates of this vertex, see AreVerticesGeoDups() for more information. The indices are stored in increasing order, and do *not* include the index of "this" vertex. Note that from the presence of GeoDups in a cmdl/md5 file we can *not* conclude that a break in the smoothing was intended by the modeller. Cylindrically wrapping seams are one counter-example.

            Vector3fT            Draw_Pos;      ///< Position of this vertex.
            Vector3fT            Draw_Normal;   ///< Vertex normal.
            Vector3fT            Draw_Tangent;  ///< Vertex tangent.
            Vector3fT            Draw_BiNormal; ///< Vertex binormal.
        };

        /// A weight is a fixed position in the coordinate system of a joint.
        struct WeightT
        {
            unsigned int JointIdx;
            float        Weight;
            Vector3fT    Pos;
        };


        /// Determines whether the two vertices with array indices Vertex1Nr and Vertex2Nr are geometrical duplicates of each other.
        /// Two distinct vertices are geometrical duplicates of each other if
        /// (a) they have the same NumWeights and the same FirstWeightIdx, or
        /// (b) they have the same NumWeights and the referred weights have the same contents.
        /// The two vertices may in general have different uv-coordinates, which are not considered for the geodup property.
        /// Note that if Vertex1Nr==Vertex2Nr, true is returned (case (a) above).
        /// @param Vertex1Nr Array index of first vertex.
        /// @param Vertex2Nr Array index of second vertex.
        /// @return Whether the vertices are geometrical duplicates of each other.
        bool AreGeoDups(unsigned int Vertex1Nr, unsigned int Vertex2Nr) const;

        MaterialT*               Material;       ///< The material of this mesh.
        MatSys::RenderMaterialT* RenderMaterial; ///< The render material used to render this mesh.
        ArrayT<TriangleT>        Triangles;      ///< List of triangles this mesh consists of.
        ArrayT<VertexT>          Vertices;       ///< List of vertices this mesh consists of.
        ArrayT<WeightT>          Weights;        ///< List of weights that are attached to the skeleton (hierarchy of bones/joints).
    };


    /// This struct describes one animation sequence, e.g. "run", "walk", "jump", etc.
    /// We use it to obtain an array of joints (ArrayT<JointT>, just like m_Joints) for any point (frame number) in the animation sequence.
    struct AnimT
    {
        struct AnimJointT
        {
         // std::string  Name;              ///< Checked to be identical with the name  of the base mesh at load time.
         // int          Parent;            ///< Checked to be identical with the value of the base mesh at load time.
            float        BaseValues[6];     ///< One position and one quaternion triple that are used for all frames in this anim sequence.
            unsigned int Flags;             ///< If the i-th bit in Flags is *not* set, BaseValue[i] is used for all frames in this anim sequence. Otherwise, for each set bit, we find frame-specific values in AnimData[FirstDataIdx+...].
            unsigned int FirstDataIdx;      ///< If f is the current frame, this is the index into Frames[f].AnimData[...].
         // int          NumDatas;          ///< There are so many data values as there are bits set in Flags.
        };

        /// A keyframe in the animation.
        struct FrameT
        {
            BoundingBox3fT BB;              ///< The bounding box of the model in this frame.
            ArrayT<float>  AnimData;
        };


     // std::string        Name;            ///< Name (label) of this animation sequence.
        float              FPS;             ///< Playback rate for this animation sequence.
     // int                Next;            ///< The sequence that should play after this. Use "this" for looping sequences, "none" for none.
     // ...                Events;          ///< E.g. "call a script function at frame 3".
        ArrayT<AnimJointT> AnimJoints;      ///< AnimJoints.Size() == m_Joints.Size()
        ArrayT<FrameT>     Frames;          ///< List of keyframes this animation consists of.
    };


    /// This structure is used to describe the locations where GUIs can be attached to the model.
    /// Note that the current static/fixed-position implementation (origin, x- and y-axis) is temporary though,
    /// it should eventually be possible to attach GUIs even to animated models.
    struct GuiLocT
    {
        Vector3fT Origin;
        Vector3fT AxisX;
        Vector3fT AxisY;
    };


    /// The constructor. Creates a new Cafu model from a file as directed by the given model loader.
    /// @param Loader   The model loader that actually imports the file and fills in the model data.
    CafuModelT(ModelLoaderT& Loader);

    /// The destructor.
    ~CafuModelT();

    /// Saves the model into the given stream.
    void Save(std::ostream& OutStream) const;

    // Inspector methods.
    bool GetUseGivenTS() const { return m_UseGivenTangentSpace; }
    const ArrayT<JointT>& GetJoints() const { return m_Joints; }
    const ArrayT<MeshT>&  GetMeshes() const { return m_Meshes; }
    const ArrayT<AnimT>&  GetAnims()  const { return m_Anims; }

    /// This method returns the set of drawing matrices (one per joint) at the given sequence and frame number.
    const ArrayT<MatrixT>& GetDrawJointMatrices(int SequenceNr, float FrameNr) const;

    // The ModelT interface.
    const std::string& GetFileName() const;     // TODO: Remove!?!
    void               Draw(int SequenceNr, float FrameNr, float LodDist, const ModelT* SubModel=NULL) const;
    bool               GetGuiPlane(int SequenceNr, float FrameNr, float LodDist, Vector3fT& GuiOrigin, Vector3fT& GuiAxisX, Vector3fT& GuiAxisY) const;
    void               Print() const;
    int                GetNrOfSequences() const;
    BoundingBox3fT     GetBB(int SequenceNr, float FrameNr) const;
    bool               TraceRay(int SequenceNr, float FrameNr, const Vector3fT& RayOrigin, const Vector3fT& RayDir, TraceResultT& Result) const;
 // float              GetNrOfFrames(int SequenceNr) const;
    float              AdvanceFrameNr(int SequenceNr, float FrameNr, float DeltaTime, bool Loop=true) const;


    private:

    void InitMeshes();                                                  ///< An auxiliary method for the constructors.
    void UpdateCachedDrawData(int SequenceNr, float FrameNr) const;     ///< A private auxiliary method.


    const std::string     m_FileName;               ///< File name of this model.   TODO: Remove!?!
    const bool            m_UseGivenTangentSpace;
 // const bool            m_CastShadows;            ///< Should this model cast shadows?
    ArrayT<JointT>        m_Joints;                 ///< Array of joints of this model.
    mutable ArrayT<MeshT> m_Meshes;                 ///< Array of (sub)meshes of this model.
    BoundingBox3fT        m_BasePoseBB;             ///< The bounding-box for the base pose of the model.
    ArrayT<AnimT>         m_Anims;                  ///< Array of animations of this model.
    ArrayT<GuiLocT>       m_GuiLocs;                ///< Array of locations where GUIs can be attached to this model.

    mutable ArrayT<MatrixT>       m_Draw_JointMatrices;
    mutable ArrayT<MatSys::MeshT> m_Draw_Meshes;

    mutable int   m_Draw_CachedDataAtSequNr;
    mutable float m_Draw_CachedDataAtFrameNr;
};

#endif
