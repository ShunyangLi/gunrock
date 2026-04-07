/**
 * @file ab_core.hxx
 * @brief (alpha, beta)-core decomposition for bipartite graphs.
 *
 * Given a bipartite graph G = (U ∪ L, E) where:
 *   - Upper vertices U: indices [0, u_num)
 *   - Lower vertices L: indices [u_num, n)
 *
 * For a fixed alpha, computes the maximum beta for each vertex such that
 * the vertex is in the (alpha, beta)-core, where every vertex in U has
 * degree >= alpha and every vertex in L has degree >= beta.
 *
 * Algorithm (iterative peeling):
 *   For beta = 1, 2, ..., beta_max:
 *     - Peel lower vertices with degree <= beta
 *     - Cascade: peel upper vertices whose degree drops to <= alpha
 *     - Record the current beta as the core value for peeled vertices
 */
#pragma once

#include <gunrock/algorithms/algorithms.hxx>
#include <thrust/logical.h>

namespace gunrock {
namespace ab_core {

template <typename vertex_t>
struct param_t {
  vertex_t u_num;   ///< Number of upper vertices (upper = [0, u_num))
  int alpha;        ///< Degree threshold for upper vertices
  int beta_max;     ///< Maximum beta value to iterate to

  param_t(vertex_t _u_num, int _alpha, int _beta_max)
      : u_num(_u_num), alpha(_alpha), beta_max(_beta_max) {}
};

template <typename vertex_t>
struct result_t {
  int* core_values;  ///< Output: beta level at which each vertex was peeled
  result_t(int* _core_values) : core_values(_core_values) {}
};

template <typename graph_t, typename param_type, typename result_type>
struct problem_t : gunrock::problem_t<graph_t> {
  param_type param;
  result_type result;

  problem_t(graph_t& G,
            param_type& _param,
            result_type& _result,
            std::shared_ptr<gcuda::multi_context_t> _context)
      : gunrock::problem_t<graph_t>(G, _context),
        param(_param),
        result(_result) {}

  using vertex_t = typename graph_t::vertex_type;
  using edge_t = typename graph_t::edge_type;
  using weight_t = typename graph_t::weight_type;

  thrust::device_vector<int> degrees;
  thrust::device_vector<bool> deleted;
  thrust::device_vector<bool> to_be_deleted;

  void init() override {
    auto n = this->get_graph().get_number_of_vertices();
    degrees.resize(n);
    deleted.resize(n);
    to_be_deleted.resize(n);
  }

  void reset() override {
    auto g = this->get_graph();
    auto n = g.get_number_of_vertices();
    auto policy = this->context->get_context(0)->execution_policy();

    thrust::fill(policy, this->result.core_values,
                 this->result.core_values + n, 0);
    thrust::fill(policy, deleted.begin(), deleted.end(), false);
    thrust::fill(policy, to_be_deleted.begin(), to_be_deleted.end(), false);

    auto get_deg = [=] __host__ __device__(const vertex_t& i) -> int {
      return g.get_number_of_neighbors(i);
    };
    thrust::transform(policy,
                      thrust::counting_iterator<vertex_t>(0),
                      thrust::counting_iterator<vertex_t>(n),
                      degrees.begin(), get_deg);
  }
};

template <typename problem_t>
struct enactor_t : gunrock::enactor_t<problem_t> {
  enactor_t(problem_t* _problem,
            std::shared_ptr<gcuda::multi_context_t> _context)
      : gunrock::enactor_t<problem_t>(_problem, _context) {}

  using vertex_t = typename problem_t::vertex_t;
  using edge_t = typename problem_t::edge_t;
  using weight_t = typename problem_t::weight_t;
  using frontier_t = typename enactor_t<problem_t>::frontier_t;

  void prepare_frontier(frontier_t* f,
                        gcuda::multi_context_t& context) override {
    auto P = this->get_problem();
    auto n = P->get_graph().get_number_of_vertices();
    f->sequence((vertex_t)0, n, context.get_context(0)->stream());
  }

  void loop(gcuda::multi_context_t& context) override {
    auto E = this->get_enactor();
    auto P = this->get_problem();
    auto G = P->get_graph();

    auto core_values = P->result.core_values;
    auto degrees = P->degrees.data().get();
    auto deleted = P->deleted.data().get();
    auto to_be_deleted = P->to_be_deleted.data().get();
    auto f = this->get_input_frontier();

    auto u_num = P->param.u_num;
    auto alpha = P->param.alpha;
    auto beta = this->iteration + 1;

    // Peel vertices whose degree has dropped to/below their threshold.
    //   Upper vertices (v < u_num): threshold = alpha
    //   Lower vertices (v >= u_num): threshold = current beta
    auto advance_op = [=] __host__ __device__(
                          vertex_t const& source,
                          vertex_t const& neighbor,
                          edge_t const& edge,
                          weight_t const& weight) -> bool {
      if (deleted[source])
        return false;

      int threshold = (source < u_num) ? alpha : beta;
      if (degrees[source] > threshold)
        return false;

      core_values[source] = beta;
      to_be_deleted[source] = true;

      return !deleted[neighbor];
    };

    // For each neighbor of a peeled vertex: atomically decrement its degree.
    // If the degree just dropped to the threshold, keep it in the frontier
    // so it will be peeled in the next advance pass (cascade).
    auto filter_op = [=] __host__ __device__(vertex_t const& v) -> bool {
      if (deleted[v])
        return false;

      int old_deg = math::atomic::add(&degrees[v], -1);
      int threshold = (v < u_num) ? alpha : beta;
      return (old_deg == threshold + 1);
    };

    while (!f->is_empty()) {
      operators::advance::execute_runtime(
          G, E, advance_op,
          operators::load_balance_t::block_mapped, context);

      auto mark_del = [=] __device__(const vertex_t& v) {
        deleted[v] = deleted[v] | to_be_deleted[v];
      };
      operators::parallel_for::execute<operators::parallel_for_each_t::vertex>(
          G, mark_del, context);

      operators::filter::execute_runtime(
          G, E, filter_op,
          operators::filter_algorithm_t::predicated, context);
    }
  }

  virtual bool is_converged(gcuda::multi_context_t& context) override {
    auto P = this->get_problem();
    auto n = P->get_graph().get_number_of_vertices();
    auto f = this->get_input_frontier();
    auto policy = context.get_context(0)->execution_policy();

    if (this->iteration >= P->param.beta_max)
      return true;

    bool all_done = thrust::all_of(
        policy, P->deleted.begin(), P->deleted.end(),
        [] __host__ __device__(bool x) { return x; });
    if (all_done)
      return true;

    // Refill frontier with all vertices for the next beta level
    f->sequence((vertex_t)0, n, context.get_context(0)->stream());
    return false;
  }
};

/**
 * @brief Run (alpha, beta)-core decomposition on a bipartite graph.
 *
 * @tparam graph_t Graph type.
 * @param G       Graph object (vertices [0, u_num) are upper, rest are lower).
 * @param param   Algorithm parameters: u_num, alpha, beta_max.
 * @param result  Output: core_values array of size n_vertices.
 * @param context Device context.
 * @return float  GPU elapsed time in milliseconds.
 */
template <typename graph_t>
float run(graph_t& G,
          param_t<typename graph_t::vertex_type>& param,
          result_t<typename graph_t::vertex_type>& result,
          std::shared_ptr<gcuda::multi_context_t> context =
              std::shared_ptr<gcuda::multi_context_t>(
                  new gcuda::multi_context_t(0))) {
  using vertex_t = typename graph_t::vertex_type;
  using param_type = param_t<vertex_t>;
  using result_type = result_t<vertex_t>;

  using problem_type = problem_t<graph_t, param_type, result_type>;
  using enactor_type = enactor_t<problem_type>;

  problem_type problem(G, param, result, context);
  problem.init();
  problem.reset();

  enactor_type enactor(&problem, context);
  return enactor.enact();
}

}  // namespace ab_core
}  // namespace gunrock
