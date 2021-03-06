#include "grower.h"

#include "counters.h"
#include "fraction.h"

// Custom hash and compare for the Graph type. Treat isomorphic graphs as being equal.
struct GraphHasher {
  size_t operator()(const Graph& g) const { return g.get_graph_hash(); }
};
struct GraphComparer {
  bool operator()(const Graph& g, const Graph& h) const { return g.is_isomorphic(h); }
};

Grower::Grower(int num_worker_threads_, bool skip_final_enum_, bool use_min_theta_opt_,
               bool use_contains_Tk_opt_, int start_idx_, int end_idx_, bool search_ratio_graph_,
               Fraction ratio_to_search_)
    : num_worker_threads(num_worker_threads_),
      skip_final_enum(skip_final_enum_),
      use_min_theta_opt(use_min_theta_opt_),
      use_contains_Tk_opt(use_contains_Tk_opt_),
      start_idx(start_idx_),
      end_idx(end_idx_),
      search_ratio_graph(search_ratio_graph_),
      ratio_to_search(ratio_to_search_),
      log(nullptr),
      log_detail(nullptr),
      log_result(nullptr),
      to_be_processed_id(start_idx_) {}

void Grower::set_logging(std::ostream* summary, std::ostream* detail, std::ostream* result) {
  log = summary;
  log_detail = detail;
  log_result = result;
}

void Grower::set_stats_print_interval(uint64 check_every_n_gen, int print_every_n_seconds) {
  stats_check_every_n_gen = check_every_n_gen;
  stats_print_every_n_seconds = print_every_n_seconds;
}

// Find all canonical isomorphism class representations with up to max_n vertices.
void Grower::grow() {
  assert(Graph::N <= MAX_VERTICES);

  // Initialize empty graph with k-1 vertices.
  std::vector<Graph> collected_graphs[MAX_VERTICES];
  Graph g;
  g.canonicalize();
  collected_graphs[Graph::K - 1].push_back(g);
  Counters::observe_ratio(g, get_ratio(g));

  // First grow to N-1 vertices, accumulate one graph from each isomorphic class.
  for (int n = Graph::K; n < Graph::N; n++) {
    collected_graphs[n] = grow_step(n, collected_graphs[n - 1]);
  }
  Counters::print_counters();
  print_before_final(collected_graphs);

  if (!skip_final_enum) {
    // Finally, enumerate all graphs with N vertices, no need to store graphs.
    enumerate_final_step(collected_graphs[Graph::N - 1]);
    std::sort(results.begin(), results.end());
    if (log_result != nullptr && !search_ratio_graph) {
      for (const auto& r : results) {
        int base_graph_id = std::get<0>(r);
        const Graph& base_graph = std::get<1>(r);
        const Graph& min_ratio_graph = std::get<2>(r);
        // If the graph's ratio is the global minimum, print into the result log.
        if (get_ratio(min_ratio_graph) == Counters::get_min_ratio()) {
          *log_result << "G[" << base_graph_id
                      << "] min_ratio=" << get_ratio(min_ratio_graph).to_string() << "\n  ";
          base_graph.print_concise(*log_result, true);
          *log_result << "  ";
          min_ratio_graph.print_concise(*log_result, true);
        }
      }
      log_result->flush();
    }
  } else {
    std::cout << "Skipped.\n";
  }
}

// Constructs all non-isomorphic graphs with n vertices that are T_k-free,
// and add them to the canonicals. Before calling this, all such graphs
// with <n vertices must already be in the canonicals.
// Note all edges added in this step contains vertex (n-1).
std::vector<Graph> Grower::grow_step(int n, const std::vector<Graph>& base_graphs) {
  assert(n < Graph::N);
  EdgeCandidates edge_candidates(n);
  Counters::new_growth_step(n, base_graphs.size());
  std::unordered_set<Graph, GraphHasher, GraphComparer> results;

  // Add all non-empty graphs from the previous step to the results.
  for (const Graph& g : base_graphs) {
    if (g.get_edge_count() > 0) {
      results.insert(g);
    }
  }

  // This data structure will be reused when processing the graphs.
  Graph copy;

  for (const Graph& g : base_graphs) {
    Counters::increment_growth_processed_graphs_in_current_step();
    EdgeGenerator edge_gen(edge_candidates, g);

    // Loop through all ((K+1)^\binom{n-1}{k-1} - 1) edge combinations, add them to g, and check
    // add to canonicals unless it's isomorphic to an existing one.
    while (edge_gen.next(copy)) {
      if (contains_forbidden_subgraph(copy, n - 1)) {
        edge_gen.notify_contain_tk_skip();
        continue;
      }

      copy.canonicalize();

      if (results.find(copy) == results.cend()) {
        results.insert(copy);
        Counters::observe_ratio(copy, get_ratio(copy));
      }
    }
  }

  std::vector<Graph> new_graphs(results.cbegin(), results.cend());
  std::sort(new_graphs.begin(), new_graphs.end());
  return new_graphs;
}

void Grower::enumerate_final_step(const std::vector<Graph>& base_graphs) {
  int max_idx = static_cast<int>(base_graphs.size());
  if (end_idx > 0 && end_idx < max_idx) {
    max_idx = end_idx + 1;
  }
  for (int i = start_idx; i < max_idx; i++) {
    to_be_processed.push(base_graphs[i]);
  }
  Counters::enter_final_step(to_be_processed.size());
  if (search_ratio_graph) {
    Counters::initialize_ratio_graph_search(ratio_to_search);
  }

  if (num_worker_threads == 0) {
    worker_thread_main(0);
  } else {
    // The worker threads used in the final enumeration phase.
    std::vector<std::thread> worker_threads;

    // Start the threads.
    for (int i = 0; i < num_worker_threads; i++) {
      worker_threads.push_back(std::thread(&Grower::worker_thread_main, this, i));
    }

    // Wait for them to finish.
    for (std::thread& t : worker_threads) {
      t.join();
    }
  }
}

void Grower::worker_thread_main(int thread_id) {
  // These instances will be reused when processing the graphs.
  Graph base;
  int base_graph_id;
  Graph copy;
  Graph min_ratio_graph;
  EdgeCandidates edge_candidates(Graph::N);

  auto last_check_time = std::chrono::steady_clock::now();

  while (true) {
    // The lock scope to safely get a graph from the queue.
    {
      std::scoped_lock lock(queue_mutex);
      if (to_be_processed.empty()) return;
      base = to_be_processed.front();
      base_graph_id = to_be_processed_id++;
      to_be_processed.pop();
      Counters::increment_growth_processed_graphs_in_current_step();
    }

    Fraction min_ratio = Fraction::infinity();
    if (search_ratio_graph) {
      // If we are searching all graphs generating the given ratio value, set min_ratio
      // to be slightly higher than the given value so that the min_ratio optimization
      // won't skip any graph that produces the ratio value.
      min_ratio = ratio_to_search + Fraction::epsilon();
    }

    uint64 graphs_processed = 0;
    EdgeGenerator edge_gen(edge_candidates, base);
    while (edge_gen.next(copy, true, min_ratio)) {
      // Thread 0 has the extra responsibility as time keeper,
      // to periodically ask Counters to print.
      if (thread_id == 0) {
        if (edge_gen.stats_edge_sets % stats_check_every_n_gen == 0) {
          const auto now = std::chrono::steady_clock::now();
          int seconds =
              std::chrono::duration_cast<std::chrono::seconds>(now - last_check_time).count();
          if (seconds >= stats_print_every_n_seconds) {
            std::scoped_lock lock(counters_mutex);
            last_check_time = now;
            Counters::observe_ratio(min_ratio_graph, get_ratio(min_ratio_graph), graphs_processed);
            Counters::observe_edgegen_stats(edge_gen.stats_tk_skip, edge_gen.stats_tk_skip_bits,
                                            edge_gen.stats_theta_edges_skip,
                                            edge_gen.stats_theta_directed_edges_skip,
                                            edge_gen.stats_edge_sets);
            edge_gen.clear_stats();
            Counters::print_at_time_interval();
            if (log != nullptr) {
              edge_gen.print_debug(std::cout, false, base_graph_id);
              edge_gen.print_debug(*log, false, base_graph_id);
            }
          }
        }
      }

      if (contains_forbidden_subgraph(copy, Graph::N - 1)) {
        edge_gen.notify_contain_tk_skip();
        continue;
      }

      // Bookkeeping: retain the minimum ratio value encountered so far, and the graph genreated it.
      graphs_processed++;
      if (!search_ratio_graph) {
        // Normal path: we are searching for min_ratio.
        if (get_ratio(copy) < min_ratio) {
          min_ratio = get_ratio(copy);
          min_ratio_graph = copy;
        }
      } else {
        // Here we are searching for all graphs that produce the given ratio value.
        if (get_ratio(copy) <= ratio_to_search) {
          std::string to_print = "G[" + std::to_string(base_graph_id) +
                                 "], base_ratio = " + get_ratio(base).to_string() +
                                 ", grow_to_ratio = " + get_ratio(copy).to_string() + " :\n  " +
                                 base.serialize_edges() + "\n  " + copy.serialize_edges() + "\n";

          std::scoped_lock lock(counters_mutex);
          std::cout << to_print;
          if (log_result != nullptr) {
            *log_result << to_print;
          }
          Counters::notify_ratio_graph_found(copy, get_ratio(copy));
        }
      }
    }

    // The lock scope to add the min ratio to the global Counters.
    {
      std::scoped_lock lock(counters_mutex);
      results.push_back(std::make_tuple(base_graph_id, base, min_ratio_graph));
      Counters::observe_ratio(min_ratio_graph, get_ratio(min_ratio_graph), graphs_processed);
      Counters::observe_edgegen_stats(
          edge_gen.stats_tk_skip, edge_gen.stats_tk_skip_bits, edge_gen.stats_theta_edges_skip,
          edge_gen.stats_theta_directed_edges_skip, edge_gen.stats_edge_sets);
      if (log_detail != nullptr && !search_ratio_graph) {
        *log_detail << "---- G[" << base_graph_id << "] T[" << thread_id
                    << "]: min_ratio = " << min_ratio.to_string() << " :\n  ";
        base.print_concise(*log_detail, true);
        *log_detail << "  ";
        min_ratio_graph.print_concise(*log_detail, true);
        log_detail->flush();
      }
    }
  }
}

// Print the content of the collected graphs after the growth to console and log files.
void Grower::print_before_final(const std::vector<Graph> collected_graphs[MAX_VERTICES]) const {
  if (log != nullptr) {
    print_state_to_stream(std::cout, collected_graphs);
    print_state_to_stream(*log, collected_graphs);
    log->flush();
  }
  if (log_detail != nullptr) {
    for (int i = 0; i < Graph::N; i++) {
      Fraction min_ratio = Fraction::infinity();
      for (const Graph& g : collected_graphs[i]) {
        min_ratio = std::min(min_ratio, get_ratio(g));
      }

      *log_detail << "-------- Accumulated canonicals[order=" << i
                  << "] : count = " << collected_graphs[i].size()
                  << ", min_ratio = " << min_ratio.to_string() << " --------\n";
      int idx = 0;
      for (const Graph& g : collected_graphs[i]) {
        *log_detail << "  [" << idx++ << "] ";
        g.print_concise(*log_detail, true);
      }
    }
    log_detail->flush();
  }
}
void Grower::print_state_to_stream(std::ostream& os,
                                   const std::vector<Graph> collected_graphs[MAX_VERTICES]) const {
  os << "\n---------------------------------\n"
     << "Growth phase completed. State:\n";
  for (int i = 0; i < Graph::N; i++) {
    int all_directed = 0;
    for (const Graph& g : collected_graphs[i]) {
      if (g.get_directed_edge_count() == g.get_edge_count()) {
        ++all_directed;
      }
    }
    os << "    order=" << i << " : collected= " << collected_graphs[i].size()
       << ", all_edge_directed= " << all_directed << "\n";
  }
  os << "---------------------------------\n\nStarting final enumeration phase...\n";
}
