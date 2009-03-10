//
//  OrientedGraph.cpp - Class which compactly stores an adjacency matrix 
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

#include "OrientedGraph.hpp"

#if ORIENTEDGRAPH_SELFTEST

#include "BoostImplementation.hpp"
#include "RandomEdgePicker.hpp"

namespace nocycle {

bool OrientedGraph::SelfTest() {

	const unsigned NUM_TEST_NODES = 128;
	
	typedef RandomEdgePicker<OrientedGraph> OGType;
	
	OGType og (NUM_TEST_NODES);
	BoostOrientedGraph bog (NUM_TEST_NODES);

	for (OGType::VertexID vertex = 0; vertex < NUM_TEST_NODES; vertex++) {
		if (og.VertexExists(vertex)) {
			std::cout << "FAILURE: Vertex #" << vertex << 
				" should not exist after constructing empty OrientedGraph" << std::endl;
			return false;
		}
	}
	
	for (OGType::VertexID vertex = 0; vertex < NUM_TEST_NODES; vertex++) {
		if ((rand() % 4) != 0) { // use 75% of available VertexIDs 
			og.CreateVertex(vertex);
			bog.CreateVertex(vertex);
		}
	}

#if BOOSTIMPLEMENTATION_TRACK_EXISTENCE
	for (OGType::VertexID vertex = 0; vertex < NUM_TEST_NODES; vertex++) {
		bool shouldExist = bog.VertexExists(vertex);
		if (og.VertexExists(vertex) != shouldExist) {
			std::cout << "FAILURE: OrientedGraph Vertex #" << vertex << (shouldExist ? " should exist and does not " :
				" should not exist, and it does. ") << std::endl;
				return false;
		}
	}
#endif
	
	// add a smattering of connections to both graphs
	for (unsigned index = 0; index < (NUM_TEST_NODES * NUM_TEST_NODES) / 4; index++) {
		OGType::VertexID vertexSource;
		OGType::VertexID vertexDest;

		og.GetRandomNonEdge(vertexSource, vertexDest);

		bog.AddEdge(vertexSource, vertexDest);
		og.AddEdge(vertexSource, vertexDest);
	}
	
	// Graphs should be identical after this
	if (bog != og) {
		std::cout << "FAILURE: OrientedGraph not equivalent to version of OrientedGraph implemented via Boost Graph library." << std::endl;
		return false;
	}
	
	return true;
}

} // end namespace nocycle

#endif