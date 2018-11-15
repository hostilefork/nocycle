//
//  BoostImplementation.hpp - These classes put a relatively thin
//     wrapper on top of the boost graph library to give it the
//     same interface as the OrientedGraph and DirectedAcyclicGraph.
//     The resulting classes (BoostDirectedAcyclicGraph and
//     BoostOrientedGraph) are useful for testing.
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

#include <utility> // boost makes heavy use of "std::pair"
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_matrix.hpp>
#include <boost/graph/depth_first_search.hpp>

#include "OrientedGraph.hpp"
#include "DirectedAcyclicGraph.hpp"

namespace nocycle {

struct BoostVertexProperties {
  #if BOOSTIMPLEMENTATION_TRACK_EXISTENCE || DIRECTEDACYCLICGRAPH_USER_TRISTATE
    bool exists;
  #endif
};

struct BoostEdgeProperties {
  #if DIRECTEDACYCLICGRAPH_USER_TRISTATE
    Nstate<3> tristate;
  #endif
};

typedef boost::adjacency_matrix<
    //
    // `directedS` does not allow easy access to `in_edges` in `adjacency_list`
    // so you'd have to use `bidirectionalS`, which is unfortunately not
    // supported by `adjacency_matrix` (!).  But fortunately for this code,
    // `adjacency_matrix` supports in_edges on `directedS`.  Go figure.
    //
    boost::directedS,

    // "bundled properties"
    // http://www.boost.org/doc/libs/1_38_0/libs/graph/doc/bundles.html
    //
    BoostVertexProperties,
    BoostEdgeProperties
> BoostBaseGraph;

typedef boost::graph_traits<BoostBaseGraph> BoostGraphTraits;

// technically speaking, the boost vertex_descriptor is just an unsigned int
// that increases in order, and many tutorials use it this way.  BUT the guy
// who wrote the book about the boost graph library says you should
// treat it as an opaque handle and use properties if you want to associate
// an integer with every vertex.
//
// http://lists.boost.org/boost-users/2002/07/1288.php
//
// So don't use integers directly to address vertices in boost, even though a
// lot of code samples on the web do this.  To be "correct", use:
//
//  vertex_descriptor vertex(vertices_size_type n, const adjacency_matrix& g)
//  "Returns the nth vertex in the graph's vertex list."
//
typedef BoostGraphTraits::vertex_descriptor BoostVertex;
typedef BoostGraphTraits::edge_descriptor BoostEdge;

// http://www.boost.org/doc/libs/1_38_0/libs/graph/doc/using_adjacency_list.html

// Something of a kluge, we publicly inherit from BoostBaseGraph
// As it's just a class for test, we'll let this slide
class BoostOrientedGraph : public BoostBaseGraph  {
public:
    typedef OrientedGraph::VertexID VertexID;

public:
    BoostOrientedGraph (const size_t initial_size) :
        BoostBaseGraph (initial_size)
    {
#if BOOSTIMPLEMENTATION_TRACK_EXISTENCE
        for (VertexID vertex = 0; vertex < initial_size; vertex++) {
            // used to use boost::add_vertex, but that's not implemented
            BoostVertex bv = boost::vertex(vertex, *this);
            (*this)[bv].exists = false;
        }
#endif
    }

public:
    void CreateVertex(DirectedAcyclicGraph::VertexID vertex) {
#if BOOSTIMPLEMENTATION_TRACK_EXISTENCE
        BoostVertex bv = boost::vertex(vertex, *this);
        nocycle_assert(!(*this)[bv].exists);
        (*this)[bv].exists = true;
#else
        // do nothing; CreateVertex is a noop, but DestroyVertex is illegal
#endif
    }
    void DestroyVertex(DirectedAcyclicGraph::VertexID vertex) {
#if BOOSTIMPLEMENTATION_TRACK_EXISTENCE
        BoostVertex bv = boost::vertex(vertex, *this);
        nocycle_assert((*this)[bv].exists);
        (*this)[bv].exists = false;

        // 1. remove_vertex is not implemented in boost::adjacency_matrix
        // 2. on all graph types, remove_vertex invalidates any vertex descriptors you
        //    might have outstanding... and your vertex_index property map will
        //    be renumbered...

        boost::clear_vertex(bv, (*this)); // only removes incoming and outgoing edges
#else
        nocycle_assert(false);
#endif
    }
    bool VertexExists(DirectedAcyclicGraph::VertexID vertex) const {
#if BOOSTIMPLEMENTATION_TRACK_EXISTENCE
        return (*this)[boost::vertex(vertex, (*this))].exists;
#else
        return true;
#endif
    }
    VertexID GetFirstInvalidVertexID() const {
        return num_vertices((*this));
    }

public:
    std::set<DirectedAcyclicGraph::VertexID> OutgoingEdgesForVertex(VertexID vertex) const {
        nocycle_assert(VertexExists(vertex));

        BoostVertex bv = boost::vertex(vertex, *this);
        BoostGraphTraits::out_edge_iterator out_i, out_end;
        tie(out_i, out_end) = out_edges(bv, (*this)); // tie just breaks a pair down

        // "right" way to get a vertex index from a vertex_descriptor
        // instead of static_cast<int>(vertex_descriptor)
        boost::property_map<BoostBaseGraph, boost::vertex_index_t>::type v_index = boost::get(boost::vertex_index, (*this));

        std::set<VertexID> outgoing;
        while (out_i != out_end) {
            BoostGraphTraits::edge_descriptor e = *out_i;
            BoostVertex src = boost::source(e, *this);
            nocycle_assert(src == bv);
            BoostVertex targ = boost::target(e, *this);
            outgoing.insert(v_index[targ]);
            ++out_i;
        }
        return outgoing;
    }
    std::set<DirectedAcyclicGraph::VertexID> IncomingEdgesForVertex(VertexID vertex) const {
        nocycle_assert(VertexExists(vertex));

        BoostVertex bv = boost::vertex(vertex, *this);
        BoostGraphTraits::in_edge_iterator in_i, in_end;
        tie(in_i, in_end) = boost::in_edges(bv, (*this)); // tie just breaks a pair down

        // "right" way to get a vertex index from a vertex_descriptor
        // instead of static_cast<int>(vertex_descriptor)
        boost::property_map<BoostBaseGraph, boost::vertex_index_t>::type v_index = boost::get(boost::vertex_index, (*this));

        std::set<VertexID> incoming;
        while (in_i != in_end) {
            BoostGraphTraits::edge_descriptor e = *in_i;
            BoostVertex targ = boost::target(e, *this);
            nocycle_assert(targ == bv);
            BoostVertex src = boost::source(e, *this);
            incoming.insert(v_index[src]);
            ++in_i;
        }
        return incoming;
    }

#if DIRECTEDACYCLICGRAPH_USER_TRISTATE
// For testing the tristate storage...
public:
    Nstate<3> GetTristateForConnection(VertexID fromVertex, VertexID toVertex) const {
        BoostVertex bvSource = boost::vertex(fromVertex, (*this));
        BoostVertex bvDest = boost::vertex(toVertex, (*this));

        BoostEdge be;
        bool edgeExists;

        tie(be, edgeExists) = boost::edge(bvSource, bvDest, (*this));
        nocycle_assert(edgeExists);

        return (*this)[be].tristate;
    }
    void SetTristateForConnection(VertexID fromVertex, VertexID toVertex, Nstate<3> tristate) {
        BoostVertex bvSource = boost::vertex(fromVertex, (*this));
        BoostVertex bvDest = boost::vertex(toVertex, (*this));

        BoostEdge be;
        bool edgeExists;

        tie(be, edgeExists) = boost::edge(bvSource, bvDest, (*this));
        nocycle_assert(edgeExists);

        (*this)[be].tristate = tristate;
    }
#endif

public:
    bool HasLinkage(VertexID fromVertex, VertexID toVertex, bool* forwardLink = NULL, bool* reverseLink = NULL) const {
        BoostVertex bvSource = boost::vertex(fromVertex, (*this));
        BoostVertex bvDest = boost::vertex(toVertex, (*this));

        BoostEdge beForward;
        bool forwardEdge;
        BoostEdge beReverse;
        bool reverseEdge;

        tie(beForward, forwardEdge) = boost::edge(bvSource, bvDest, (*this));
        tie(beReverse, reverseEdge) = boost::edge(bvDest, bvSource, (*this));
        if (forwardLink)
            *forwardLink = forwardEdge;
        if (reverseLink)
            *reverseLink = reverseEdge;
        return (forwardEdge || reverseEdge);
    }
    bool EdgeExists(VertexID fromVertex, VertexID toVertex) const {
        BoostVertex bvSource = boost::vertex(fromVertex, (*this));
        BoostVertex bvDest = boost::vertex(toVertex, (*this));

        BoostEdge beForward;
        bool forwardEdge;

        boost::tie(beForward, forwardEdge) = boost::edge(bvSource, bvDest, (*this));
        return forwardEdge;
    }
    bool SetEdge(VertexID fromVertex, VertexID toVertex) {
        nocycle_assert(VertexExists(fromVertex));
        nocycle_assert(VertexExists(toVertex));

        BoostVertex bvSource = boost::vertex(fromVertex, (*this));
        BoostVertex bvDest = boost::vertex(toVertex, (*this));

        // If the edge is already in the graph then a duplicate will not be added
        // http://www.boost.org/doc/libs/1_38_0/libs/graph/doc/adjacency_matrix.html
        bool edgeIsNew;
        BoostEdge be;
        tie(be, edgeIsNew) = boost::add_edge(bvSource, bvDest, (*this));
#if DIRECTEDACYCLICGRAPH_USER_TRISTATE
        (*this)[be].tristate = 0;
#endif
        return edgeIsNew;
    }
    void AddEdge(VertexID fromVertex, VertexID toVertex) {
        if (!SetEdge(fromVertex, toVertex))
            nocycle_assert(false);
    }
    bool ClearEdge(VertexID fromVertex, VertexID toVertex) {
        BoostVertex bvSource = boost::vertex(fromVertex, (*this));
        BoostVertex bvDest = boost::vertex(toVertex, (*this));

        BoostEdge be;
        bool edgeExists;

        tie(be, edgeExists) = boost::edge(bvSource, bvDest, (*this));
        if (!edgeExists)
            return false;

        boost::remove_edge(be, (*this));
        return true;
    }
    void RemoveEdge(VertexID fromVertex, VertexID toVertex) {
        if (!ClearEdge(fromVertex, toVertex))
            nocycle_assert(false);
    }
    bool operator == (const OrientedGraph & og) const {
        if (og.GetFirstInvalidVertexID() != boost::num_vertices(*this))
            return false;

        for (VertexID vertexCheck = 0; vertexCheck < og.GetFirstInvalidVertexID(); vertexCheck++) {
#if BOOSTIMPLEMENTATION_TRACK_EXISTENCE
            if (!this->VertexExists(vertexCheck)) {
                if (og.VertexExists(vertexCheck))
                    return false;

                continue;
            }

            if (!og.VertexExists(vertexCheck))
                return false;
#else
            if (!og.VertexExists(vertexCheck))
                continue;
#endif
            std::set<VertexID> incomingEdges = og.IncomingEdgesForVertex(vertexCheck);
            std::set<VertexID> outgoingEdges = og.OutgoingEdgesForVertex(vertexCheck);

            nocycle_assert(incomingEdges == this->IncomingEdgesForVertex(vertexCheck));
            nocycle_assert(outgoingEdges == this->OutgoingEdgesForVertex(vertexCheck));

            for (VertexID vertexOther = 0; vertexOther < og.GetFirstInvalidVertexID(); vertexOther++) {
                BoostVertex bvOther = boost::vertex(vertexOther, *this);

#if BOOSTIMPLEMENTATION_TRACK_EXISTENCE
                if (!VertexExists(vertexOther)) {
                    if(og.VertexExists(vertexOther))
                        return false;
                    continue;
                }

                if (!og.VertexExists(vertexOther))
                    return false;
#else

                if (!og.VertexExists(vertexOther))
                    continue;
#endif

                if (vertexCheck != vertexOther) {
                    bool forwardEdgeInOg, reverseEdgeInOg;
                    og.HasLinkage(vertexCheck, vertexOther, &forwardEdgeInOg, &reverseEdgeInOg);

                    if (this->EdgeExists(vertexCheck, vertexOther)) {
                        if (!forwardEdgeInOg)
                            return false;
                        nocycle_assert(outgoingEdges.find(vertexOther) != outgoingEdges.end());
                    } else {
                        if (forwardEdgeInOg)
                            return false;
                        nocycle_assert(outgoingEdges.find(vertexOther) == outgoingEdges.end());
                    }

                    if (this->EdgeExists(vertexOther, vertexCheck)) {
                        if (!reverseEdgeInOg)
                            return false;
                        nocycle_assert(incomingEdges.find(vertexOther) != incomingEdges.end());
                    } else {
                        if (reverseEdgeInOg)
                            return false;
                        nocycle_assert(incomingEdges.find(vertexOther) == incomingEdges.end());
                    }
                }
            }
        }
        return true;
    }
    bool operator != (const OrientedGraph & og) const {
        return !((*this) == og);
    }
    virtual ~BoostOrientedGraph() {};
};

// Something of a kluge, we publicly inherit from BoostOrientedGraph
// as this is just a test class, we'll let it pass for now.
class BoostDirectedAcyclicGraph : public BoostOrientedGraph {
public:
    typedef DirectedAcyclicGraph::VertexID VertexID;

public:
    BoostDirectedAcyclicGraph (const size_t initial_size = 0) :
        BoostOrientedGraph (initial_size)
    {
    }


private:
    // this method hooks the DFS "back_edge", taken from:
    // http://www.boost.org/doc/libs/1_38_0/libs/graph/doc/file_dependency_example.html
    struct cycle_detector : public boost::dfs_visitor<>
    {
        cycle_detector(bool& has_cycle)
            : _has_cycle(has_cycle)
        {
        }

        template <class BoostEdge, class BoostBaseGraph>
        void back_edge(BoostEdge, BoostBaseGraph&) {
            _has_cycle = true;
        }

    protected:
        bool& _has_cycle;
    };

private:

    // this method hooks the DFS "on_discover_vertex", adapted from listing 1:
    // http://www.ddj.com/cpp/184401546
    struct reachability_detector : public boost::base_visitor<reachability_detector> {
        typedef boost::on_discover_vertex event_filter;
        reachability_detector (BoostVertex test_vertex, bool& is_reachable) :
            _test_vertex (test_vertex),
            _is_reachable (is_reachable)
        {
        }
        template <class BoostVertex, class BoostBaseGraph>
        void operator()(BoostVertex vertex, BoostBaseGraph& graph)
        {
            // boost algorithms have no termination condition other than finishing the visitation
            // the only way out is to throw an exception
            // "Note that, like with other BGL search algorithms, our A* implementation
            //  has no termination condition other than that it has visited every vertex in the
            //  same connected component as the start vertex. A custom visitor can cause the
            //  algorithm to terminate by throwing an exception"
            // http://www.cs.rpi.edu/~beevek/research/astar_bgl04.pdf
            if (vertex == _test_vertex) {
                _is_reachable = true;
            }
        }
        protected:
            BoostVertex _test_vertex;
            bool& _is_reachable;
    };

public:
    bool SetEdge(VertexID fromVertex, VertexID toVertex) {

        if (0) {

            // First way I found on the web was a cycle detector
            // to see if an insertion would cause a cycle, perform it and then see if cycle
            // If so, remove the link, otherwise success

            bool newEdge = BoostOrientedGraph::SetEdge(fromVertex, toVertex);
            if (!newEdge)
                return false;

            bool has_cycle = false;
            cycle_detector vis (has_cycle);

            if (0) {
                // Searches entire graph for cycles
                boost::depth_first_search((*this), boost::visitor(vis));
            } else {
                // First improvement: searches only region connected to fromVertex for cycles
                // see also: http://lists.boost.org/Archives/boost/2000/10/5573.php
                // http://www.boost.org/doc/libs/1_38_0/libs/graph/doc/depth_first_visit.html
                // http://archives.free.net.ph/message/20080228.115853.14aa712c.el.html

                // depth_first_visit is not documented well, and BGL is fairly abstruse if you get beyond the samples
                // but thanks to this message by Vladimir Prus I was able to figure out the magic to use it:
                // http://aspn.activestate.com/ASPN/Mail/Message/boost/2204027
                std::vector<boost::default_color_type> colors(num_vertices((*this)));

                boost::depth_first_visit((*this), boost::vertex(fromVertex, (*this)), vis,
                    boost::make_iterator_property_map(&colors[0], boost::get(boost::vertex_index, (*this))));
            }

            if (has_cycle) {
                BoostOrientedGraph::RemoveEdge(fromVertex, toVertex);

                bad_cycle bc;
                throw bc;
            }

            return true;

        } else {

            // Second improvement: do reachability without modifying the graph
            // Builds on depth_first_visit instead of depth_first_search
            if (EdgeExists(fromVertex, toVertex))
                return false;

            bool is_reachable = false;
            BoostVertex find_vertex = boost::vertex(fromVertex, (*this));

            // this line from http://www.ddj.com/cpp/184401546 -- listing 1, again.
            boost::dfs_visitor<std::pair<reachability_detector,boost::null_visitor> > vis2 =
                std::make_pair(reachability_detector(find_vertex, is_reachable), boost::null_visitor());

            // http://aspn.activestate.com/ASPN/Mail/Message/boost/2204027
            std::vector<boost::default_color_type> colors(num_vertices((*this)));

            boost::depth_first_visit((*this), boost::vertex(toVertex, (*this)), vis2,
                boost::make_iterator_property_map(&colors[0], boost::get(boost::vertex_index, (*this))));

            if (is_reachable) {
                bad_cycle bc;
                throw bc;
            }

            bool newEdge = BoostOrientedGraph::SetEdge(fromVertex, toVertex);
            assert(newEdge);
            return true;
        }

    }
    void AddEdge(VertexID fromVertex, VertexID toVertex) {
        if (!SetEdge(fromVertex, toVertex))
            nocycle_assert(false);
    }
    bool operator == (const DirectedAcyclicGraph & dag) const {
        if (static_cast<const BoostOrientedGraph&>(*this) != static_cast<const OrientedGraph&>(dag))
            return false;

#if DIRECTEDACYCLICGRAPH_USER_TRISTATE
        // additional checking - the tristates on the edges must match
        for (OrientedGraph::VertexID vertexCheck = 0; vertexCheck < dag.GetFirstInvalidVertexID(); vertexCheck++) {
            if (!VertexExists(vertexCheck))
                continue;

            for (OrientedGraph::VertexID vertexOther = 0; vertexOther < dag.GetFirstInvalidVertexID(); vertexOther++) {
                if (vertexOther == vertexCheck)
                    continue;

                if (!VertexExists(vertexOther))
                    continue;

                if (EdgeExists(vertexCheck, vertexOther)) {
                    if (dag.GetTristateForConnection(vertexCheck, vertexOther) != GetTristateForConnection(vertexCheck, vertexOther))
                        return false;
                }
            }
        }
#endif

        return true;
    }
    bool operator != (const DirectedAcyclicGraph & dag) const {
        return !((*this) == dag);
    }
    virtual ~BoostDirectedAcyclicGraph() { }
};

} // end namespace nocycle
