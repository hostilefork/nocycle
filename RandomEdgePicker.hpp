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

#include "NocycleSettings.hpp"

// This class is a template of which base class you wish to apply it to.
// I wanted to be able to quickly remove a random edge from the graph.  Unfortunately, in
// a dense matrix representation in which there can be many missing edges... the edges can
// be difficult to count and index.
//
// This adds a set on top of the graph that keeps lists of how many connections of
// each outgoing vertex count exist.  Then with a
// probability weighted by the number of outgoing links you pick a node from the
// corresponding set.  e.g. a node with two outgoing links is twice as likely to
// have one of its outgoing links picked than a node with only one.
//
// Implementing this is simply a matter of picking a random number ranging 0...numLinks,
// and then skipping through the sets proportional to the # of links in that set.
// So if you have 20 nodes with 1 outgoing link and 10 nodes with 2 outgoing links,
// and you pick the number 30... you subtract 20 from 30 and then with 10 left over
// you go to the 5th node in the 2 link collection.  
template<class BaseClass>
class RandomEdgePicker : public BaseClass {
public:
	typedef typename BaseClass::VertexID VertexID;
	
private:
	std::map<size_t, std::set<VertexID> > m_verticesByOutgoingEdgeCount;
	unsigned m_numEdges;
		
public:
	// NOTE: There are variations of these functions, need to mirror them all!
	void CreateVertex(VertexID vertex) {
		m_verticesByOutgoingEdgeCount[0].insert(vertex);
		BaseClass::CreateVertex(vertex);
	}
	void DestroyVertex(VertexID vertex) {
		nocycle_assert(m_verticesByOutgoingEdgeCount[0].find(vertex) != m_verticesByOutgoingEdgeCount[0].end());
		m_verticesByOutgoingEdgeCount[0].erase(vertex);
		BaseClass::DestroyVertex(vertex);
	}
	bool SetEdge(VertexID fromVertex, VertexID toVertex) {
		if (BaseClass::SetEdge(fromVertex, toVertex)) {
			m_numEdges++;
			unsigned numOutgoing = OutgoingEdgesForVertex(fromVertex).size();
			m_verticesByOutgoingEdgeCount[numOutgoing-1].erase(fromVertex);
			m_verticesByOutgoingEdgeCount[numOutgoing].insert(fromVertex);
			return true;
		}
		return false;
	}
	void AddEdge(VertexID fromVertex, VertexID toVertex) {
		if (!SetEdge(fromVertex, toVertex))
			nocycle_assert(false);
	}
	bool ClearEdge(VertexID fromVertex, VertexID toVertex) {
		if (BaseClass::ClearEdge(fromVertex, toVertex)) {
			nocycle_assert(m_numEdges > 0);
			m_numEdges--;
			unsigned numOutgoing = OutgoingEdgesForVertex(fromVertex).size();
			m_verticesByOutgoingEdgeCount[numOutgoing+1].erase(fromVertex);
			m_verticesByOutgoingEdgeCount[numOutgoing].insert(fromVertex);
			return true;
		}
		return false;
	}
	void RemoveEdge(VertexID fromVertex, VertexID toVertex) {
		if (!ClearEdge(fromVertex, toVertex))
			nocycle_assert(false);
	}
	
public:
	size_t NumEdges() const {
		return m_numEdges;
	}
	void GetRandomEdge(VertexID& fromVertex, VertexID& toVertex) const {
		nocycle_assert(m_numEdges > 0);
		
		unsigned edgeToRemove = rand() % m_numEdges;
		unsigned edgeIndex = edgeToRemove;
		unsigned numOutgoing = 0;
		typename std::map<size_t, std::set<VertexID> >::const_iterator verticesByOutgoingEdgeCountIter = 
			m_verticesByOutgoingEdgeCount.begin();
		while (verticesByOutgoingEdgeCountIter != m_verticesByOutgoingEdgeCount.end()) {
			numOutgoing = (*verticesByOutgoingEdgeCountIter).first;
			if (edgeIndex < numOutgoing * ((*verticesByOutgoingEdgeCountIter).second.size())) {
				typename std::set<VertexID>::const_iterator setOfVerticesIter = (*verticesByOutgoingEdgeCountIter).second.begin();
				while (edgeIndex >= numOutgoing) {
					edgeIndex -= numOutgoing;
					setOfVerticesIter++;
					nocycle_assert(setOfVerticesIter != (*verticesByOutgoingEdgeCountIter).second.end());
				}
				fromVertex = *setOfVerticesIter;
				std::set<VertexID> outgoing = OutgoingEdgesForVertex(fromVertex);
				nocycle_assert(outgoing.size() == numOutgoing);
				typename std::set<VertexID>::const_iterator outgoingIter = outgoing.begin();
				while (edgeIndex > 0) {
					edgeIndex--;
					outgoingIter++;
					nocycle_assert(outgoingIter != outgoing.end());
				}
				toVertex = *outgoingIter;
				break;
			}
			edgeIndex -= numOutgoing * (*verticesByOutgoingEdgeCountIter).second.size();
			verticesByOutgoingEdgeCountIter++;
		}
				
		nocycle_assert(verticesByOutgoingEdgeCountIter != m_verticesByOutgoingEdgeCount.end());
		nocycle_assert(numOutgoing > 0);
	}
	void GetRandomNonEdge(VertexID& fromVertex, VertexID& toVertex) const {
		VertexID maxID = BaseClass::GetFirstInvalidVertexID();
		
		// for the moment, we assume the graph is not so dense that this will take
		// an inordinate amount of time, but we could in theory use a reverse
		// of the above approach
		do {
			do {
				fromVertex = rand() % maxID;
			} while (!VertexExists(fromVertex));
			do {
				toVertex = rand() % maxID;
			} while ((fromVertex == toVertex) || (!VertexExists(toVertex)));
		} while (HasLinkage(fromVertex, toVertex));
	}

public:
	RandomEdgePicker (const size_t initial_size) : 
		BaseClass (initial_size), 
		m_numEdges (0)
	{
	}
	virtual ~RandomEdgePicker() {
	}
};

#endif
