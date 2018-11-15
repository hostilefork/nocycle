//
//  RandomEdgePicker.hpp - Template class that can be applied to an
//     OrientedGraph, DirectedGraph, or boost wrapper in order to get
//     better performance at picking a random edge in a graph
//     implemented with an adjacency matrix than picking pairs
//     of nodes at random and hoping they will be connected.
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
#ifndef RANDOMEDGEPICKER_HPP
#define RANDOMEDGEPICKER_HPP

#include "NocycleConfig.hpp"

#include <cstdlib> // needed for rand()

namespace nocycle {

// I wanted to be able to quickly remove a random edge from the graph.  Unfortunately, in
// a dense matrix representation in which there can be many missing edges... the edges can
// be difficult to impose an enumeration on that can be chosen from a random index.
//
// This adds some tracking on top of the graph which keeps track of all the vertex IDs
// and how many outgoing connections they have.  Armed with this information, you can
// pick a link at random!
//
template<class Base>
class RandomEdgePicker : public Base {
  public:
    typedef typename Base::VertexID VertexID;

private:
    std::map<size_t, std::set<VertexID> > m_verticesByOutgoingEdgeCount;
    unsigned m_numEdges;

public:
    // NOTE: There are variations of these functions, need to mirror them all!
    void CreateVertex(VertexID vertex) {
        m_verticesByOutgoingEdgeCount[0].insert(vertex);
        Base::CreateVertex(vertex);
    }
    void DestroyVertex(VertexID vertex) {
        nocycle_assert(m_verticesByOutgoingEdgeCount[0].find(vertex) != m_verticesByOutgoingEdgeCount[0].end());
        m_verticesByOutgoingEdgeCount[0].erase(vertex);
        Base::DestroyVertex(vertex);
    }
    bool SetEdge(VertexID fromVertex, VertexID toVertex) {
        if (Base::SetEdge(fromVertex, toVertex)) {
            m_numEdges++;
            unsigned numOutgoing = Base::OutgoingEdgesForVertex(fromVertex).size();
            m_verticesByOutgoingEdgeCount[numOutgoing-1].erase(fromVertex);
            m_verticesByOutgoingEdgeCount[numOutgoing].insert(fromVertex);
            return true;
        }
        return false;
    }
    void AddEdge(VertexID fromVertex, VertexID toVertex) {
        if (!RandomEdgePicker::SetEdge(fromVertex, toVertex))
            nocycle_assert(false);
    }
    bool ClearEdge(VertexID fromVertex, VertexID toVertex) {
        if (Base::ClearEdge(fromVertex, toVertex)) {
            nocycle_assert(m_numEdges > 0);
            m_numEdges--;
            unsigned numOutgoing = Base::OutgoingEdgesForVertex(fromVertex).size();
            m_verticesByOutgoingEdgeCount[numOutgoing+1].erase(fromVertex);
            m_verticesByOutgoingEdgeCount[numOutgoing].insert(fromVertex);
            return true;
        }
        return false;
    }
    void RemoveEdge(VertexID fromVertex, VertexID toVertex) {
        if (!RandomEdgePicker::ClearEdge(fromVertex, toVertex))
            nocycle_assert(false);
    }

public:
    size_t NumEdges() const {
        return m_numEdges;
    }

    // The algorithm for picking a random edge is to start with a random number that ranges
    // from [0..numEdges-1], and then weight the probability of picking a link from a node
    // based on its outgoing number of connections.
    //
    // Imagine you have 100 nodes, and 100 edges between them:
    //    40 nodes with 0 outgoing edges
    //    30 nodes with 1 outgoing edge
    //    20 nodes with 2 outgoing edges
    //    10 nodes with 3 outgoing edges
    //
    // You pick a random number between [0..99].  You want to be three times as likely to
    // pick a node with 3 outgoing edges as you are to pick a node with 1 outgoing edges,
    // and twice as likely to pick a node with 2 outgoing edges as 1 outgoing.   So let's
    // say your random pick was 90.
    //
    // * Skip all the nodes with 0 outgoing edges, since they have no links to take
    // * 90 > 30*1, so skip past the nodes with one outgoing edge and subtract 30*1 (60)
    // * 60 > 20*2, so skip past the nodes with two outgoing edges and subtract 20*2 (20)
    // * 20 < 10*3... so we div 20 by 3 to realize we need to pick the 6th node
    //     with 3 outgoing edges... and mod 20 by 3 to realize we need to pick the 3rd
    //     edge of that sixth node (index 2)
    //
    // Really, it makes sense.  :)  Leaving it a little messy for the moment since it's
    // just used for testing.
    void GetRandomEdge(VertexID& fromVertex, VertexID& toVertex) const {
        nocycle_assert(m_numEdges > 0);
        if (0) { // test code, useful if you suspect the sets aren't accurate
            unsigned numEdgesCheck = 0;
            typename std::map<size_t, std::set<VertexID> >::const_iterator verticesByOutgoingEdgeCountCheckIter =
                m_verticesByOutgoingEdgeCount.begin();

            while (verticesByOutgoingEdgeCountCheckIter != m_verticesByOutgoingEdgeCount.end()) {
                numEdgesCheck += (*verticesByOutgoingEdgeCountCheckIter).first * (*verticesByOutgoingEdgeCountCheckIter).second.size();
                verticesByOutgoingEdgeCountCheckIter++;
            }
            nocycle_assert(numEdgesCheck == m_numEdges);
        }

        unsigned edgeIndexToRemove = rand() % m_numEdges;
        unsigned edgeIndex = edgeIndexToRemove;
        typename std::map<size_t, std::set<VertexID> >::const_iterator verticesByOutgoingEdgeCountIter =
            m_verticesByOutgoingEdgeCount.begin();

        unsigned numOutgoing = (*verticesByOutgoingEdgeCountIter).first;
        unsigned numEdgesWithThisOutgoingCount = (*verticesByOutgoingEdgeCountIter).second.size();

        while (edgeIndex >= numOutgoing * numEdgesWithThisOutgoingCount) {
            // the edge "index" we are looking for wouldn't be in the current set
            edgeIndex -= numOutgoing * numEdgesWithThisOutgoingCount;
            verticesByOutgoingEdgeCountIter++;
            numOutgoing = (*verticesByOutgoingEdgeCountIter).first;
            numEdgesWithThisOutgoingCount = (*verticesByOutgoingEdgeCountIter).second.size();
            nocycle_assert(verticesByOutgoingEdgeCountIter != m_verticesByOutgoingEdgeCount.end());
        }

        typename std::set<VertexID>::const_iterator setOfVerticesIter = (*verticesByOutgoingEdgeCountIter).second.begin();
        while (edgeIndex >= numOutgoing) {
            edgeIndex -= numOutgoing;
            setOfVerticesIter++;
            nocycle_assert(setOfVerticesIter != (*verticesByOutgoingEdgeCountIter).second.end());
        }

        fromVertex = *setOfVerticesIter;

        // Now we pick the edge from the outgoing set based on what's left of our index
        std::set<VertexID> outgoing = Base::OutgoingEdgesForVertex(fromVertex);
        nocycle_assert(outgoing.size() == numOutgoing);
        typename std::set<VertexID>::const_iterator outgoingIter = outgoing.begin();
        while (edgeIndex > 0) {
            edgeIndex--;
            outgoingIter++;
            nocycle_assert(outgoingIter != outgoing.end());
        }

        toVertex = *outgoingIter;

        nocycle_assert(verticesByOutgoingEdgeCountIter != m_verticesByOutgoingEdgeCount.end());
        nocycle_assert(numOutgoing > 0);
    }
    void GetRandomNonEdge(VertexID& fromVertex, VertexID& toVertex) const {
        VertexID maxID = Base::GetFirstInvalidVertexID();

        // for the moment, we assume the graph is not so dense that this will take
        // an inordinate amount of time, but we could in theory use a reverse
        // of the above approach
        do {
            do {
                fromVertex = rand() % maxID;
            } while (!Base::VertexExists(fromVertex));
            do {
                toVertex = rand() % maxID;
            } while ((fromVertex == toVertex) || (!Base::VertexExists(toVertex)));
        } while (Base::HasLinkage(fromVertex, toVertex));
    }

public:
    RandomEdgePicker (const size_t initial_size) :
        Base (initial_size),
        m_numEdges (0)
    {
    }
    virtual ~RandomEdgePicker() {
    }
};

} // end namespace nocycle

#endif
