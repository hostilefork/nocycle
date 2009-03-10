//
//  PerformanceTest.cpp - Simple random performance test which can be
//      run against the DirectedAcyclicGraph or its boost wrapper
//      equivalent.  Uses the boost POSIX time library because <ctime.h>
//      can't add time intervals.
//
//  Copyright (c) 2009 HostileFork.com
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://hostilefork.com/nocycle for documentation.
//

#include <iostream>

#include "Nstate.hpp"
#include "OrientedGraph.hpp"
#include "DirectedAcyclicGraph.hpp"
#include "BoostImplementation.hpp"

#include "RandomEdgePicker.hpp"

#include <set>
#include <map>

#include "boost/date_time/posix_time/posix_time.hpp" 

#define REGRESSION_TESTS 1
const unsigned NUM_TEST_NODES = /*16384*/ 12288;
const unsigned NUM_TEST_ITERATIONS = NUM_TEST_NODES*4;
const float REMOVE_PROBABILITY = 0.0/8.0; // zero for now

int main (int argc, char * const argv[]) {

	boost::posix_time::time_duration addTime = boost::posix_time::seconds(0);
	boost::posix_time::time_duration removeTime = boost::posix_time::seconds(0);

#if REGRESSION_TESTS && NSTATE_SELFTEST
	if (nstate::Nstate<3>::SelfTest()) {
		std::cout << "SUCCESS: All Nstate SelfTest() passed regression." << std::endl;
	} else {
		return 1;
	}
	
	if (nstate::NstateArray<3>::SelfTest()) {
		std::cout << "SUCCESS: All NstateArray SelfTest() passed regression." << std::endl;
	} else {
		return 1;
	}
#endif

#if REGRESSION_TESTS && ORIENTEDGRAPH_SELFTEST	
	if (nocycle::OrientedGraph::SelfTest()) {
		std::cout << "SUCCESS: All OrientedGraph SelfTest() passed regression." << std::endl;
	} else {
		return 1;
	}
#endif
	
#if REGRESSION_TESTS && DIRECTEDACYCLICGRAPH_SELFTEST
	if (nocycle::DirectedAcyclicGraph::SelfTest()) {
		std::cout << "SUCCESS: All DirectedAcyclicGraph SelfTest() passed regression." << std::endl;
	} else {
		return 1;
	}
#endif

	unsigned numCyclesCaught = 0;
	unsigned numInsertions = 0;
	unsigned numDeletions = 0;
	
	typedef nocycle::RandomEdgePicker<nocycle::BoostDirectedAcyclicGraph> DAGType;
	DAGType dag (NUM_TEST_NODES);
	
	for (DAGType::VertexID vertex = 0; vertex < NUM_TEST_NODES; vertex++) {
		dag.CreateVertex(vertex);
	}
	
	// keep adding random vertices and make sure that if it causes a cycle exception on the boost
	// implemented class, that it also causes a cycle exception on the non-boost class
	unsigned index = 0;
	while (index < NUM_TEST_ITERATIONS) {
		DAGType::VertexID vertexSource;
		DAGType::VertexID vertexDest;
		
		bool removeEdge = (dag.NumEdges() > 0) && ((rand() % 10000) <= (REMOVE_PROBABILITY * 10000));
		
		if (removeEdge) {
		
			// We don't count the time for picking the vertex in the performance evaluation
			dag.GetRandomEdge(vertexSource, vertexDest);
			
			// We want to be twice as likely to pick a vertex with 2 outgoing connections as 1,
			// and three times as likely to pick a vertex with 3 outgoing connections... etc.
			// Vertices with 0 outgoing connections will not be picked!
			
			boost::posix_time::ptime timeStart = boost::posix_time::microsec_clock::local_time();
			dag.RemoveEdge(vertexSource, vertexDest);
			boost::posix_time::time_duration timeDuration = (boost::posix_time::microsec_clock::local_time() - timeStart);
			removeTime += timeDuration;
			std::cout << "dag.RemoveEdge(" << vertexSource << ", " << vertexDest << ")" << " : " << timeDuration << std::endl;
			
			numDeletions++;
			
		} else {
			dag.GetRandomNonEdge(vertexSource, vertexDest);

			unsigned numOutgoing = dag.OutgoingEdgesForVertex(vertexSource).size();
			
			bool causedCycle = false;
			boost::posix_time::ptime timeStart = boost::posix_time::microsec_clock::local_time();
			try {
				dag.AddEdge(vertexSource, vertexDest);
			} catch (nocycle::bad_cycle& e) {
				causedCycle = true;
			}
			boost::posix_time::time_duration timeDuration = (boost::posix_time::microsec_clock::local_time() - timeStart);
			addTime += timeDuration;
			
			if (causedCycle) {
				std::cout << " CYCLE! ";
				numCyclesCaught++;
			} else {
				numInsertions++;
			}
			std::cout << "dag.AddEdge(" << vertexSource << ", " << vertexDest << ")" << " : " << timeDuration << std::endl;
		}

		index++;
	}
	std::cout << "NOTE: Inserted " << numInsertions << ", Deleted " << numDeletions << ", and Caught " << numCyclesCaught << " cycles." << std::endl;
	std::cout << "NOTE: Total AddEdge time = " << addTime << std::endl;
	std::cout << "NOTE: Total RemoveEdge time = " << removeTime << std::endl;

	return 0;
}