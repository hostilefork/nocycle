//
//  DirectedAcyclicGraph.hpp - Experimental DAG graph class with a
//      sidestructure of its transitive closure.  Implemented as two
//      OrientedGraph objects, and various enhancements to the 
//      sidestructure can be tried out.
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
#ifndef DIRECTEDACYCLICGRAPH_HPP
#define DIRECTEDACYCLICGRAPH_HPP

#include "NocycleSettings.hpp"

#include "OrientedGraph.hpp"

#include <set>
#include <stack>

namespace nocycle {

// 
// EXCEPTIONS
// See: http://www.cplusplus.com/doc/tutorial/exceptions.html
//
class bad_cycle : public std::exception {
	virtual const char* what() const throw() {
		return "Attempt to insert a cycle into a DirectedAcyclicGraph";
	}
};



// Each class that uses another in its implementation should insulate its 
// clients from that fact.  A lazy man's composition pattern can be achieved with
// private inheritance: http://www.parashift.com/c++-faq-lite/private-inheritance.html
//
// Public inheritance lets users coerce pointers from the derived class to the
// base class.  Without virtual methods, this is bad because it disregards the
// overriden methods.  As Nocycle is refined I will make decisions about this, but I
// use public inheritance for expedience while the library is still in flux.
class DirectedAcyclicGraph : public OrientedGraph {
public:
	typedef OrientedGraph::VertexID VertexID;
	
private:
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
	// Sidestructure for fast O(1) acyclic insertion
	//
	// IF no edge A->B or A<-B is in the main data, then m_canreach is the
	// transitive closure relationship.  e.g.:
	//
	// * no edge between A and B indicates any edge is permitted.  e.g.,
	//   (B cantreach A) and (A cantreach B)
	//
	// * A->B indicates that (A canreach B) and (B cantreach A)
	//   hence edges from A->B are ok, but B->A would create a cycle
	//
	// * A<-B indicates that (A cantreach B) and (B canreach A)
	//   hence edges from B->A are ok, but A->B would create a cycle
	//
	// IF there is a edge in the main data, then the canreach data for 
	// that edge is somewhat redundant, and it is possible to derive
	// an independent tristate for each such connection.  It may be
	// possible to use that information to enhance the behavior of
	// the transitive closure sidestructure, so there's some experiments
	// to that effect...such as caching whether the pointed to vertex
	// would be reachable if the physical edge were removed.
	OrientedGraph m_canreach;
#endif
public:
	DirectedAcyclicGraph(const size_t initial_size) : 
		OrientedGraph(initial_size)
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
		, m_canreach (initial_size)
#endif
	{
	}

	virtual ~DirectedAcyclicGraph() {
	}

	// Special features of our DAG's sidestructure
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
	// If a physical connection exists between fromVertex and toVertex, we can use its
	// reachability data for other purposes...effectively, a tristate!
	// public for testing, but will probably go private
#if DIRECTEDACYCLICGRAPH_CACHE_REACH_WITHOUT_LINK
private:
	enum ExtraTristate {
		isReachableWithoutEdge = 0,
		notReachableWithoutEdge = 1,
		thirdStateNotSureWhatToDoWithIt = 2
	};
#endif

#if DIRECTEDACYCLICGRAPH_USER_TRISTATE
public:
#else
private:
#endif
	Nstate<3> GetTristateForConnection(VertexID fromVertex, VertexID toVertex) const {
		nocycle_assert(EdgeExists(fromVertex, toVertex));

		bool forwardEdge, reverseEdge;
		if (m_canreach.HasLinkage(fromVertex, toVertex, &forwardEdge, &reverseEdge)) {
			if (forwardEdge)
				return 1;
		
			if (reverseEdge)
				return 2;
				
			nocycle_assert(false);
		}
			
		return 0;
	}
	void SetTristateForConnection(VertexID fromVertex, VertexID toVertex, Nstate<3> tristate) {
		nocycle_assert(EdgeExists(fromVertex, toVertex));
		
		bool forwardEdge, reverseEdge;
		m_canreach.HasLinkage(fromVertex, toVertex, &forwardEdge, &reverseEdge);

		switch (tristate) {
			case 0: {
				if (forwardEdge)
					m_canreach.RemoveEdge(fromVertex, toVertex);
				if (reverseEdge)
					m_canreach.RemoveEdge(toVertex, fromVertex);
				break;
			}
			case 1: {
				if (reverseEdge)
					m_canreach.RemoveEdge(toVertex, fromVertex);
				m_canreach.SetEdge(fromVertex, toVertex);
				break;
			}
			case 2: {
				if (forwardEdge)
					m_canreach.RemoveEdge(fromVertex, toVertex);
				m_canreach.SetEdge(toVertex, fromVertex);
				break;
			}
			default:
				nocycle_assert(false);
		}
	}

private:
	// For each vertex in the canreach data, we say its reachability is either "clean"
	// or "dirty".  Current strategy is that dirty reachability has false positives but 
	// no false negatives.
#define canreachClean vertexTypeOne
#define canreachMayHaveFalsePositives vertexTypeTwo
	bool ClearReachEdge(VertexID fromVertex, VertexID toVertex) {
		// don't want to damage a tristate!
		nocycle_assert(!HasLinkage(fromVertex, toVertex));
		return m_canreach.ClearEdge(fromVertex, toVertex);
	}
	void RemoveReachEdge(VertexID fromVertex, VertexID toVertex) {
		if (!ClearReachEdge(fromVertex, toVertex))
			assert (false);
	}
	
	bool SetReachEdge(VertexID fromVertex, VertexID toVertex) {
		// don't want to damage a tristate!
		nocycle_assert(!HasLinkage(fromVertex, toVertex));
		return m_canreach.SetEdge(fromVertex, toVertex);
	}
	void AddReachEdge(VertexID fromVertex, VertexID toVertex) {
		if (!SetReachEdge(fromVertex, toVertex))
			assert (false);
	}
	
	// The "incoming reach" is all incoming edges, and for all non-incoming edges that
	// are also not outgoing edges... the data contained in the canreach graph.
	// (Note: includes vertex, as being able to "reach" itself)
	std::set<VertexID> IncomingReachForVertexIncludingSelf(VertexID vertex) {
		std::set<VertexID> incoming = IncomingEdgesForVertex(vertex);
		std::set<VertexID> incomingReach = m_canreach.IncomingEdgesForVertex(vertex);
		std::set<VertexID>::iterator incomingReachIter = incomingReach.begin();
		while (incomingReachIter != incomingReach.end()) {
			VertexID incomingReachVertex = *incomingReachIter;
			if (!HasLinkage(vertex, incomingReachVertex))
				incoming.insert(incomingReachVertex);
			incomingReachIter++;
		}
		incoming.insert(vertex);
		return incoming;
	}
	
	// The "outgoing reach" is all outgoing edges, and for all non-outgoing edges that
	// are also not incoming edges... the data contained in the canreach graph.
	// (Note: includes vertex, as being able to "reach" itself)
	std::set<VertexID> OutgoingReachForVertexIncludingSelf(VertexID vertex) {
		std::set<VertexID> outgoing = OutgoingEdgesForVertex(vertex);
		std::set<VertexID> outgoingReach = m_canreach.OutgoingEdgesForVertex(vertex);
		std::set<VertexID>::iterator outgoingReachIter = outgoingReach.begin();
		while (outgoingReachIter != outgoingReach.end()) {
			VertexID outgoingReachVertex = *outgoingReachIter;
			if (!HasLinkage(outgoingReachVertex, vertex))
				outgoing.insert(outgoingReachVertex);
			outgoingReachIter++;
		}
		outgoing.insert(vertex);
		return outgoing;
	}
	
	// cleans as much reachability data as needed to answer if from can reach
	// to.  In theory, can leave the vertex dirty if answer is NO...
	void CleanUpReachability(VertexID fromVertex, VertexID toVertex) {
		// fromVertex's canreach data is dirty.  that means that some of the outgoing
		// edges may be false positives.  Start by clearing all outgoing reachability
		// edges...but remember which ones since those are the only ones that need
		// fixing
		std::set<VertexID> outgoingReachAndNoise = m_canreach.OutgoingEdgesForVertex(fromVertex);
		std::set<VertexID>::iterator outgoingReachAndNoiseIter = outgoingReachAndNoise.begin();
		std::set<VertexID> outgoingReachBeforeClean;
		while (outgoingReachAndNoiseIter != outgoingReachAndNoise.end()) {
			VertexID outgoingReachAndNoiseVertex = (*outgoingReachAndNoiseIter++);
			if (HasLinkage(fromVertex, outgoingReachAndNoiseVertex)) {
				// noise, ignore it
			} else {
				outgoingReachBeforeClean.insert(outgoingReachAndNoiseVertex);
				// we don't really need to modify the edge here...
				// in fact, it causes us to break the "false positives" invariant
				// (hence why I'm saving the outgoingReachBeforeClean)
				RemoveReachEdge(fromVertex, outgoingReachAndNoiseVertex);
			}
		}
		
		// now go through all the vertices to which there is a physical connection
		// if their canreach data is good, then use it
		// otherwise, clean it and use it
		// (there will be no loops because it's acyclic)
		std::set<VertexID> outgoing = OutgoingEdgesForVertex(fromVertex);
		std::set<VertexID>::iterator outgoingIter = outgoing.begin();
#if DIRECTEDACYCLICGRAPH_CACHE_REACH_WITHOUT_LINK
		std::map<VertexID, std::set<VertexID> > mapOfOutgoingReachIncludingSelf;
#endif
		while (outgoingIter != outgoing.end()) {
			OrientedGraph::VertexID outgoingVertex = (*outgoingIter++);
			
			if (m_canreach.GetVertexType(outgoingVertex) == canreachMayHaveFalsePositives)
				CleanUpReachability(outgoingVertex, toVertex);
			
			std::set<VertexID> outgoingForOutgoing = OutgoingReachForVertexIncludingSelf(outgoingVertex);
			
#if DIRECTEDACYCLICGRAPH_CACHE_REACH_WITHOUT_LINK
			mapOfOutgoingReachIncludingSelf[outgoingVertex] = outgoingForOutgoing;
#endif
			std::set<VertexID>::iterator outgoingForOutgoingIter = outgoingForOutgoing.begin();
			while (outgoingForOutgoingIter != outgoingForOutgoing.end()) {
				VertexID outgoingForOutgoingVertex = (*outgoingForOutgoingIter++);
				if (outgoingVertex == outgoingForOutgoingVertex)
					continue;
					
				if (!EdgeExists(fromVertex, outgoingForOutgoingVertex)) {
					if (m_canreach.EdgeExists(outgoingForOutgoingVertex, fromVertex)) {
						VertexType vertexTypeOutgoingForOutgoing = m_canreach.GetVertexType(outgoingForOutgoingVertex);
						nocycle_assert(vertexTypeOutgoingForOutgoing == canreachMayHaveFalsePositives);
						RemoveReachEdge(outgoingForOutgoingVertex, fromVertex);
					}
					SetReachEdge(fromVertex, outgoingForOutgoingVertex);
				}
			}
		}
		
#if DIRECTEDACYCLICGRAPH_CACHE_REACH_WITHOUT_LINK
		std::map<VertexID, std::set<VertexID> >::iterator mapOfOutgoingReachIncludingSelfIter = mapOfOutgoingReachIncludingSelf.begin();
		while (mapOfOutgoingReachIncludingSelfIter != mapOfOutgoingReachIncludingSelf.end()) {
			VertexID linkedVertex = (*mapOfOutgoingReachIncludingSelfIter).first;
			if (GetTristateForConnection(fromVertex, linkedVertex) == isReachableWithoutEdge) {
				bool foundOtherPath = false;
				std::map<VertexID, std::set<VertexID> >::iterator mapOfOutgoingReachIncludingSelfOtherIter = mapOfOutgoingReachIncludingSelf.begin();
				while (!foundOtherPath && (mapOfOutgoingReachIncludingSelfOtherIter != mapOfOutgoingReachIncludingSelf.end())) {
					if (mapOfOutgoingReachIncludingSelfIter != mapOfOutgoingReachIncludingSelfOtherIter) {
						std::set<VertexID> outgoingOtherReachIncludingOther = (*mapOfOutgoingReachIncludingSelfOtherIter).second;
						if (outgoingOtherReachIncludingOther.find(linkedVertex) !=outgoingOtherReachIncludingOther.end())
							foundOtherPath = true;
					}
					mapOfOutgoingReachIncludingSelfOtherIter++;
				}
				
				if (!foundOtherPath)
					SetTristateForConnection(fromVertex, linkedVertex, notReachableWithoutEdge);
			}

			mapOfOutgoingReachIncludingSelfIter++;
		}
#endif 
		
		m_canreach.SetVertexType(fromVertex, canreachClean);
	}
#endif
	
public:
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
	bool CanReach(VertexID fromVertex, VertexID toVertex) {

		// If there is a physical edge, then we are using the canreach data for other purposes
		bool forwardEdge, reverseEdge;
		if (HasLinkage(fromVertex, toVertex, &forwardEdge, &reverseEdge)) {
		
			// If a physical edge exists to the target vertex, we can reach it
			if (forwardEdge)
				return true;
			
			// If a physical edge exists from the target to us, then if we could reach
			// it then that would cause a cycle
			if (reverseEdge)
				return false;
				
			nocycle_assert(false);
			return false;
		}
		
		// When there's no physical edge, the canreach data is the transitive closure, except
		// for "dirtiness"
		
		switch (m_canreach.GetVertexType(fromVertex)) {
			case canreachClean: {
				return m_canreach.EdgeExists(fromVertex, toVertex);
			}
				
			case canreachMayHaveFalsePositives: {
				if (!m_canreach.EdgeExists(fromVertex, toVertex))
					return false;
				CleanUpReachability(fromVertex, toVertex);
				return m_canreach.EdgeExists(fromVertex, toVertex);
			}
				
			default:
				nocycle_assert(false);
				return false;
		};
		
		nocycle_assert(false);
		return false;		
	}
#else
	bool CanReach(VertexID fromVertex, VertexID toVertex) {
		// Simply do a depth-first search to determine reachability
		// Using LIFO stack instead of recursion...
		assert(fromVertex != toVertex);
		
		std::set<VertexID> visitedVertices;
		std::stack<VertexID> searchStack;
		searchStack.push(fromVertex);
		
		while (!searchStack.empty()) {
			VertexID searchVertex = searchStack.top();
			searchStack.pop();
			
			std::set<VertexID> outgoing = OutgoingEdgesForVertex(searchVertex);
			std::set<VertexID>::iterator outgoingIter = outgoing.begin();
			while (outgoingIter != outgoing.end()) {
				VertexID outgoingVertex = (*outgoingIter++);
				if (outgoingVertex == toVertex)
					return true;
				if (visitedVertices.find(outgoingVertex) != visitedVertices.end())
					continue;
				visitedVertices.insert(outgoingVertex);
				searchStack.push(outgoingVertex);
			}
		}
		
		return false;
	}
#endif

	
public:
	// This expands the buffer vector so that it can accommodate the existence and
	// connection data for vertexL.  Any new vertices added will not exist yet and not
	// have connection data.  Any vertices existing above this ID # will 
	void SetCapacityForMaxValidVertexID(VertexID vertexL) {
		OrientedGraph::SetCapacityForMaxValidVertexID(vertexL);
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
		m_canreach.SetCapacityForMaxValidVertexID(vertexL);
#endif
	}
	void SetCapacitySoVertexIsFirstInvalidID(VertexID vertexL) {
		OrientedGraph::SetCapacitySoVertexIsFirstInvalidID(vertexL);
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
		m_canreach.SetCapacityForMaxValidVertexID(vertexL);
#endif
	}
	void GrowCapacityForMaxValidVertexID(VertexID vertexL) {
		OrientedGraph::GrowCapacityForMaxValidVertexID(vertexL);
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
		m_canreach.SetCapacityForMaxValidVertexID(vertexL);
#endif
	}
	void ShrinkCapacitySoVertexIsFirstInvalidID(VertexID vertexL) {
		OrientedGraph::ShrinkCapacitySoVertexIsFirstInvalidID(vertexL);
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
		m_canreach.ShrinkCapacitySoVertexIsFirstInvalidID(vertexL);
#endif
	}

	//
	// CREATION OVERRIDES
	//
public:
	void CreateVertexEx(VertexID vertexE, VertexType vertexType) {
		OrientedGraph::CreateVertexEx(vertexE, vertexType);
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
		m_canreach.CreateVertexEx(vertexE, canreachClean);
#endif
	}
	inline void CreateVertex(VertexID vertexE) {
		return CreateVertexEx(vertexE, vertexTypeOne);
	}
	
	//
	// DESTRUCTION OVERRIDES
	//
public:
	inline void DestroyVertexEx(VertexID vertex, VertexType& vertexType, bool compactIfDestroy = true, unsigned* incomingEdgeCount = NULL, unsigned* outgoingEdgeCount = NULL ) {
		OrientedGraph::DestroyVertexEx(vertex, vertexType, compactIfDestroy, incomingEdgeCount, outgoingEdgeCount);
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
		unsigned incomingEdgeCanreach;
		unsigned outgoingEdgeCanreach;
		VertexType vertexTypeCanreach;
		m_canreach.DestroyVertexEx(vertex, vertexTypeCanreach, compactIfDestroy, &incomingEdgeCanreach, &outgoingEdgeCanreach);
#endif
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
		nocycle_assert(incomingEdgeCount == 0);
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
		nocycle_assert(outgoingEdgeCount == 0);
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
		nocycle_assert(incomingEdgeCount == 0);
		nocycle_assert(outgoingEdgeCount == 0);
	}
	inline void DestroyIsolatedVertex(VertexID vertex) {
		VertexType vertexType;
		DestroyIsolatedVertexEx(vertex, vertexType, true /* compactIfDestroy */ );
	}
	inline void DestroyIsolatedVertexDontCompact(VertexID vertex) {
		VertexType vertexType;
		DestroyIsolatedVertexEx(vertex, vertexType, true /* compactIfDestroy */ );
	}
	
public:
	// SetEdge throws exceptions on cycle.  To avoid having to write exception handling,
	// use this routine before calling SetEdge.
	inline bool InsertionWouldCauseCycle(VertexID fromVertex, VertexID toVertex) {
		return CanReach(toVertex, fromVertex);
	}
	
	bool SetEdge(VertexID fromVertex, VertexID toVertex) {
#if DIRECTEDACYCLICGRAPH_CONSISTENCY_CHECK
		ConsistencyCheck cc (*this);
#endif
		
		if (InsertionWouldCauseCycle(fromVertex, toVertex)) {
			bad_cycle bc;
			throw bc;
		}

#if DIRECTEDACYCLICGRAPH_CACHE_REACH_WITHOUT_LINK
		// this may have false positives, for the moment let's union the "false positive tristate"
		// with the rest of the "false positive" reachability data...
		bool reachablePriorToEdge = m_canreach.EdgeExists(fromVertex, toVertex);
#endif

		// set the physical edge in the data structure
		bool edgeIsNew = OrientedGraph::SetEdge(fromVertex, toVertex);
		if (!edgeIsNew)
			return false;

#if DIRECTEDACYCLICGRAPH_CACHE_REACH_WITHOUT_LINK		
		// save whether the toVertex was reachable prior to the physical connection in the
		// extra tristate for this edge
		if (reachablePriorToEdge) {
			SetTristateForConnection(fromVertex, toVertex, isReachableWithoutEdge);
		} else {
			SetTristateForConnection(fromVertex, toVertex, notReachableWithoutEdge);
		}
#endif
	
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
		// All the vertices that toVertex "canreach", including itself
		// (Note: may contain false positives if vertexTypeTo == canreachMayHaveFalsePositives)
		std::set<OrientedGraph::VertexID> toCanreach = OutgoingReachForVertexIncludingSelf(toVertex);
		
		VertexType vertexTypeTo = m_canreach.GetVertexType(toVertex);
		
		// All the vertices that "canreach" fromVertex, including itself
		// (Note: may contain false positives if the incoming vertices are of type canreachMayHaveFalsePositives)
		// (Note2: contains "lies"... if any of these vertices have physical edges, you'll be missing
		std::set<OrientedGraph::VertexID> canreachFrom = IncomingReachForVertexIncludingSelf(fromVertex);
		
		VertexType vertexTypeFrom = m_canreach.GetVertexType(fromVertex);
		
		// Now update the reachability.
			
		// All the vertices that canreach fromVertex (including fromVertex!) can now 
		// reach toVertex, as well as anything toVertex can reach...worst case O(N^2) "operations" but they
		// are fast operations.  Dirtiness needs to propagate, too.
				
		std::set<OrientedGraph::VertexID>::iterator iterCanreachFrom = canreachFrom.begin();
		while (iterCanreachFrom != canreachFrom.end()) {

			VertexID canreachFromVertex = (*iterCanreachFrom++);

#if DIRECTEDACYCLICGRAPH_CACHE_REACH_WITHOUT_LINK
			// These new reachabilities may lead us to need to bump a physical connection on
			// vertices that can reach fromVertex regarding how it can reach a vertex that toVertex canreach.
			// This would be from notReachableWithoutEdge to isReachableWithoutEdge.  Basically,
			// if some vertex to which you are getting a reachability relationship is able to reach
			// any of the vertices to which you are physically edgeed to, then you must bump that edge
			// if it was notReachableWithoutEdge

			std::set<VertexID> outgoing = OutgoingEdgesForVertex(canreachFromVertex);
			std::set<VertexID>::iterator outgoingIter = outgoing.begin();
			while (outgoingIter != outgoing.end()) {
				VertexID outgoingVertex = *outgoingIter++;
				if ((outgoingVertex == toVertex) && (canreachFromVertex == fromVertex))
					continue;
			/*	if (GetTristateForConnection(canreachFromVertex, outgoingVertex) == notReachableWithoutEdge) {*/
					if (toCanreach.find(outgoingVertex) != toCanreach.end()) {
						SetTristateForConnection(canreachFromVertex, outgoingVertex, isReachableWithoutEdge);
						if (vertexTypeTo == canreachMayHaveFalsePositives)
							SetVertexType(canreachFromVertex, canreachMayHaveFalsePositives);
					}
			/*	}*/
			}
#endif
			
			std::set<OrientedGraph::VertexID>::iterator iterToCanreach = toCanreach.begin();
			while(iterToCanreach != toCanreach.end()) {
			
				VertexID toCanreachVertex = (*iterToCanreach++);
				nocycle_assert(canreachFromVertex != toCanreachVertex);

				bool forwardEdge, reverseEdge;
				HasLinkage(canreachFromVertex, toCanreachVertex, &forwardEdge, &reverseEdge);
				if (forwardEdge) {
					// one case is that there's a physical edge and this is just a tristate
					// that edge would have to be from canreachFromVertex to toCanreachVertex
					// leave it alone
				} else {
					VertexType vertexTypeToCanreach = m_canreach.GetVertexType(toCanreachVertex);
					if (vertexTypeToCanreach == canreachMayHaveFalsePositives) {
						// there might be stale false positives which could have been
						// left behind, and we need to tolerate those.
						ClearReachEdge(toCanreachVertex, canreachFromVertex);
					} else {
						// if it were actually true that a vertex that toVertex canreach could reach
						// a vertex that canreach fromVertex, then a connection from toVertex to fromVertex
						// would be a cycle.
						nocycle_assert(!m_canreach.EdgeExists(toCanreachVertex, canreachFromVertex));
					}
								
					if (reverseEdge) {
						nocycle_assert(vertexTypeToCanreach == canreachMayHaveFalsePositives);
						// we know for a fact here now that canreachFromVertex *can't* reach toCanreachVertex
						// so ignore this and don't propagate it...
					} else {
						VertexType vertexTypeCanreachFrom = m_canreach.GetVertexType(canreachFromVertex);
						if ((vertexTypeCanreachFrom == canreachClean) && (vertexTypeTo == canreachClean) && (vertexTypeFrom == canreachClean))
							m_canreach.SetVertexType(canreachFromVertex,  canreachClean);
						else
							m_canreach.SetVertexType(canreachFromVertex, canreachMayHaveFalsePositives);
						SetReachEdge(canreachFromVertex, toCanreachVertex);
					}
				}				
			}
		}
#endif
	
		return true;
	}
	void AddEdge(VertexID fromVertex, VertexID toVertex) {
		if (!SetEdge(fromVertex, toVertex))
			nocycle_assert(false);
	}
	
	bool ClearEdge(VertexID fromVertex, VertexID toVertex) {
#if DIRECTEDACYCLICGRAPH_CONSISTENCY_CHECK
		ConsistencyCheck cc (*this);
#endif

#if DIRECTEDACYCLICGRAPH_CACHE_REACH_WITHOUT_LINK
		if (!EdgeExists(fromVertex, toVertex))
			return false;

		ExtraTristate extra = static_cast<ExtraTristate>(static_cast<unsigned char>(GetTristateForConnection(fromVertex, toVertex)));
		SetTristateForConnection(fromVertex, toVertex, 0); // clear out tristate
		
		OrientedGraph::RemoveEdge(fromVertex, toVertex);
		
		// we have a short cut.  if we are removing a edge, and our invalidation data does not have
		// "false positives"... then if the vertex tristate said we were connected prior to the edge 
		// we still will be after remove it.  We do not need to mark anything as having false
		// positives.
		VertexType vertexTypeFrom = m_canreach.GetVertexType(fromVertex);
		if ((vertexTypeFrom == canreachClean) && (extra == isReachableWithoutEdge)) {
			m_canreach.AddEdge(fromVertex, toVertex);
			return true;
		}
#else
		if (!OrientedGraph::ClearEdge(fromVertex, toVertex))
			return false;
#endif

#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
		// removing a edge calls into question all of our canreach vertices, and all the canreach vertices of vertices that
		// canreach us... however, anything downstream of us is guaranteed to not have its reachability affected
		// due to the lack of cyclic relationships.  e.g. anything that we "canreach" is excluded from concern
		// of recalculation of its canreach properties
		
		// because the update can be somewhat costly, we merely dirty ourselves and all the vertices we canreach
		// and let a background process take care of the cleaning.  this does not affect readers, only writers,
		// and insertions on disconnected regions of the graph will not affect each other
		
		// All the vertices that canreach fromVertex...these have their reachability data coming into question
		// (Note: we may be dirtying more than we need to due to "false positives" in the reachability)
		std::set<OrientedGraph::VertexID> canreachFrom = IncomingReachForVertexIncludingSelf(fromVertex);
		
		std::set<OrientedGraph::VertexID>::iterator canreachFromIter = canreachFrom.begin();
		while(canreachFromIter != canreachFrom.end()) {
			OrientedGraph::VertexID canreachFromVertex = (*canreachFromIter);
			m_canreach.SetVertexType(canreachFromVertex, canreachMayHaveFalsePositives);
			canreachFromIter++;
		}
		
		// if there was a tristate associated with this edge, we're losing it
		// allow for false positives...there may have been a transitive edge!
		if (m_canreach.EdgeExists(toVertex, fromVertex))
			m_canreach.RemoveEdge(toVertex, fromVertex);
		m_canreach.SetEdge(fromVertex, toVertex);
#endif
		return true;
	}
	void RemoveEdge(VertexID fromVertex, VertexID toVertex) {
		if (!ClearEdge(fromVertex, toVertex))
			nocycle_assert(false);
	}
					
					
	// 
	// DEBUGGING ROUTINES
	//
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY
public:
	std::set<VertexID> OutgoingTransitiveVertices(VertexID vertex, const VertexID* vertexIgnoreEdge, bool includeDirectEdges) {
		std::set<VertexID> result;
		std::stack<VertexID> vertexStack;

		std::set<VertexID> outgoingInit = OutgoingEdgesForVertex(vertex);
		std::set<VertexID>::iterator outgoingInitIter = outgoingInit.begin();
		while (outgoingInitIter != outgoingInit.end()) {
			VertexID outgoingVertex = (*outgoingInitIter++);
			if (!vertexIgnoreEdge || (*vertexIgnoreEdge != outgoingVertex)) {
				vertexStack.push(outgoingVertex);
				if (includeDirectEdges)
					result.insert(outgoingVertex);
			}
		}
		
		while (!vertexStack.empty()) {
			VertexID vertexCurrent = vertexStack.top();
			vertexStack.pop();
					
			std::set<VertexID> outgoing = OutgoingEdgesForVertex(vertexCurrent);
			std::set<VertexID>::iterator outgoingIter = outgoing.begin();
			while (outgoingIter != outgoing.end()) {
				VertexID outgoingVertex = (*outgoingIter++);
				if (result.find(outgoingVertex) == result.end()) {
					vertexStack.push(outgoingVertex);
					result.insert(outgoingVertex);
				}
			}
			
		}
					
		return result;
	}
	std::set<VertexID> OutgoingTransitiveVerticesNotDirectlyEdged(VertexID vertex) {
		return OutgoingTransitiveVertices(vertex, NULL, false);
	}
	
	bool IsInternallyConsistent() {
		for (OrientedGraph::VertexID vertex = 0; vertex < GetFirstInvalidVertexID(); vertex++) {
			if (!VertexExists(vertex))
				continue;
				
			std::set<VertexID> outgoingReach = OutgoingReachForVertexIncludingSelf(vertex);
			std::set<VertexID> outgoingTransitive = OutgoingTransitiveVerticesNotDirectlyEdged(vertex);
			std::set<VertexID> outgoingTransitiveClosure = outgoingTransitive;
			std::set<VertexID> outgoing = OutgoingEdgesForVertex(vertex);
			outgoingTransitiveClosure.insert(outgoing.begin(), outgoing.end());
			outgoingTransitiveClosure.insert(vertex);
			
			switch (m_canreach.GetVertexType(vertex)) {
				case canreachClean: {
					size_t outgoingReachSize = outgoingReach.size();
					size_t outgoingTransitiveClosureSize = outgoingTransitiveClosure.size();
						
					std::set<VertexID>::iterator outgoingTransitiveClosureIter = outgoingTransitiveClosure.begin();
					while (outgoingTransitiveClosureIter != outgoingTransitiveClosure.end()) {
						VertexID outgoingTransitiveClosureVertex = (*outgoingTransitiveClosureIter++);
						if (outgoingReach.find(outgoingTransitiveClosureVertex) == outgoingReach.end())
							return false;
					}
					nocycle_assert(outgoingReach.size() == outgoingTransitiveClosure.size());
					
#if DIRECTEDACYCLICGRAPH_CACHE_REACH_WITHOUT_LINK
					std::set<VertexID>::iterator outgoingIter = outgoing.begin();
					while (outgoingIter != outgoing.end()) {
						VertexID outgoingVertex = (*outgoingIter++);
						std::set<VertexID> outgoingTransitiveWithoutEdge = OutgoingTransitiveVertices(vertex, &outgoingVertex, false);
						ExtraTristate extra = static_cast<ExtraTristate>(static_cast<unsigned char>(GetTristateForConnection(vertex, outgoingVertex)));
						switch (extra) {
						case isReachableWithoutEdge: {
							if (outgoingTransitiveWithoutEdge.find(outgoingVertex) == outgoingTransitiveWithoutEdge.end())
								return false;
							break;
						}
						case notReachableWithoutEdge: {
							if (outgoingTransitiveWithoutEdge.find(outgoingVertex) != outgoingTransitiveWithoutEdge.end())
								return false;
							break;
						}
						default:
							nocycle_assert(false);
						}
					}
#endif
					break;
				}
				case canreachMayHaveFalsePositives: {
					std::set<VertexID>::iterator outgoingTransitiveClosureIter = outgoingTransitiveClosure.begin();
					while (outgoingTransitiveClosureIter != outgoingTransitiveClosure.end()) {
						VertexID outgoingTransitiveClosureVertex = (*outgoingTransitiveClosureIter++);
						if (outgoingReach.find(outgoingTransitiveClosureVertex) == outgoingReach.end())
							return false;
					}
					nocycle_assert(outgoingReach.size() >= outgoingTransitiveClosure.size());
					break;
				}
				default:
					nocycle_assert(false);
			}
		}
		
		return true;
	}

	class ConsistencyCheck {
	private:
		DirectedAcyclicGraph& m_dag;
	public:
		ConsistencyCheck(DirectedAcyclicGraph& dag) : m_dag (dag) { };
		virtual ~ConsistencyCheck() {
			nocycle_assert(m_dag.IsInternallyConsistent());
		}
	};
#endif

public:
	static bool SelfTest();
};

}

#endif