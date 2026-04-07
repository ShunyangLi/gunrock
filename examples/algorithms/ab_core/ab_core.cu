#include <gunrock/algorithms/ab_core.hxx>
#include <fstream>

using namespace gunrock;
using namespace memory;

void test_ab_core(int num_arguments, char** argument_array) {
  if (num_arguments < 3) {
    std::cerr << "usage: ./ab_core <filename.bin> <alpha> [beta_max]"
              << std::endl;
    exit(1);
  }

  // --
  // Define types

  using vertex_t = int;
  using edge_t = int;
  using weight_t = float;
  using csr_t =
      format::csr_t<memory_space_t::device, vertex_t, edge_t, weight_t>;

  // --
  // Parse arguments

  std::string filename = argument_array[1];
  int alpha = std::stoi(argument_array[2]);

  // --
  // Load bipartite graph from binary file

  std::ifstream in(filename, std::ios::binary);
  if (!in.is_open()) {
    std::cerr << "Unable to open file: " << filename << std::endl;
    exit(1);
  }

  uint32_t u_num, l_num, n, u_max_degree, l_max_degree, m, k_max;
  in.read(reinterpret_cast<char*>(&u_num), sizeof(u_num));
  in.read(reinterpret_cast<char*>(&l_num), sizeof(l_num));
  in.read(reinterpret_cast<char*>(&n), sizeof(n));
  in.read(reinterpret_cast<char*>(&u_max_degree), sizeof(u_max_degree));
  in.read(reinterpret_cast<char*>(&l_max_degree), sizeof(l_max_degree));
  in.read(reinterpret_cast<char*>(&m), sizeof(m));
  in.read(reinterpret_cast<char*>(&k_max), sizeof(k_max));

  int beta_max = (num_arguments > 3) ? std::stoi(argument_array[3])
                                     : static_cast<int>(l_max_degree);

  std::vector<uint32_t> h_neighbors(2 * m);
  std::vector<uint32_t> h_offsets(n + 1);
  std::vector<uint32_t> h_degrees(n);
  std::vector<int32_t> h_core(n);

  in.read(reinterpret_cast<char*>(h_neighbors.data()),
          sizeof(uint32_t) * 2 * m);
  in.read(reinterpret_cast<char*>(h_offsets.data()),
          sizeof(uint32_t) * (n + 1));
  in.read(reinterpret_cast<char*>(h_degrees.data()),
          sizeof(uint32_t) * n);
  in.read(reinterpret_cast<char*>(h_core.data()),
          sizeof(int32_t) * n);
  in.close();

  // --
  // Populate gunrock CSR (host vectors, then assign to device csr_t)

  thrust::host_vector<edge_t> host_offsets(n + 1);
  thrust::host_vector<vertex_t> host_indices(2 * m);
  thrust::host_vector<weight_t> host_values(2 * m, 1.0f);

  for (uint32_t i = 0; i <= n; i++)
    host_offsets[i] = static_cast<edge_t>(h_offsets[i]);
  for (uint32_t i = 0; i < 2 * m; i++)
    host_indices[i] = static_cast<vertex_t>(h_neighbors[i]);

  csr_t csr;
  csr.number_of_rows = static_cast<vertex_t>(n);
  csr.number_of_columns = static_cast<vertex_t>(n);
  csr.number_of_nonzeros = static_cast<edge_t>(2 * m);
  csr.row_offsets = host_offsets;
  csr.column_indices = host_indices;
  csr.nonzero_values = host_values;

  // --
  // Build graph

  graph::graph_properties_t properties;
  properties.directed = false;
  properties.symmetric = true;
  properties.weighted = false;

  auto G = graph::build<memory_space_t::device>(properties, csr);

  vertex_t n_vertices = G.get_number_of_vertices();
  edge_t n_edges = G.get_number_of_edges();

  std::cout << "Graph loaded from binary:" << std::endl;
  std::cout << "  Upper vertices (u_num) : " << u_num << std::endl;
  std::cout << "  Lower vertices (l_num) : " << l_num << std::endl;
  std::cout << "  Total vertices         : " << n_vertices << std::endl;
  std::cout << "  Edges (directed)       : " << n_edges << std::endl;
  std::cout << "  u_max_degree           : " << u_max_degree << std::endl;
  std::cout << "  l_max_degree           : " << l_max_degree << std::endl;
  std::cout << "  alpha                  : " << alpha << std::endl;
  std::cout << "  beta_max               : " << beta_max << std::endl;

  // --
  // Params and memory allocation

  thrust::device_vector<int> core_values(n_vertices);

  auto context = std::make_shared<gcuda::multi_context_t>(0);

  ab_core::param_t<vertex_t> param(static_cast<vertex_t>(u_num),
                                   alpha, beta_max);
  ab_core::result_t<vertex_t> result(core_values.data().get());

  // --
  // GPU Run

  float gpu_elapsed = ab_core::run(G, param, result, context);

  // --
  // Log

  print::head(core_values, 40, "GPU (alpha,beta)-core values");
  std::cout << "GPU Elapsed Time : " << gpu_elapsed << " (ms)" << std::endl;
}

int main(int argc, char** argv) {
  test_ab_core(argc, argv);
}
