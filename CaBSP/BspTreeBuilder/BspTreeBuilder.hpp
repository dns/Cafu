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

#ifndef CAFU_BSPTREEBUILDER_HPP_INCLUDED
#define CAFU_BSPTREEBUILDER_HPP_INCLUDED

#include "Math3D/Brush.hpp"
#include "Math3D/Polygon.hpp"
#include "Math3D/Vector3.hpp"

#if defined(_WIN32) && _MSC_VER<1600
#include "pstdint.h"            // Paul Hsieh's portable implementation of the stdint.h header.
#else
#include <stdint.h>
#endif


class MaterialT;
namespace cf { namespace SceneGraph { class BspTreeNodeT; } }
namespace cf { namespace SceneGraph { class FaceNodeT; } }
namespace cf { namespace SceneGraph { class GenericNodeT; } }


class BspTreeBuilderT
{
    public:

    cf::SceneGraph::BspTreeNodeT*                BspTree;
 // ArrayT<cf::SceneGraph::BspTreeNodeT::NodeT>& Nodes;
 // ArrayT<cf::SceneGraph::BspTreeNodeT::LeafT>& Leaves;
    ArrayT<uint32_t>&                            PVS;
    ArrayT<cf::SceneGraph::FaceNodeT*>&          FaceChildren;
    ArrayT<cf::SceneGraph::GenericNodeT*>&       OtherChildren;
    ArrayT<VectorT>&                             GlobalDrawVertices;

    const bool Option_MostSimpleTree;
    const bool Option_MinimizeFaceSplits;


    BspTreeBuilderT(cf::SceneGraph::BspTreeNodeT* BspTree_, bool MostSimpleTree, bool MinFaceSplits);

    void Build(bool IsWorldspawn,
               const ArrayT<Vector3dT>& FloodFillSources_,
               const ArrayT<Vector3dT>& OutsidePointSamples,
               const std::string& MapFileName /*for .pts pointfile*/);


    // Alle sich schneidenden Faces werden geteilt, sodaß keine Schnitte mehr übrig bleiben.
    // Splits werden aber nur dann durchgeführt, wenn dies nicht zu ungültigen Polygonen führt!
    // Übergeordnetes Ziel ist es, von sich schneidenden Faces so viel wie möglich beim "FloodFillInside" loswerden zu können,
    // sodaß am Schluß nur eine perfekt glatte Haut der Welt übrigbleibt!
    void ChopUpFaces();

    // Prepare the world for leak detection. Must be called before 'BuildBSPTree()'.
    // Never pollute the final BSP tree with the polygons generated by this function!
    void PrepareLeakDetection(const ArrayT<Vector3dT>& FloodFillSources, MaterialT* LeakDetectMat);

    // Builds a BSP tree from the 'Faces'.
    // The created nodes and leaves are stored in the 'Nodes' and 'Leaves' members, respectively.
    // Note that some members of the leaves remain empty.
    // Also the 'Faces' array will be modified during the process when BSP splits occur.
    void BuildBSPTree();

    // Creates the 'Portals' for the leaves.
    void Portalize();

    // Starting from the 'FloodFillOrigins', this flood-fills the world.
    // The 'IsInnerLeaf' member of all reached leaves is set to true.
    void FloodFillInside(const ArrayT<VectorT>& FloodFillOrigins, const ArrayT<VectorT>& WorldOutsidePointSamples);

    // Detects if a world has leaks. 'PrepareLeakDetection()' must have been called earlier.
    // Must be called after 'FloodFillInside()' and before 'RemoveOuterFaces()' resp. 'RemoveOuterFacesTIsAndTDs()'.
    void DetectLeaks(const ArrayT<VectorT>& FloodFillOrigins, const std::string& WorldFileName, MaterialT* LeakDetectMat);

    // This fills the 'OtherChildrenSet' of the leaves.
    void AssignOtherChildrenToLeaves();

    // Removes all contents from the 'Faces' array that is not referenced by an "inner" leaf.
    // The contents of the 'Nodes' and 'Leaves' becomes invalid thereafter, and thus is deleted.
    void RemoveOuterFaces();

    // Removes portals from outer leaves. The tree remains intact!
    void RemoveOuterPortals();

    // Merges faces as far as possible (but they must remain convex and their TexInfos must match).
    // The contents of the 'Nodes' and 'Leaves', if any, becomes invalid.
    void MergeFaces();

    // Sortiert die Faces gemäß ihres Texture-Namens und paßt die anderen Datenstrukturen entsprechend an.
    // Der BSP-Baum bleibt also intakt.
    void SortFacesIntoTexNameOrder();

    // Splits up faces that are too large to be covered by a single LightMap.
    void ChopUpForMaxLightMapSize();

    // Splits up faces that are too large to be covered by a single SHLMap.
    void ChopUpForMaxSHLMapSize();

    // Creates full-bright LightMaps for all faces.
    // After this function was called, the 'Faces' MUST REMAIN UNMODIFIED! No further split or sorting operations!
    void CreateFullBrightLightMaps();

    // Creates empty, zero-band SHLMaps for all faces.
    // After this function was called, the 'Faces' MUST REMAIN UNMODIFIED! No further split or sorting operations!
    void CreateZeroBandSHLMaps();

    // Basing on the contents of the 'Faces' array, this function computes the 'FacesDrawVertices' and 'FacesDrawIndices'.
    // The result will have all T-Junctions removed and can be used with the OpenGL "glDrawElements()" function.
    void ComputeDrawStructures();

    // Creates the trivial full-visibility PVS information for all leaves,
    // that is, each leaf can see every other leaf.
    void CreateFullVisPVS();


    private:

    Plane3T<double> ChooseSplitPlane(const ArrayT<unsigned long>& FaceSet) const;
    void BuildBSPTreeRecursive(const ArrayT<unsigned long>& FaceSet);
    void FillBSPLeaves(unsigned long NodeNr, const ArrayT<cf::SceneGraph::FaceNodeT*>& Face2, const ArrayT<unsigned long>& FaceSet, const BoundingBox3T<double>& BB);
    void CreateLeafPortals(unsigned long LeafNr, const ArrayT< Plane3T<double> >& NodeList);
    void BuildBSPPortals(unsigned long NodeNr, ArrayT< Plane3T<double> >& NodeList);
    void FloodFillInsideRecursive(unsigned long Leaf1Nr);
    void ComputeLeakPathByBFS(const VectorT& Start) const;
    void LeakDetected(const VectorT& InfoPlayerStartOrigin, const std::string& PointFileName, const unsigned long LeafNr) const;
    void QuickSortFacesIntoTexNameOrder();
};

#endif
