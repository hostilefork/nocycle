//
//  DirectedAcyclicGraph.cpp - Experimental DAG graph class with a
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

#include "DirectedAcyclicGraph.hpp"

#if DIRECTEDACYCLICGRAPH_SELFTEST

#include <iostream>
#include "BoostImplementation.hpp"
#include "RandomEdgePicker.hpp"

const unsigned NUM_TEST_NODES = 128;
const float REMOVE_PROBABILITY = 0.0/8.0; // zero for now

namespace nocycle {

bool DirectedAcyclicGraph::SelfTest() {

	// Here are some simple test cases that can reveal basic breaks and are
	// easier to debug than random testing.
	
	if (true) { // Direct cycle
		DirectedAcyclicGraph dag(2);
		
		dag.CreateVertex(0);
		dag.CreateVertex(1);
		
		dag.SetEdge(0, 1);
		try {
			dag.SetEdge(1, 0);
			std::cout << "FAILURE: Did not catch direct cycle." << std::endl;
			return false;
		} catch (bad_cycle& e) {
		}
	}
	
	if (true) { // Simple transitive cycle
		DirectedAcyclicGraph dag(3);
		
		dag.CreateVertex(0);
		dag.CreateVertex(1);
		dag.CreateVertex(2);

		dag.SetEdge(0, 1);
		dag.SetEdge(1, 2);
		try {
			dag.SetEdge(2, 0);
			std::cout << "FAILURE: Did not catch simple transitive cycle." << std::endl;
			return false;
		} catch (bad_cycle& e) {
		}
	}

	if (true) { // Simple case of removing edge that would have caused a transitive cycle
		DirectedAcyclicGraph dag(3);
		
		dag.CreateVertex(0);
		dag.CreateVertex(1);
		dag.CreateVertex(2);
		
		dag.SetEdge(0, 1);
		dag.SetEdge(1, 2);
		dag.RemoveEdge(1, 2);
		try {
			dag.SetEdge(2, 0);
		} catch (bad_cycle& e) {
			std::cout << "FAILURE: Deletion of simple transitive cycle edge still threw cycle exception." << std::endl;
			return false;
		}
	}

	if (true) { // Drew this out on paper as a first test, keeping it

		DirectedAcyclicGraph dag(5);
		
		dag.CreateVertex(0);
		dag.CreateVertex(1);
		dag.CreateVertex(2);
		dag.CreateVertex(3);
		dag.CreateVertex(4);

		dag.SetEdge(0, 2);
		dag.SetEdge(1, 2);
		dag.SetEdge(1, 3);
		dag.SetEdge(2, 3);
		dag.SetEdge(4, 0);
		dag.SetEdge(4, 3);
		try {
			dag.SetEdge(2, 4);
			std::cout << "FAILURE: Did not catch random case I drew on a sheet of paper." << std::endl;
			return false;
		} catch (bad_cycle& e) {
		}
	}

	if (true) { // Reduced case of something that triggered a code path that broke once
		DirectedAcyclicGraph dag(5);
		
		dag.CreateVertex(0);
		dag.CreateVertex(1);
		dag.CreateVertex(2);
		dag.CreateVertex(3);

		dag.SetEdge(1,2);
		dag.RemoveEdge(1,2);
		dag.SetEdge(3,1);
		dag.SetEdge(0,3);
		try {
			dag.SetEdge(2, 0);
		} catch (bad_cycle& e) {
			std::cout << "FAILURE: False cycle found, no path from 0->2 yet insertion of 2->0 failed." << std::endl;
			return false;
		}
		try {
			dag.SetEdge(1, 0);
			std::cout << "FAILURE: Did not find cycle 1->0->3->1." << std::endl;
			return false;
		} catch (bad_cycle& e) {
		}
	}
			
	// Here is the fuzz testing approach with a lot of random adds and removes.
	// http://en.wikipedia.org/wiki/Fuzz_testing
	// (If this fails, try recompiling with DIRECTEDACYCLICGRAPH_CONSISTENCY_CHECK set to 1,
	// that will help pinpoint the source of the invalid state)
		
	typedef RandomEdgePicker<DirectedAcyclicGraph> DAGType;
	
	if (true) {
		unsigned numCyclesCaught = 0;
		unsigned numInsertions = 0;
		unsigned numDeletions = 0;
		
		DAGType dag (NUM_TEST_NODES);
		BoostDirectedAcyclicGraph bdag (NUM_TEST_NODES);
		
		for (DAGType::VertexID vertex = 0; vertex < NUM_TEST_NODES; vertex++) {
			dag.CreateVertex(vertex);
			bdag.CreateVertex(vertex);
		}
		
		// keep adding random vertices and make sure that if it causes a cycle exception on the boost
		// implemented class, that it also causes a cycle exception on the non-boost class
		unsigned index = 0;
		while (index < (NUM_TEST_NODES * NUM_TEST_NODES) / 4) {
			DAGType::VertexID vertexSource;
			DAGType::VertexID vertexDest;
			
			bool removeEdge = (dag.NumEdges() > 0) && ((rand() % 10000) <= (REMOVE_PROBABILITY * 10000));
			
			if (removeEdge) {
				dag.GetRandomEdge(vertexSource, vertexDest);
				
				bdag.RemoveEdge(vertexSource, vertexDest);
				dag.RemoveEdge(vertexSource, vertexDest);
				numDeletions++;
				
			} else {
				dag.GetRandomNonEdge(vertexSource, vertexDest);

#if DIRECTEDACYCLICGRAPH_USER_TRISTATE
				Nstate<3> randomTristate (rand() % 3);
#endif

				bool causedCycleInBoost = false;
				try {
					bdag.AddEdge(vertexSource, vertexDest);
#if DIRECTEDACYCLICGRAPH_USER_TRISTATE
					bdag.SetTristateForConnection(vertexSource, vertexDest, randomTristate);
#endif
				} catch (bad_cycle& e) {
					causedCycleInBoost = true;
				}
				
				bool causedCycle = false;
				try {
					dag.AddEdge(vertexSource, vertexDest);
#if DIRECTEDACYCLICGRAPH_CACHE_REACHABILITY && DIRECTEDACYCLICGRAPH_USER_TRISTATE
					dag.SetTristateForConnection(vertexSource, vertexDest, randomTristate);
#endif
				} catch (bad_cycle& e) {
					causedCycle = true;
				}
								
				if (causedCycle != causedCycleInBoost) {
					std::cout << "FAILURE: Insertion of edge that " << (causedCycleInBoost ? "caused a cycle" : "did not cause a cycle") <<
					" in Boost " << (causedCycle ? "caused a cycle" : "did not cause a cycle") << " in the DirectedAcyclicGraph implementation." << std::endl;
					return false;
				}
				
				if (causedCycle)
					numCyclesCaught++;
				else {
					numInsertions++;
				}
			}

			index++;
		}
		
		std::cout << "NOTE: Inserted " << numInsertions << ", Deleted " << numDeletions << ", and Caught " << numCyclesCaught << " cycles." << std::endl;
		
		// Graphs should be identical after this
		if (bdag != dag) {
			std::cout << "FAILURE: DirectedAcyclicGraph not equivalent to version of DirectedAcyclicGraph implemented via Boost Graph library." << std::endl;
			return false;
		}
		
	}
	
	return true;
}

} // end namespace nocycle

#endif