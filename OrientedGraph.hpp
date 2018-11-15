//
//  OrientedGraph.hpp - Class which compactly stores an adjacency matrix
//     for a graph, in which each vertex may have a maximum of one
//     directed connection to any other vertex.  It is built upon the
//     Nstate class for efficiently building arrays of tristates, and
//     the memory layout is optimized for dynamically growing and
//     shrinking the graph capacity.
//
//  Copyright (c) 2009 HostileFork.com
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://hostilefork.com/nocycle for documentation.
//

#pragma once

#include "NocycleConfig.hpp"

#include <limits> // numeric_limits
#include <set>
#include <cassert>

#include "Nstate.hpp"
//#include "nstate/Nstate.hpp"

namespace nocycle {

// The graph will grow dynamically to accomodate
class OrientedGraph {
  public:
    typedef unsigned VertexID;
    class EdgeIter;
    friend class EdgeIter;

  private:
    enum VertexExistenceTristate {
        doesNotExist = 0,
        existsAsTypeOne = 1,
        existsAsTypeTwo = 2
    };
    enum VertexConnectionTristate {
        notConnected = 0,
        lowPointsToHigh = 1,
        highPointsToLow = 2
    };

  public:
    enum VertexType {
        vertexTypeOne,
        vertexTypeTwo
    };

  private:
    NstateArray<3> m_buffer;

  private:
    // E(N) => N*(N-1)/2
    // Explained at http://hostilefork.com/nocycle/
    inline size_t TristateIndexForExistence(VertexID vertexE) const {
        assert(vertexE < std::numeric_limits<unsigned>::max());
        return (vertexE * static_cast<unsigned long long>(vertexE + 1)) / 2;
    }

    // C(S,L) => E(L) + (L - S)
    // Explained at http://hostilefork.com/nocycle/
    inline size_t TristateIndexForConnection(VertexID vertexS, VertexID vertexL) const {
        assert(vertexL < std::numeric_limits<unsigned>::max());
        assert(vertexS < vertexL);

        return TristateIndexForExistence(vertexL) + (vertexL - vertexS);
    }

    void VertexFromExistenceTristateIndex(size_t pos, VertexID& vertexE) const {
        // The index into the NstateArray can be *big*, and multiplying by 8 can exceed size_t
        // Must cast to an unsigned long long to do this calculation
        vertexE = static_cast<VertexID>((sqrt(1 + 8 * static_cast<unsigned long long>(pos)) - 1) / 2);
        assert(TristateIndexForExistence(vertexE) == pos); // should be *exact*, not rounded down!!!!
    }

    void VerticesFromConnectionTristateIndex(size_t pos, VertexID& vertexS, VertexID& vertexL) const {
        vertexL = static_cast<VertexID>((sqrt(1 + 8 * static_cast<unsigned long long>(pos)) - 1) / 2);
        size_t tife = TristateIndexForExistence(vertexL);
        assert(tife != pos); // should be rounded down, not *exact*!!!
        /* vertexL - vertexS = pos - tife; */
        assert (tife + vertexL > pos);
        vertexS = (tife + vertexL) - pos;
        assert(vertexL > vertexS);
        assert(TristateIndexForConnection(vertexS, vertexL) == pos);
    }

    bool IsExistenceTristateIndex(size_t pos) const {
        return TristateIndexForExistence(static_cast<VertexID>((sqrt(1 + 8 * static_cast<unsigned long long>(pos)) - 1) / 2)) == pos;
    }

    bool IsConnectionTristateIndex(size_t pos) const {
        return !IsExistenceTristateIndex(pos);
    }

  public:
    // We could cache this, but it can be computed from
    // the NstateArray length.
    //
    // Since E(N) => N*(N+1)/2...
    //    solve for N we get 0 = N^2 + N - 2E(N)
    //    quadratic equation tells us (-b +/- sqrt( b^2 - 4ac)) / 2a
    //    so ruling out negative N values... (-1 + sqrt(1 + 4*2E(N)))/2
    //    = (sqrt(1 + 8E(N)) - 1)/2
    VertexID GetFirstInvalidVertexID() const {
        if (m_buffer.Length() == 0)
            return 0; // Zero is not valid, we can't track its existence

        VertexID ret;
        VertexFromExistenceTristateIndex(m_buffer.Length(), ret);
        return ret; // this will be the number of the first invalid vertex
    }

    // Variant of GetFirstInvalidVertexID().  A little confusing interface, since we may have an empty graph and
    // we may also have a graph containing nothing but vertex 0...
    unsigned GetMaxValidVertexID(bool& noValidID) const {
        unsigned ret = GetFirstInvalidVertexID();
        if (ret == 0) {
            noValidID = true;
            return std::numeric_limits<unsigned>::max();
        }
        noValidID = false;
        return (ret - 1);
    }

    // This expands the buffer vector so that it can accommodate the existence and
    // connection data for vertexL.  Any new vertices added will not exist yet and not
    // have connection data.  Any vertices existing above this ID # will
    void SetCapacityForMaxValidVertexID(VertexID vertexL) {
        assert(vertexL < std::numeric_limits<unsigned>::max()); // max is reserved for max invalid vertex ID
        m_buffer.ResizeWithZeros(TristateIndexForExistence(vertexL + 1));
    }
    void SetCapacitySoVertexIsFirstInvalidID(VertexID vertexL) {
        if (vertexL == 0)
            m_buffer.ResizeWithZeros(0);
        else
            m_buffer.ResizeWithZeros(TristateIndexForExistence(vertexL));
    }
    void GrowCapacityForMaxValidVertexID(VertexID vertexL) {
        assert(vertexL >= GetFirstInvalidVertexID());
        SetCapacityForMaxValidVertexID(vertexL);
    }
    void ShrinkCapacitySoVertexIsFirstInvalidID(VertexID vertexL) {
        assert(vertexL < GetFirstInvalidVertexID());
        SetCapacitySoVertexIsFirstInvalidID(vertexL);
    }

    // This core routine is used to get vertex information, and it can also delete vertices and their connections while doing so
  private:
    void GetVertexInfoMaybeDestroy(
        VertexID vertexE,
        bool &exists,
        VertexType& vertexType,
        unsigned* incomingEdgeCount = NULL,
        unsigned* outgoingEdgeCount = NULL,
        std::set<VertexID>* incomingEdges = NULL,
        std::set<VertexID>* outgoingEdges = NULL,
        bool destroyIfExists = false,
        bool compactIfDestroy = true
    ){
        switch (m_buffer[TristateIndexForExistence(vertexE)]) {
          case doesNotExist:
            exists = false;
            break;

          case existsAsTypeOne:
            exists = true;
            vertexType = vertexTypeOne;
            break;

          case existsAsTypeTwo:
            exists = true;
            vertexType = vertexTypeTwo;
            break;

          default:
            assert(false);
        }

        if (outgoingEdgeCount != NULL)
            *outgoingEdgeCount = 0;
        if (incomingEdgeCount != NULL)
            *incomingEdgeCount = 0;

        if (!exists)
            return;

        // check connections, if requested
        if ((incomingEdgeCount != NULL) || (outgoingEdgeCount != NULL) || (incomingEdges != NULL) || (outgoingEdges != NULL)) {
            for (VertexID vertexT = 0; vertexT < GetFirstInvalidVertexID(); vertexT++) {
                if (vertexT == vertexE)
                    continue;

                VertexID vertexS = vertexT < vertexE ? vertexT : vertexE;
                VertexID vertexL = vertexT > vertexE ? vertexT : vertexE;

                switch (m_buffer[TristateIndexForConnection(vertexS, vertexL)]) {
                  case notConnected:
                    continue;

                  case lowPointsToHigh:
                    if (vertexE < vertexT) {
                        if (outgoingEdgeCount != NULL)
                            (*outgoingEdgeCount)++;
                        if (outgoingEdges != NULL)
                            outgoingEdges->insert(vertexT);
                    } else {
                        if (incomingEdgeCount != NULL)
                            (*incomingEdgeCount)++;
                        if (incomingEdges != NULL)
                            incomingEdges->insert(vertexT);
                    }
                    break;

                  case highPointsToLow:
                    if (vertexE > vertexT) {
                        if (outgoingEdgeCount != NULL)
                            (*outgoingEdgeCount)++;
                        if (outgoingEdges != NULL)
                            outgoingEdges->insert(vertexT);
                    } else {
                        if (incomingEdgeCount != NULL)
                            (*incomingEdgeCount)++;
                        if (incomingEdges != NULL)
                            incomingEdges->insert(vertexT);
                    }
                    break;

                  default:
                    assert(false);
                }

                // Destroying a vertex's existence also destroys all incoming and outgoing connections for that vertex
                if (destroyIfExists)
                    m_buffer[TristateIndexForConnection(vertexS, vertexL)] = notConnected;
            }
        }

        if (destroyIfExists && exists) {
            m_buffer[TristateIndexForExistence(vertexE)] = doesNotExist;

            // caller can tell us to make a destruction do a compaction
            // (because not all destroys compact, we may have trailing data...)
            // we can potentially save on the destroy loops above if we know we're going to shrink!
            if (compactIfDestroy) {
                bool noValidID;
                unsigned vertexT = GetMaxValidVertexID(noValidID);
                assert(!noValidID);

                if (m_buffer[TristateIndexForExistence(vertexT)] == doesNotExist) {
                    VertexID vertexFirstUnused = vertexT;
                    while (vertexFirstUnused > 0) {
                        if (m_buffer[TristateIndexForExistence(vertexFirstUnused - 1)] != doesNotExist)
                            break;
                        vertexFirstUnused = vertexFirstUnused - 1;
                    }
                    ShrinkCapacitySoVertexIsFirstInvalidID(vertexFirstUnused);
                }
            }
        }
    }
    void GetVertexInfo(VertexID vertexE, bool &exists, VertexType& vertexType) const {
        return const_cast<OrientedGraph*>(this)->GetVertexInfoMaybeDestroy(vertexE, exists, vertexType);
    }
    void GetVertexInfoMaybeCounts(VertexID vertexE, bool &exists, VertexType& vertexType, unsigned* incomingEdgeCount = NULL, unsigned* outgoingEdgeCount = NULL) const {
        return const_cast<OrientedGraph*>(this)->GetVertexInfoMaybeDestroy(vertexE, exists, vertexType, incomingEdgeCount, outgoingEdgeCount);
    }

  public:

    //
    // NODE EXISTENCE ROUTINES
    //

    // Sees if a vertex exists, and tells you if it exists in 'state one' or 'state two'
    inline bool VertexExistsEx(VertexID vertexE, VertexType& vertexType) const {
        bool exists;
        const_cast<OrientedGraph*>(this)->GetVertexInfoMaybeDestroy(vertexE, exists, vertexType);
        return exists;
    }
    inline bool VertexExists(VertexID vertexE) const {
        VertexType vertexType;
        return VertexExistsEx(vertexE, vertexType);
    }

    //
    // NODE CREATION
    //

    void CreateVertexEx(VertexID vertexE, VertexType vertexType) {
        assert(!VertexExists(vertexE));
        if (vertexType == vertexTypeOne) {
            m_buffer[TristateIndexForExistence(vertexE)] = existsAsTypeOne;
        } else {
            m_buffer[TristateIndexForExistence(vertexE)] = existsAsTypeTwo;
        }
    }
    inline void CreateVertex(VertexID vertexE) {
        return CreateVertexEx(vertexE, vertexTypeOne);
    }
    void SetVertexType(VertexID vertexE, VertexType vertexType) {
        assert(VertexExists(vertexE));
        if (vertexType == vertexTypeOne) {
            m_buffer[TristateIndexForExistence(vertexE)] = existsAsTypeOne;
        } else {
            m_buffer[TristateIndexForExistence(vertexE)] = existsAsTypeTwo;
        }
    }
    VertexType GetVertexType(VertexID vertexE) {
        bool exists;
        VertexType vertexType;
        GetVertexInfo(vertexE, exists, vertexType);
        assert(exists);
        return vertexType;
    }
    void FlipVertexType(VertexID vertexE) {
        bool exists;
        VertexType vertexType;
        GetVertexInfo(vertexE, exists, vertexType);
        assert(exists);
        if (vertexType == vertexTypeOne)
            SetVertexType(vertexE, vertexTypeTwo);
        else
            SetVertexType(vertexE, vertexTypeOne);
    }

    //
    // NODE DESTRUCTION ROUTINES
    //

    inline void DestroyVertexEx(VertexID vertex, VertexType& vertexType, bool compactIfDestroy = true, unsigned* incomingEdgeCount = NULL, unsigned* outgoingEdgeCount = NULL ) {
        bool exists;
        GetVertexInfoMaybeDestroy(vertex, exists, vertexType, incomingEdgeCount, outgoingEdgeCount, NULL, NULL, true /* destroyIfExists */, compactIfDestroy);
        assert(exists); // it doesn't exist any more!
    }
    inline void DestroyVertex(VertexID vertex, unsigned* incomingEdgeCount = NULL, unsigned* outgoingEdgeCount = NULL) {
        VertexType vertexType;
        return DestroyVertexEx(vertex, vertexType, true /* compactIfDestroy */, incomingEdgeCount, outgoingEdgeCount);
    }
    inline void DestroyVertexDontCompact(VertexID vertex, unsigned* incomingEdgeCount = NULL, unsigned* outgoingEdgeCount = NULL) {
        VertexType vertexType;
        return DestroyVertexEx(vertex, vertexType, false /* compactIfDestroy */, incomingEdgeCount, outgoingEdgeCount);
    }
    // presupposition: no incoming edges, only points to others (if any)
    void DestroySourceVertexEx(VertexID vertex, VertexType& vertexType, bool compactIfDestroy = true, unsigned* outgoingEdgeCount = NULL) {
        unsigned incomingEdgeCount;
        DestroyVertexEx(vertex, vertexType, compactIfDestroy, &incomingEdgeCount, outgoingEdgeCount);
        assert(incomingEdgeCount == 0);
    }
    inline void DestroySourceVertex(VertexID vertex, unsigned* outgoingEdgeCount = NULL) {
        VertexType vertexType;
        return DestroySourceVertexEx(vertex, vertexType, true /* compactIfDestroy */, outgoingEdgeCount);
    }
    inline void DestroySourceVertexDontCompact(VertexID vertex, unsigned* outgoingEdgeCount = NULL) {
        VertexType vertexType;
        return DestroySourceVertexEx(vertex, vertexType, false /* compactIfDestroy */, outgoingEdgeCount);
    }
    // presupposition: no outgoing edges, only incoming ones (if any)
    void DestroySinkVertexEx(VertexID vertex, VertexType& vertexType, bool compactIfDestroy = true, unsigned* incomingEdgeCount = NULL) {
        unsigned outgoingEdgeCount;
        DestroyVertexEx(vertex, vertexType, compactIfDestroy, incomingEdgeCount, &outgoingEdgeCount);
        assert(outgoingEdgeCount == 0);
    }
    inline void DestroySinkVertex(VertexID vertex, unsigned* incomingEdgeCount = NULL) {
        VertexType vertexType;
        return DestroySinkVertexEx(vertex, vertexType, true /* compactIfDestroy */, incomingEdgeCount);
    }
    inline void DestroySinkVertexDontCompact(VertexID vertex, unsigned* incomingEdgeCount = NULL) {
        VertexType vertexType;
        return DestroySinkVertexEx(vertex, vertexType, false /* compactIfDestroy */, incomingEdgeCount);
    }
    // presupposition: no incoming or outgoing edges
    void DestroyIsolatedVertexEx(VertexID vertex, VertexType& vertexType, bool compactIfDestroy = true) {
        unsigned incomingEdgeCount;
        unsigned outgoingEdgeCount;
        DestroyVertexEx(vertex, vertexType, compactIfDestroy, &incomingEdgeCount, &outgoingEdgeCount);
        assert(incomingEdgeCount == 0);
        assert(outgoingEdgeCount == 0);
    }
    inline void DestroyIsolatedVertex(VertexID vertex) {
        VertexType vertexType;
        DestroyIsolatedVertexEx(vertex, vertexType, true /* compactIfDestroy */ );
    }
    inline void DestroyIsolatedVertexDontCompact(VertexID vertex) {
        VertexType vertexType;
        DestroyIsolatedVertexEx(vertex, vertexType, true /* compactIfDestroy */ );
    }

    //
    // ITERATION ROUTINES
    //

    std::set<VertexID> OutgoingEdgesForVertex(VertexID vertex) const {
        bool exists;
        VertexType vertexType;
        std::set<VertexID> outgoingEdges;
        const_cast<OrientedGraph*>(this)->GetVertexInfoMaybeDestroy(vertex, exists, vertexType, NULL, NULL, NULL, &outgoingEdges);
        assert(exists);
        return outgoingEdges;
    }

    std::set<VertexID> IncomingEdgesForVertex(VertexID vertex) const {
        bool exists;
        VertexType vertexType;
        std::set<VertexID> incomingEdges;
        const_cast<OrientedGraph*>(this)->GetVertexInfoMaybeDestroy(vertex, exists, vertexType, NULL, NULL, &incomingEdges, NULL);
        assert(exists);
        return incomingEdges;
    }

public:
    bool HasLinkage(VertexID fromVertex, VertexID toVertex, bool* forwardEdge = NULL, bool* reverseEdge = NULL) const {
        assert(fromVertex != toVertex);
        assert(VertexExists(fromVertex));
        assert(VertexExists(toVertex));

        VertexID vertexL = fromVertex > toVertex ? fromVertex : toVertex;
        VertexID vertexS = fromVertex > toVertex ? toVertex : fromVertex;

        VertexConnectionTristate nct = static_cast<VertexConnectionTristate>(
            static_cast<unsigned int>(m_buffer[TristateIndexForConnection(vertexS, vertexL)]));

        if (nct != notConnected) {
            if (toVertex > fromVertex) {
                if (forwardEdge)
                    *forwardEdge = (nct == lowPointsToHigh);
                if (reverseEdge)
                    *reverseEdge = (nct == highPointsToLow);
            } else {
                if (forwardEdge)
                    *forwardEdge = (nct == highPointsToLow);
                if (reverseEdge)
                    *reverseEdge = (nct == lowPointsToHigh);
            }
            return true;
        }

        if (forwardEdge)
            *forwardEdge = false;
        if (reverseEdge)
            *reverseEdge = false;
        return false;
    }
    bool EdgeExists(VertexID fromVertex, VertexID toVertex) const {
        bool forwardEdge;
        if (HasLinkage(fromVertex, toVertex, &forwardEdge))
            return forwardEdge;
        return false;
    }

public:
    bool SetEdge(VertexID fromVertex, VertexID toVertex) {
        assert(fromVertex != toVertex);
        assert(VertexExists(fromVertex));
        assert(VertexExists(toVertex));

        VertexID vertexL = fromVertex > toVertex ? fromVertex : toVertex;
        VertexID vertexS = fromVertex > toVertex ? toVertex : fromVertex;

        size_t tifc = TristateIndexForConnection(vertexS, vertexL);
        if (toVertex > fromVertex) {
            switch (m_buffer[tifc]) {
              case lowPointsToHigh:
                return false;

              case notConnected:
                m_buffer[tifc] = lowPointsToHigh;
                return true;

              case highPointsToLow:
                assert(false);
                return false;

              default:
                assert(false);
                return false;
            }
        } else {
            switch (m_buffer[tifc]) {
              case highPointsToLow:
                return false;

              case notConnected:
                m_buffer[tifc] = highPointsToLow;
                return true;

              case lowPointsToHigh:
                assert(false);
                return false;

              default:
                assert(false);
                return false;
            }
        }
    }
    void AddEdge(VertexID fromVertex, VertexID toVertex) {
        if (!SetEdge(fromVertex, toVertex))
            assert(false);
    }
    bool ClearEdge(VertexID fromVertex, VertexID toVertex) {
        assert(fromVertex != toVertex);

        bool fromExists;
        VertexType fromVertexType;
        GetVertexInfo(fromVertex, fromExists, fromVertexType);
        assert(fromExists);
        bool toExists;
        VertexType toVertexType;
        GetVertexInfo(toVertex, toExists, toVertexType);
        assert(toExists);

        VertexID vertexL = fromVertex > toVertex ? fromVertex : toVertex;
        VertexID vertexS = fromVertex > toVertex ? toVertex : fromVertex;

        size_t tifc = TristateIndexForConnection(vertexS, vertexL);
        if (toVertex > fromVertex) {
            switch (m_buffer[tifc]) {
              case lowPointsToHigh:
                m_buffer[tifc] = notConnected;
                return true;

              case notConnected:
              case highPointsToLow:
                return false;

              default:
                assert(false);
                return false;
            }
        } else {
            switch (m_buffer[tifc]) {
              case highPointsToLow:
                m_buffer[tifc] = notConnected;
                return true;

              case notConnected:
              case lowPointsToHigh:
                return false;

              default:
                assert(false);
                return false;
            }
        }
    }
    void RemoveEdge(VertexID fromVertex, VertexID toVertex) {
        if (!ClearEdge(fromVertex, toVertex))
            assert(false);
    }

// Construction and destruction
public:
    OrientedGraph(const size_t initial_size) :
        m_buffer (0) // fills with zeros
    {
        SetCapacitySoVertexIsFirstInvalidID(initial_size);
    }

    virtual ~OrientedGraph() {
    }

  #if ORIENTEDGRAPH_SELFTEST
  public:
    static bool SelfTest();
  #endif
};

} // end namespace nocycle
