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

#ifndef CAFU_CLIPSYS_TRACESOLID_HPP_INCLUDED
#define CAFU_CLIPSYS_TRACESOLID_HPP_INCLUDED

#include "Math3D/BoundingBox.hpp"
#include "Math3D/Matrix3x3.hpp"
#include "Math3D/Plane3.hpp"


namespace cf
{
    namespace ClipSys
    {
        /// This class represents a solid object that can be traced through collision worlds, models and shapes.
        /// The shape of the object is defined as a convex polyhedron of arbitrary complexity.
        /// However, polyhedrons of the least possible complexity (regarding the number of vertices, planes and edges)
        /// are strongly preferable regarding performance considerations.
        ///
        /// Moreover, the solids must be "well defined" without degeneracies. In particular, this means:
        ///   - there must be no duplicate or colinear vertices,
        ///   - there must be no duplicate or redundant planes, and
        ///   - the solid must have a true positive volume.
        /// The only exception to these rules is a trace solid with exactly one vertex, no planes and no edges.
        /// Such trace solids are treated equivalently to point traces, so that special-case optimized code is employed.
        class TraceSolidT
        {
            public:

            /// This struct describes an edge of a TraceSolidT.
            struct EdgeT
            {
                unsigned int A;   ///< Index of the first  vertex of this edge.
                unsigned int B;   ///< Index of the second vertex of this edge.
            };


            /// Creates an empty (invalid) trace model.
            TraceSolidT();

            /// Creates a trace solid from (in the shape of) the given axis-aligned bounding-box.
            TraceSolidT(const BoundingBox3dT& BB);

            /// Updates this trace solid to the shape of the given bounding-box.
            /// Compared to creating a new trace solid, this can significantly reduce or even
            /// completely eliminate the required memory (re-)allocations.
            void SetBB(const BoundingBox3dT& BB);

            /// Assigns the given solid to this one, transformed by the *transpose* of the given matrix.
            /// Compared to creating a new trace solid, this can significantly reduce or even
            /// completely eliminate the required memory (re-)allocations.
            void AssignInvTransformed(const TraceSolidT& Other, const math::Matrix3x3dT& Mat);

            /// Returns the bounding-box of (the vertices of) this solid.
            BoundingBox3dT GetBB() const
            {
                BoundingBox3dT BB;

                for (unsigned int i = 0; i < GetNumVertices(); i++)
                    BB += GetVertices()[i];

                return BB;
            }

            /// Returns the number of vertices of this solid.
            unsigned int GetNumVertices() const { return m_Vertices.Size(); }

            /// Returns the vertices of this solid.
            const Vector3dT* GetVertices() const { return &m_Vertices[0]; }

            /// Returns the number of planes of this solid.
            unsigned int GetNumPlanes() const { return m_Planes.Size(); }

            /// Returns the planes of this solid.
            const Plane3dT* GetPlanes() const { return &m_Planes[0]; }

            /// Returns the number of edges of this solid.
            unsigned int GetNumEdges() const { return m_Edges.Size(); }

            /// Returns the edges of this solid.
            const EdgeT* GetEdges() const { return &m_Edges[0]; }


            private:

            ArrayT<Vector3dT> m_Vertices;
            ArrayT<Plane3dT>  m_Planes;
            ArrayT<EdgeT>     m_Edges;
        };
    }
}

#endif
