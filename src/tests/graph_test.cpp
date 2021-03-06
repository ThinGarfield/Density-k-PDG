#include "../graph.h"

#include "../counters.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iso_stress_test.h"

using namespace testing;

// Helper used in parse_edges().
#define throw_assert(c) \
  if (!(c)) throw std::invalid_argument(#c);

// Returns a graph, constructed from the text representation of the edges. For example from input
//    "{123>2, 013}"
// A 3-graph with 2 edges will be constructed, the first edge is directed with 2 as the head,
// the second edge is undirected.
//
// This is used to facilitate unit tests.
Graph parse_edges(const std::string& text) {
  Graph g;
  throw_assert(Graph::parse_edges(text, g));
  // Verify the parse produced the same graph as the input.
  throw_assert(g.serialize_edges() == text);
  return g;
}

TEST(GraphTest, GraphDataStructure) {
  Graph::set_global_graph_info(3, 7);
  Graph g = parse_edges("{234, 156>5, 123>2, 013}");

  EXPECT_FALSE(g.is_canonical);

  EXPECT_EQ(4, g.edge_count);
  EXPECT_EQ(2, g.undirected_edge_count);
  EXPECT_EQ(g.edges[0].vertex_set, 0b11100);
  EXPECT_EQ(g.edges[0].head_vertex, UNDIRECTED);
  EXPECT_EQ(g.edges[1].vertex_set, 0b1100010);
  EXPECT_EQ(g.edges[1].head_vertex, 5);
  EXPECT_EQ(g.edges[2].vertex_set, 0b1110);
  EXPECT_EQ(g.edges[2].head_vertex, 2);
  EXPECT_EQ(g.edges[3].vertex_set, 0b1011);
  EXPECT_EQ(g.edges[3].head_vertex, UNDIRECTED);
  EXPECT_EQ(g.serialize_edges(), "{234, 156>5, 123>2, 013}");

  g.canonicalize();
  EXPECT_EQ(g.serialize_edges(), "{012>1, 023, 014, 256>5}");
  EXPECT_EQ(g.vertices[0].degree_undirected, 2);
  EXPECT_EQ(g.vertices[0].degree_head, 0);
  EXPECT_EQ(g.vertices[0].degree_tail, 1);

  EXPECT_EQ(g.vertices[1].degree_undirected, 1);
  EXPECT_EQ(g.vertices[1].degree_head, 1);
  EXPECT_EQ(g.vertices[1].degree_tail, 0);

  EXPECT_EQ(g.vertices[2].degree_undirected, 1);
  EXPECT_EQ(g.vertices[2].degree_head, 0);
  EXPECT_EQ(g.vertices[2].degree_tail, 2);

  EXPECT_EQ(g.vertices[3].degree_undirected, 1);
  EXPECT_EQ(g.vertices[3].degree_head, 0);
  EXPECT_EQ(g.vertices[3].degree_tail, 0);

  EXPECT_EQ(g.vertices[4].degree_undirected, 1);
  EXPECT_EQ(g.vertices[4].degree_head, 0);
  EXPECT_EQ(g.vertices[4].degree_tail, 0);

  EXPECT_EQ(g.vertices[5].degree_undirected, 0);
  EXPECT_EQ(g.vertices[5].degree_head, 1);
  EXPECT_EQ(g.vertices[5].degree_tail, 0);

  EXPECT_EQ(g.vertices[6].degree_undirected, 0);
  EXPECT_EQ(g.vertices[6].degree_head, 0);
  EXPECT_EQ(g.vertices[6].degree_tail, 1);
}

// Utility function to create and initialize T_3.
Graph get_T3() {
  Graph::set_global_graph_info(3, 5);
  Graph g = parse_edges("{013, 123>2, 023, 234>2}");
  g.canonicalize();
  return g;
}

TEST(GraphTest, T3) {
  Graph g = get_T3();

  EXPECT_EQ(4, g.edge_count);
  // Canonicalization: 0->1, 1->3, 2->2, 3->0, 4->4.
  EXPECT_EQ(g.edges[0].vertex_set, 0b0111);  // 012
  EXPECT_EQ(g.edges[0].head_vertex, UNDIRECTED);
  EXPECT_EQ(g.edges[1].vertex_set, 0b1011);  // 013
  EXPECT_EQ(g.edges[1].head_vertex, UNDIRECTED);
  EXPECT_EQ(g.edges[2].vertex_set, 0b01101);  // 023>2
  EXPECT_EQ(g.edges[2].head_vertex, 2);
  EXPECT_EQ(g.edges[3].vertex_set, 0b10101);  // 024>2
  EXPECT_EQ(g.edges[3].head_vertex, 2);
  EXPECT_EQ(g.serialize_edges(), "{012, 013, 023>2, 024>2}");

  g.canonicalize();

  EXPECT_EQ(g.vertices[0].degree_undirected, 2);
  EXPECT_EQ(g.vertices[0].degree_head, 0);
  EXPECT_EQ(g.vertices[0].degree_tail, 2);

  EXPECT_EQ(g.vertices[1].degree_undirected, 2);
  EXPECT_EQ(g.vertices[1].degree_head, 0);
  EXPECT_EQ(g.vertices[1].degree_tail, 0);

  EXPECT_EQ(g.vertices[2].degree_undirected, 1);
  EXPECT_EQ(g.vertices[2].degree_head, 2);
  EXPECT_EQ(g.vertices[2].degree_tail, 0);

  EXPECT_EQ(g.vertices[3].degree_undirected, 1);
  EXPECT_EQ(g.vertices[3].degree_head, 0);
  EXPECT_EQ(g.vertices[3].degree_tail, 1);

  EXPECT_EQ(g.vertices[4].degree_undirected, 0);
  EXPECT_EQ(g.vertices[4].degree_head, 0);
  EXPECT_EQ(g.vertices[4].degree_tail, 1);
}

TEST(GraphTest, IsomorphicSlow) {
  Graph::set_global_graph_info(3, 5);
  Graph g = parse_edges("{013>3, 023>3, 014, 034}");
  g.finalize_edges();
  Graph h = parse_edges("{014>0, 034>0, 124, 024}");
  h.finalize_edges();
  EXPECT_TRUE(g.is_isomorphic_slow(h));
  EXPECT_TRUE(h.is_isomorphic_slow(g));
}

TEST(GraphTest, PermuteIsomorphic) {
  Graph g = get_T3();
  Graph h;
  int p[5]{0, 1, 2, 3, 4};
  do {
    g.permute_for_testing(p, h);
    EXPECT_TRUE(g.is_isomorphic_slow(h));
    EXPECT_TRUE(h.is_isomorphic_slow(g));

    EXPECT_EQ(g.get_graph_hash(), h.get_graph_hash());
    h.canonicalize();
    EXPECT_TRUE(h.is_isomorphic(g));
    EXPECT_EQ(g.edge_count, 4);
    EXPECT_EQ(g.undirected_edge_count, 2);
  } while (std::next_permutation(p, p + 5));
}

TEST(GraphTest, PermuteCanonical) {
  Graph g = get_T3();
  g.canonicalize();
  Graph h;
  int p[5]{0, 1, 2, 3, 4};
  g.permute_canonical(p, h);
  EXPECT_TRUE(g.is_identical(h));
  EXPECT_EQ(h.edge_count, 4);
  EXPECT_EQ(h.undirected_edge_count, 2);
}

TEST(GraphTest, PermuteCanonical2) {
  Graph::set_global_graph_info(2, 4);
  Graph h, f;
  Graph g = parse_edges("{03, 12, 02>2, 13>3}");
  g.canonicalize();

  int p[4]{0, 1, 3, 2};
  g.permute_canonical(p, h);
  EXPECT_TRUE(g.is_isomorphic(h));
  h.permute_canonical(p, f);
  EXPECT_TRUE(g.is_identical(f));
}

TEST(GraphTest, Canonicalize) {
  Graph g = get_T3();
  for (int v = 0; v < 4; v++) {
    EXPECT_GE(g.vertices[v].get_degrees(), g.vertices[v + 1].get_degrees());
  }

  Graph h = get_T3();

  EXPECT_TRUE(g.is_canonical);
  EXPECT_TRUE(h.is_canonical);
  EXPECT_EQ(g.get_graph_hash(), h.get_graph_hash());
  EXPECT_TRUE(h.is_isomorphic(g));
  EXPECT_TRUE(g.is_isomorphic(h));
  EXPECT_TRUE(h.is_identical(g));
  EXPECT_TRUE(g.is_identical(h));

  // Canonicalization should be idempotent.
  h.canonicalize();
  EXPECT_TRUE(h.is_canonical);
  EXPECT_EQ(g.get_graph_hash(), h.get_graph_hash());
  EXPECT_TRUE(h.is_isomorphic(g));
}

TEST(GraphTest, Canonicalize2) {
  Graph::set_global_graph_info(3, 7);
  Graph g = parse_edges("{235, 345>4, 245, 456>4}");
  g.canonicalize();
  EXPECT_EQ(g.serialize_edges(), "{012, 013, 023>2, 024>2}");

  EXPECT_EQ(g.vertices[0].get_degrees(), 0x020002);
  EXPECT_EQ(g.vertices[1].get_degrees(), 0x020000);
  EXPECT_EQ(g.vertices[2].get_degrees(), 0x010200);
  EXPECT_EQ(g.vertices[3].get_degrees(), 0x010001);
  EXPECT_EQ(g.vertices[4].get_degrees(), 0x000001);

  Graph h = g;
  h.canonicalize();
  EXPECT_EQ(g.get_graph_hash(), h.get_graph_hash());
  EXPECT_TRUE(h.is_canonical);

  Graph f = get_T3();
  Graph::set_global_graph_info(3, 7);
  f.canonicalize();
  EXPECT_EQ(h.get_graph_hash(), f.get_graph_hash());
}

TEST(GraphTest, Canonicalize3) {
  Graph::set_global_graph_info(2, 7);
  Graph g, h;
  g.add_edge(Edge(0b0101, UNDIRECTED));
  g.copy_edges(h);

  h.canonicalize();
  EXPECT_TRUE(h.is_canonical);
}

Graph get_G4() {
  Graph::set_global_graph_info(4, 7);
  Graph g = parse_edges(
      "{0125>5, 0135>5, 0235>5, 0145>5, 1245>1, 0345>5, 2345, 0126>6, 0136>6, 1236>6, 0146>6, "
      "0246>6, 1246>6, 0346>6, 2356>2}");
  g.canonicalize();
  return g;
}

TEST(GraphTest, Copy) {
  Graph g = get_T3();
  g.add_edge(Edge(0b011001, UNDIRECTED));

  g.canonicalize();
  Graph h;
  g.copy_edges(h);
  h.canonicalize();

  EXPECT_EQ(g.get_graph_hash(), h.get_graph_hash());
  EXPECT_TRUE(h.is_isomorphic(g));
  EXPECT_EQ(g.edge_count, h.edge_count);
  EXPECT_EQ(g.undirected_edge_count, 3);
  EXPECT_EQ(g.undirected_edge_count, h.undirected_edge_count);
}

TEST(GraphTest, NonIsomorphic) {
  Graph g = get_T3();

  Graph h;
  g.copy_edges(h);
  h.add_edge(Edge(0b10110, UNDIRECTED));  // 124
  h.canonicalize();

  Graph f;
  g.copy_edges(f);
  f.add_edge(Edge(0b10110, 1));  // 124
  f.canonicalize();

  EXPECT_NE(g.get_graph_hash(), f.get_graph_hash());
  EXPECT_FALSE(f.is_isomorphic(g));
  EXPECT_NE(h.get_graph_hash(), f.get_graph_hash());
  EXPECT_FALSE(f.is_isomorphic(h));
}

TEST(GraphTest, NonIsomorphicWithSameHash) {
  Graph::set_global_graph_info(3, 5);
  Graph g = parse_edges("{012>0, 013>1, 024, 134, 234}");
  Graph h = parse_edges("{012>1, 013>0, 024, 134, 234}");

  // The two graphs have the same hash, but not isomorphic
  g.canonicalize();
  h.canonicalize();
  EXPECT_FALSE(g.is_isomorphic(h));
  EXPECT_FALSE(h.is_isomorphic(g));
  EXPECT_EQ(g.get_graph_hash(), h.get_graph_hash());
}

TEST(GraphTest, IsomorphicWithSameHash) {
  Graph::set_global_graph_info(2, 6);
  Graph g = parse_edges("{02, 12>1, 04>0, 05>5, 15>5, 35>5}");
  Graph h = parse_edges("{02, 12>1, 03>0, 05>5, 15>5, 45>5}");
  g.canonicalize();
  h.canonicalize();
  EXPECT_EQ(g.get_graph_hash(), h.get_graph_hash());
  EXPECT_FALSE(g.is_identical(h));
  EXPECT_TRUE(h.is_isomorphic(g));
}

TEST(GraphTest, IsomorphicNotIdentical) {
  Graph::set_global_graph_info(3, 5);
  Graph g = parse_edges("{013>3, 023, 123, 014, 024>4, 124}");
  Graph h = parse_edges("{013, 023>3, 123, 014>4, 024, 124}");
  g.canonicalize();
  h.canonicalize();

  EXPECT_FALSE(g.is_identical(h));
  EXPECT_FALSE(h.is_identical(g));
  EXPECT_TRUE(g.is_isomorphic(h));
  EXPECT_TRUE(h.is_isomorphic(g));
}

TEST(GraphTest, Isomorphic_B) {
  Graph::set_global_graph_info(2, 3);
  Graph g = parse_edges("{01>0, 02>2, 12>1}");
  Graph h = parse_edges("{01>1, 02>0, 12>2}");
  EXPECT_TRUE(g.is_isomorphic_slow(h));
}

TEST(GraphTest, Isomorphic_C) {
  Graph::set_global_graph_info(5, 6);
  Graph g = parse_edges("{01234>4, 01245>5, 01345>4}");
  Graph h = parse_edges("{01234>4, 01245>4, 01345>5}");
  g.finalize_edges();
  h.finalize_edges();
  EXPECT_TRUE(g.is_isomorphic_slow(h));
  EXPECT_TRUE(h.is_isomorphic_slow(g));

  g.canonicalize();
  h.canonicalize();
  EXPECT_TRUE(g.is_isomorphic(h));
  EXPECT_TRUE(h.is_isomorphic(g));
}

TEST(GraphTest, Theta) {
  Graph g = get_T3();
  EXPECT_EQ(g.get_theta_ratio(), Fraction(4, 1));

  Graph::set_global_graph_info(2, 5);
  Graph h = parse_edges("{01>0, 12>1, 03>3, 13>3, 04>4, 24>4, 34>4}");
  EXPECT_EQ(h.get_theta_ratio(), Fraction(10, 7));

  Graph::set_global_graph_info(2, 5);
  Graph j = parse_edges("{}");
  EXPECT_EQ(j.get_theta_ratio(), Fraction::infinity());

  Graph::set_global_graph_info(3, 4);
  Graph k = parse_edges("{012>2, 123}");
  EXPECT_EQ(k.get_theta_ratio(), Fraction(3, 1));

  Graph l = parse_edges("{012>2, 123>1, 023>2, 013>0}");
  EXPECT_EQ(l.get_theta_ratio(), Fraction(1, 1));
}

TEST(GraphTest, IsomorphicStress) {
  for (int diff = 0; diff <= 3; diff++) {
    for (int n = diff + 2; n <= 3; n++) {
      int k = n - diff;
      IsomorphismStressTest t(k, n);
      t.run();
    }
  }
}

TEST(GraphTest, GraphCompare) {
  Graph::set_global_graph_info(4, 7);
  Graph g = parse_edges("{0123, 0124}");
  Graph h = parse_edges("{0123, 0124}");
  EXPECT_FALSE(g < h);

  g = parse_edges("{0123, 0124}");
  h = parse_edges("{1234}");
  EXPECT_TRUE(h < g);
  EXPECT_FALSE(g < h);
  EXPECT_LT(h, g);

  g = parse_edges("{0123, 0124}");
  h = parse_edges("{0123, 0124>1}");
  EXPECT_FALSE(h < g);
  EXPECT_TRUE(g < h);
  EXPECT_LT(g, h);

  g = parse_edges("{0123, 0124>2}");
  h = parse_edges("{0123, 0124>1}");
  EXPECT_TRUE(h < g);
  EXPECT_FALSE(g < h);
  EXPECT_LT(h, g);

  g = parse_edges("{0123, 1234}");
  h = parse_edges("{0123, 0134>4}");
  EXPECT_TRUE(h < g);
  EXPECT_FALSE(g < h);
  EXPECT_LT(h, g);
}

TEST(GraphTest, ParseUnexpectedInput) {
  Graph::set_global_graph_info(2, 12);
  EXPECT_THROW(parse_edges("{01, 23, 45, 67, 89, 9a, 9b, ac>a}"), std::invalid_argument);
  EXPECT_THROW(parse_edges("{01, 23, 45, 67, 89, 9a, 9b, ab>9}"), std::invalid_argument);

  Graph::set_global_graph_info(2, 3);
  EXPECT_THROW(parse_edges("{03}"), std::invalid_argument);
  EXPECT_THROW(parse_edges("{01>3}"), std::invalid_argument);
  EXPECT_THROW(parse_edges("{x0}"), std::invalid_argument);
}

TEST(GraphTest, GraphPrint) {
  Graph::set_global_graph_info(2, 12);
  Graph g = parse_edges("{01, 23, 45, 67, 89, 9a, 9b, ab>a}");
  {
    std::stringstream ss;
    g.print_concise(ss, false);
    EXPECT_EQ(ss.str(), "{01, 23, 45, 67, 89, 9a, 9b, ab>a}\n");
  }
  {
    std::stringstream ss;
    g.print_concise(ss, true);
    EXPECT_EQ(ss.str(), "{01  , 23  , 45  , 67  , 89  , 9a  , 9b  , ab>a}\n");
  }
  g.print();
}

TEST(GraphTest, CountersInit) {
  Graph::set_global_graph_info(3, 5);
  Counters::initialize_logging("test", 0, 0, 0, false, Fraction(1, 1), true);
  EXPECT_EQ(Counters::get_min_ratio(), Fraction::infinity());
  EXPECT_EQ(Counters::fmt(123456ULL), "123456");
  EXPECT_EQ(Counters::fmt(123456789ULL), "123`456789");
  EXPECT_EQ(Counters::fmt(12345678901234567ULL), "12345`678901`234567");
  Counters::print_done_message();
  Counters::close_logging();
}