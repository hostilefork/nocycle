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

// I use boost::posix_time for timing functions, if you don't want to use
// boost then this has to be turned off
#define RECORD_TIME_DURATIONS 0

// If you want to use the boost adjacency_matrix implementation instead of 
// Nocycle's OrientedGraph-based implementation, set to 1.  Probably won't
// work for > 12K nodes
#define USE_BOOST_GRAPH_IMPLEMENTATION 0

// Some regression tests require boost to be included in your library path
// so make sure you've turned off the self tests in NocycleSettings.hpp if
// you don't have boost
#define REGRESSION_TESTS 0

const unsigned NUM_TEST_NODES = 65536 + 1024 /*16384*/ /* 12288 */;
const unsigned NUM_TEST_ITERATIONS = 10000 /* NUM_TEST_NODES*2 */;
const float REMOVE_PROBABILITY = 1.0/8.0;

#include <iostream>

#include "Nstate.hpp"
#include "OrientedGraph.hpp"
#include "DirectedAcyclicGraph.hpp"

#include "RandomEdgePicker.hpp"

#include <set>
#include <map>

#if RECORD_TIME_DURATIONS
#include "boost/date_time/posix_time/posix_time.hpp" 
#endif

#if USE_BOOST_GRAPH_IMPLEMENTATION
#include "BoostImplementation.hpp"
typedef nocycle::RandomEdgePicker<nocycle::BoostDirectedAcyclicGraph> DAGType;
#else
typedef nocycle::RandomEdgePicker<nocycle::DirectedAcyclicGraph> DAGType;
#endif

int main (int argc, char * const argv[]) {

#if RECORD_TIME_DURATIONS
	boost::posix_time::time_duration addTime = boost::posix_time::seconds(0);
	boost::posix_time::time_duration removeTime = boost::posix_time::seconds(0);
#endif

#if REGRESSION_TESTS && NSTATE_SELFTEST
	if (nocycle::Nstate<3>::SelfTest()) {
		std::cout << "SUCCESS: All Nstate SelfTest() passed regression." << std::endl;
	} else {
		return 1;
	}
	
	if (nocycle::NstateArray<3>::SelfTest()) {
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
	
	DAGType dag (NUM_TEST_NODES);
	
	for (DAGType::VertexID vertex = 0; vertex < NUM_TEST_NODES; vertex++) {
		dag.CreateVertex(vertex);
	}
	
	// keep adding random vertices and make sure that if it causes a cycle exception on the boost
	// implemented class, that it also causes a cycle exception on the non-boost class
	for (unsigned index = 0; index < NUM_TEST_ITERATIONS; index++) {
		DAGType::VertexID vertexSource;
		DAGType::VertexID vertexDest;
		
		bool removeEdge = (dag.NumEdges() > 0) && ((rand() % 10000) < (REMOVE_PROBABILITY * 10000));
		
		if (removeEdge) {
		
			// We don't count the time for picking the vertex in the performance evaluation
			dag.GetRandomEdge(vertexSource, vertexDest);
			std::cout << "dag.RemoveEdge(" << vertexSource << ", " << vertexDest << ")";
			
			// We want to be twice as likely to pick a vertex with 2 outgoing connections as 1,
			// and three times as likely to pick a vertex with 3 outgoing connections... etc.
			// Vertices with 0 outgoing connections will not be picked!
#if RECORD_TIME_DURATIONS
			boost::posix_time::ptime timeStart = boost::posix_time::microsec_clock::local_time();
#endif

			dag.RemoveEdge(vertexSource, vertexDest);

#if RECORD_TIME_DURATIONS
			boost::posix_time::time_duration timeDuration = (boost::posix_time::microsec_clock::local_time() - timeStart);
			removeTime += timeDuration;
			std::cout << " : " << timeDuration;
#endif

			std::cout << std::endl;
			numDeletions++;
			
		} else {
			dag.GetRandomNonEdge(vertexSource, vertexDest);
			std::cout << "dag.AddEdge(" << vertexSource << ", " << vertexDest << ")";

#if RECORD_TIME_DURATIONS
			boost::posix_time::ptime timeStart = boost::posix_time::microsec_clock::local_time();
#endif

			bool causedCycle = false;
			try {
				dag.AddEdge(vertexSource, vertexDest);
			} catch (nocycle::bad_cycle& e) {
				causedCycle = true;
			}

#if RECORD_TIME_DURATIONS			
			boost::posix_time::time_duration timeDuration = (boost::posix_time::microsec_clock::local_time() - timeStart);
			addTime += timeDuration;
			std::cout << " : " << timeDuration;
#endif
			if (causedCycle) {
				std::cout << " ==> !!!CYCLE!!! ";
				numCyclesCaught++;
			} else {
				numInsertions++;
			}
			
			std::cout << std::endl;
		}
	}
	std::cout << "NOTE: Inserted " << numInsertions << ", Deleted " << numDeletions << ", and Caught " << numCyclesCaught << " cycles." << std::endl;

#if RECORD_TIME_DURATIONS
	std::cout << "NOTE: Total AddEdge time = " << addTime << std::endl;
	std::cout << "NOTE: Total RemoveEdge time = " << removeTime << std::endl;
#endif

	return 0;
}