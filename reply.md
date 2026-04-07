We thank the reviewer for the feedback. We want to clarify that GFQ takes a very different approach compared to methods like BiCore-Index. While index-based methods require a lot of memory and complex updates when the graph changes, GFQ is completely index-free. Instead, it uses connected components (CCs) to narrow down the exact area that needs to be computed. 

In addition, GFQ isn't just a basic parallel version of existing algorithms; it relies on specific GPU features. Because real-world bipartite graphs often have very uneven vertex degrees, a standard approach would cause major workload imbalances among GPU threads. To fix this, GFQ uses a "warp-centric" design, where a group of threads (a warp) works together on a single vertex's neighbors. This keeps memory access smooth and avoids bottlenecks. 

Ultimately, this design fits the natural strengths of the hardware. CPUs are great at handling complex, heavily branched tasks like updating large index structures, but GPUs are much better suited for the simple, highly parallel, and repetitive tasks needed for our CC-based approach.





We thank the reviewer for the suggestion. To avoid inserting the same vertex into the candidate queue C multiple times, we use atomic operations to update the vertex degrees. Specifically, a vertex is only added to the queue if the atomic decrement operation shows its degree has dropped exactly from the valid threshold down to the invalid one (for example, dropping from exactly α to α - 1). Because vertex degrees only decrease during the peeling process, this specific check guarantees that each vertex is triggered and added exactly once. To handle read/write conflicts on C, we manage the queue in synchronized steps. Warps read from the current queue and atomically write new candidates into a separate buffer for the next round, with a global synchronization step in between to ensure all local queues are safely flushed before the next read phase starts. We completely agree that these implementation details are crucial for understanding the parallel correctness, and we will definitely add a more detailed explanation of these atomic operations and synchronization mechanisms to the methodology section in our revised paper.





We sincerely thank the reviewer for their meticulous reading and for pointing out the presentation inconsistencies between the pseudocode and the textual descriptions. We confirm that these are purely descriptive and typographical oversights that do not affect the underlying computational logic or the experimental results of our algorithms. Following your valuable feedback, we will carefully review and revise the manuscript to ensure perfect alignment between the pseudocode and the main text. We greatly appreciate your rigorous review, which will significantly improve the readability and exactness of our paper.





Thank you for your valuable feedback. To address the lack of GPU baselines, we evaluated GCC+ against Gunrock on the OG dataset (fixing $\beta$ and varying $\alpha$). GCC+ achieves execution times of [0.0156, 0.0144, 0.0175, 0.0223] seconds, significantly outperforming Gunrock's [8.26, 8.27, 8.52, 8.67] seconds. This performance gap exists because Gunrock provides a general graph processing framework, whereas GCC+ incorporates highly specialized memory and thread allocation optimizations specifically designed for $(\alpha, \beta)$-core computation.

We also completely agree with your suggestions regarding ablation studies and GPU-side metrics. In the revised manuscript, we will include a comprehensive ablation study to isolate the impact of each proposed component. Additionally, we will add detailed device-level metrics, including detailed GPU memory usage and utilization rates, to fully substantiate our claims regarding resource efficiency.







## Reviewer 3 ([https://openreview.net/forum id=FndIBZxxEV&noteId=Z6PgDhAfPc](https://openreview.net/forum?id=FndIBZxxEV&noteId=Z6PgDhAfPc))
Q1. The paper only compares against multi-threaded CPU baselines. Could the authors include comparisons with state-of-the-art GPU-based graph processing frameworks or prior GPU k-core/bicore implementations?

> Thank you for your valuable feedback. To address the lack of state-of-the-art GPU-based graph processing frameworks, we evaluated GCC+ against Gunrock on the OG dataset (fixing $\beta$ and varying $\alpha$). GCC+ achieves execution times of [0.0156, 0.0144, 0.0175, 0.0223] seconds, significantly outperforming Gunrock's [8.26, 8.27, 8.52, 8.67] seconds. This performance gap exists because Gunrock provides a general graph processing framework, whereas GCC+ incorporates highly specialized memory and thread allocation optimizations specifically designed for $(\alpha, \beta)$-core computation.
>

Q2. The current implementation mainly relies on warp-centric execution. Could the authors clarify whether additional GPU optimizations (e.g., shared memory usage, kernel fusion, or other memory hierarchy optimizations) were explored? If not, how much performance headroom remains?

> Thank you for this insightful question. While our current presentation primarily highlights the warp-centric execution model, our implementation indeed incorporates several critical memory hierarchy optimizations to maximize GPU throughput. Specifically, we heavily utilize shared memory to maintain warp-local queues; during the peeling phase, each warp accumulates its invalidated vertices in shared memory and flushes them in bulk, which significantly mitigates atomic contention on the global candidate set. Additionally, our design is explicitly engineered to align with the CSR graph format, ensuring coalesced global memory accesses and drastically minimizing thread divergence during neighborhood traversals. Due to space limitations in the main context, we regretfully had to omit these low-level GPU implementation details. In the fellowing, we will add these optimizations to our paper.
>

Q3. The paper claims significant memory savings compared to index-based methods. Could the authors provide a detailed breakdown of memory usage (e.g., graph storage, auxiliary data structures, intermediate buffers) to better understand where the savings come from?

> <font style="color:rgb(31, 31, 31);">Thank you for your insightful question. The significant memory savings primarily stem from the fundamental difference in space complexity: our index-free GCC+ algorithm only requires </font>_<font style="color:rgb(31, 31, 31);">O</font>_<font style="color:rgb(31, 31, 31);">(</font>_<font style="color:rgb(31, 31, 31);">n</font>_<font style="color:rgb(31, 31, 31);">) space to store a single core number for each vertex, whereas index-based methods (like BiCore-Index) must precompute and store massive amounts of structural data for every possible (</font>_<font style="color:rgb(31, 31, 31);">α</font>_<font style="color:rgb(31, 31, 31);">,</font>_<font style="color:rgb(31, 31, 31);">β</font>_<font style="color:rgb(31, 31, 31);">) combination, leading to a much higher </font>_<font style="color:rgb(31, 31, 31);">O</font>_<font style="color:rgb(31, 31, 31);">(</font>_<font style="color:rgb(31, 31, 31);">δ</font>_<font style="color:rgb(31, 31, 31);">⋅</font>_<font style="color:rgb(31, 31, 31);">m</font>_<font style="color:rgb(31, 31, 31);">) space complexity. As demonstrated in Figure 6, this theoretical difference yields massive practical benefits; for instance, on the billion-scale PL dataset, GCC+ consumes only 11.7 GB at peak, compared to over 500 GB required by index-based methods. In the revised manuscript, we will include a more detailed breakdown and comparison of memory usage across all components (such as graph storage, auxiliary data structures, and intermediate buffers) to further clarify exactly where these savings occur.</font>
>

Q4. Can the authors provide an ablation study to separate the contributions of (i) GPU parallelism, (ii) index-free design, and (iii) pruning (GCC+)? This would help clarify which factor contributes most to the observed performance gains.

> <font style="color:rgb(31, 31, 31);">Thank you for the excellent suggestion. We already have the detailed experimental data to isolate these three contributions, and we will explicitly structure this into a dedicated ablation study in the revised manuscript. Specifically, the impact of (i) GPU parallelism is clearly demonstrated by comparing the CPU-based </font>`<font style="color:rgb(68, 71, 70);background-color:rgb(233, 238, 246);">Online</font>`<font style="color:rgb(31, 31, 31);"> method with our GPU-based </font>`<font style="color:rgb(68, 71, 70);background-color:rgb(233, 238, 246);">GCC</font>`<font style="color:rgb(31, 31, 31);"> algorithm in Table 3; (ii) the advantage of our index-free design is evident in the massive reduction in peak memory usage (Figure 6) and preprocessing time compared to index-based baselines ; and (iii) the specific performance gain from our pruning strategy is isolated by comparing the query latency of </font>`<font style="color:rgb(68, 71, 70);background-color:rgb(233, 238, 246);">GCC</font>`<font style="color:rgb(31, 31, 31);"> against </font>`<font style="color:rgb(68, 71, 70);background-color:rgb(233, 238, 246);">GCC+</font>`<font style="color:rgb(31, 31, 31);"> in Table 3. We will add a more comprehensive discussion of these existing comparisons in the revision to ensure the individual impact of each design choice is fully clarified.</font>
>

## <font style="color:rgb(31, 31, 31);">Reviewer 4 (</font>[https://openreview.net/forum?id=FndIBZxxEV&noteId=seNW2UbPip](https://openreview.net/forum?id=FndIBZxxEV&noteId=seNW2UbPip))
The paper is not self-contained in its current form. The related work should not be placed in the appendix.

> Thank you very much for your feedback. In the revised paper, we will add the related work to the main text.
>

In Section 2.1, the algorithm in reference [22] is described concisely and clearly. However, the description of the index is quite vague. I did not see enough details or a thorough explanation of the index itself, which makes it difficult to understand its challenges and why its overhead is so high.

> <font style="color:rgb(31, 31, 31);">Thank you for the valuable feedback. The high overhead of the index-based approach [24, 25] fundamentally stems from the nested property of (</font>_<font style="color:rgb(31, 31, 31);">α</font>_<font style="color:rgb(31, 31, 31);">,</font>_<font style="color:rgb(31, 31, 31);">β</font>_<font style="color:rgb(31, 31, 31);">)-cores; unlike the k-core, BiCore-Index must construct a highly complex 2D hierarchical tree to precompute and maintain contour boundaries for </font>_<font style="color:rgb(31, 31, 31);">every</font>_<font style="color:rgb(31, 31, 31);"> possible (</font>_<font style="color:rgb(31, 31, 31);">α</font>_<font style="color:rgb(31, 31, 31);">, </font>_<font style="color:rgb(31, 31, 31);">β</font>_<font style="color:rgb(31, 31, 31);">) combination. This exhaustive precomputation results in a massive </font>_<font style="color:rgb(31, 31, 31);">O</font>_<font style="color:rgb(31, 31, 31);">(</font>_<font style="color:rgb(31, 31, 31);">δ</font>_<font style="color:rgb(31, 31, 31);">⋅</font>_<font style="color:rgb(31, 31, 31);">m</font>_<font style="color:rgb(31, 31, 31);">) time and space complexity, leading to extreme memory consumption (over 500GB on large graphs). In the revised manuscript, we will significantly expand Section 2.1 to provide a thorough explanation of these underlying index structures and their theoretical bottlenecks, which will more clearly illustrate these challenges and solidly motivate the necessity of our index-free GPU design.</font>
>

<font style="color:rgb(31, 31, 31);">In Algorithm 1, the serial approach checks and removes one node at a time, whereas the authors' algorithm first scans all unsuitable nodes in parallel and then removes them together. Can the authors explain more clearly whether this design reduces the number of iterations compared with the serial approach?</font>

> <font style="color:rgb(31, 31, 31);">Thank you for the insightful question. Yes, the parallel design significantly reduces the number of iterations compared to the serial approach. In Algorithm 1, the serial method alternates between checking and removing vertices in the upper layer and the lower layer , meaning it must continuously bounce back and forth between layers until the graph stabilizes, which inherently limits parallelism and leads to a high iteration count. In contrast, our GCC algorithm eliminates this layer-wise separation by evaluating all vertices across the entire graph simultaneously in a unified scan. All nodes violating their respective degree constraints are grouped into a single global candidate set and removed together as a parallel batch. Because GCC processes the entire frontier of invalid nodes simultaneously, the number of iterations is strictly bounded by the graph's peeling depth (the longest chain of cascading deletions) rather than the number of individual nodes or layer alternations, which drastically reduces the total iteration count and synchronization overhead.</font>
>

Regarding the second contribution, the paper mentions “efficiently within GPU memory limits.” However, this essentially only refers to not using an index, which does not constitute a meaningful memory optimization. A more compelling contribution would involve handling graph scales that exceed the GPU’s memory capacity or demonstrating specific optimizations for such scenarios.

> Thank you for this insightful suggestion. We fully agree that our current memory efficiency primarily stems from the algorithmic reduction in space complexity via our index-free design, rather than specific system-level optimizations for graphs exceeding physical GPU memory limits. To address this important point, we will add a dedicated discussion section in the revised manuscript exploring strategies for handling out-of-core graph scales. Specifically, we will discuss potential techniques such as leveraging Unified Memory, partition-based streaming graph processing, and multi-GPU collaborative computation, providing a clear technical roadmap for scaling our approach to even larger datasets in future work.
>

Can the performance difference between the authors' algorithm and the online algorithm be compared at the algorithmic level, using the same CPU rather than comparing CPU and GPU implementations?

> Thank you for this insightful comment. We fully agree that isolating algorithmic improvements from raw hardware acceleration is crucial for a comprehensive evaluation. While the baseline GCC algorithm is inherently co-designed with GPU architecture (e.g., utilizing warp-centric execution and shared memory buffers) making a direct 1:1 CPU port impractical and unrepresentative of its design, our advanced algorithm, GCC+, introduces a fundamental algorithmic contribution that is entirely hardware-agnostic: the core-number-based pruning strategy.To demonstrate the purely algorithmic performance difference on the same CPU hardware, we can apply our GCC+ pruning strategy directly to the CPU-based Online algorithm. By preemptively filtering out trivially valid and invalid vertices before the peeling phase begins, this pruning logic dramatically shrinks the search space and the required number of iterations. This results in significant performance gains that are completely independent of GPU parallelism. In the revised manuscript, we will include a discussion and a CPU-based evaluation of this pruning mechanism to explicitly highlight the algorithmic superiority of our approach.
>

The description of experimental results in lines 716–744 should be more concise, and qualitative conclusions or trends should be provided.

> Thanks for your comments. We will describe more clearly in the revised paper .
>

The paper does not explain why some baselines increase with larger values of while others decrease. Also, in Figure 7, why are and set equal? Doesn’t this essentially reduce the computation to core number calculation (refer to definition 2)?

> Thank you for your detailed observations. The diverging performance trends stem from the fundamental differences in algorithmic paradigms. For index-based methods (like BiCore-Index), the subgraphs are completely precomputed; as α and β increase, the resulting (α,β)-core shrinks, meaning fewer vertices need to be retrieved from memory, naturally leading to a decreased query time. In contrast, the Online algorithm relies on on-the-fly iterative peeling; larger α and β values mean more vertices violate the initial thresholds, triggering deeper peeling cascades and more neighbor updates, which directly increases the computation time. Regarding Figure 7, your intuition is completely correct: setting α=β conceptually aligns with the standard bipartite core number calculation (Definition 2). We deliberately included this symmetric setting as a fundamental baseline to evaluate all algorithms under balanced density constraints before moving to the asymmetric queries (α=β) in Figures 8 and 9. Furthermore, this symmetric setting serves to perfectly highlight the theoretical advantage of our GCC+ algorithm: because GCC+ leverages core-number-based pruning, it immediately exploits this α=β symmetry to filter and return the exact result in a single parallel scan, completely bypassing the iterative peeling phase that slows down the other baseline methods. We will explicitly clarify both the reasons for these performance trends and our rationale for the symmetric evaluation in the revised manuscript.
>

The CSR data structure is suitable only for static graph storage. In dynamic scenarios involving edge insertions and deletions, how is the graph data stored on the GPU?

> Thank you for this sharp observation. You are absolutely correct that globally reconstructing the CSR structure for dynamic updates is prohibitively expensive. To efficiently handle edge insertions and deletions, our algorithm employs a highly efficient localized replacement strategy. Specifically, when an edge is updated, we precisely calculate the affected local scope and then directly replace only that specific portion of the CSR data within the GPU memory.
>

## Reviewer 5 ([https://openreview.net/forum?id=FndIBZxxEV&noteId=i37YG2pKSc](https://openreview.net/forum?id=FndIBZxxEV&noteId=i37YG2pKSc))


Since the CPU-based Online method already completes queries within a few seconds, it would be helpful if the authors could clarify the practical scenarios where GPU acceleration provides a clear advantage over strong CPU baselines.

> <font style="color:rgb(31, 31, 31);">Thank you for this insightful question. While a response time of a few seconds is acceptable for offline analysis, it is prohibitively slow for latency-sensitive, high-throughput applications such as real-time fraud detection in financial networks [25]. In these scenarios, systems must perform on-the-fly structural queries to instantly detect non-compliant or suspicious behavior during every single transaction before it is finalized. The millisecond-level real-time responsiveness enabled by our GPU acceleration is therefore critical to maintaining system security without bottlenecking the transaction pipeline.</font>
>

<font style="color:rgb(31, 31, 31);">The degree of parallelism in the proposed GPU algorithm appears to depend heavily on the size of the candidate set . Could the authors provide additional analysis on the distribution of across queries, as well as GPU utilization metrics (e.g., active threads/warps or SM occupancy) to better understand how effectively the GPU resources are utilized?</font>

> Thank you for the excellent suggestion. We tested the activity of Warp. On the WT dataset, Warp’s activity ranged from 31.5 to 31.93. On the LG dataset, Warp’s activity ranged from 31.26 to 31.87. Even on the smallest dataset, TR, Warp’s activity remained around 30. Note that Warp’s maximum activity is 32. This demonstrates that our algorithm can utilize threads to provide efficient computation.
>

The paper reports that BiCore and BIR require more than 500 GB of memory during index construction. Could the authors clarify what intermediate structures lead to such large memory consumption?

> Thanks for your comment. The excessive memory consumption of index-based methods is primarily due to their need to compute and store structural information for all possible (α,β) combinations. This requirement creates a high O(δ⋅m) space complexity, as large intermediate buffers and data structures are needed to maintain complex 2D hierarchical relationships during construction. 
>

The GFQ procedure repeatedly invokes Algorithm 2 on different connected components while assuming continuous vertex IDs. Could the authors clarify how vertex IDs are handled across components?

> Thank you for the technical clarification. To address this, we will explain that vertex management is handled through a global ID array that tracks inactive vertices. Specifically, when a vertex's degree becomes zero, its ID is reclaimed and assigned to a newly inserted vertex. If the number of new vertices exceeds the current array length, new continuous IDs are allocated. Since these operations are performed directly in-memory, they ensure high responsiveness during dynamic updates. We will include this detail about the global ID management and memory-level re-indexing in the revised manuscript to clarify how vertex IDs remain continuous across components.