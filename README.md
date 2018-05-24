# Eigen Library Optimization through Elimination of False Sharing

## Profiling ~~(Predator)~~

_We are not using Predator anymore._

Builds reliable false sharing profile at *word granularity*.

### Profile Def.

The profile is a graph with many disconnected subgraphs; a subgraph corresponds to a cache line (so it has no more than 8 vertices)

**Vertex**: word identifiers (preferably, only where at least 1 false sharing happens, to reduce output size)

- We already have this (word identifier is address)

**Edge**: connects two vertex if there's false sharing between them

- For now we detect false sharing on object (instead of word) level.

**Labels** on vertices: each vertex holds all the program locations that accesses it.

Preferably with *weight* on edges; **weight**: # of times false sharing occurs between the two words.

### Stability of Profile across Runs

What kind of stability guarantee do we need?

- That depends on our padding algorithm.

- Also we'll naturally have fault tolerance for certain aspects of stability; that is to say, slight offset of certain feature across runs won't compromise correctness and will only introduce minimal overhead/efficiency degrading/etc. See below.

#### A Very Rough Sketch of Padding Algorithm

*At (2nd pass) compile time*, grab profile and calculate a padded layout for each cache line, resulting more than one cache lines, and place them *somewhere* + direct the program to that location *somehow*(that's implementation details for later). This assumes

- The word identifiers (vertices) are stable across runs (this must be exact)

- The false sharing sites are stable across runs (this is tolerating)
  
    - Persistent memory relative layout: objects on the same cache line in 1st pass lay on the same cache line in 2nd pass (actually guaranteed within an array)

    - Persistent thread affinity: ~~there exists a permutation of threads such that if we relabel the threads in 2nd pass according to this permutation, each word will be accessed by exactly the same group of thread id in 2nd pass as in 1st pass~~. The subgraphs in 1st and 2nd runs "for each cache line" (identified by stable object identifiers) are isomorphic. (**this is the problem; does this actually hold?**)

#### How to Prove Stability

- Either by proof: can be hard because we may permit slight perturbation instead of full rigidity; how do you argue quantitatively here?

- Or by running multiple times and observe.

### List of Challenges/Todos

#### Predator Issues

There are problems with Predator: it's reporting true sharing as false sharing. We may fix it or we may end up writing one of our own.

#### Word-level Granularity

Currently we identify false sharing at object level; we want to do it at word level (because we're actually optimizing at word level)

#### Word Identifiers

- Address is unstable anyway even with custom allocator; thread may race when doing allocation.

- A solution is using **thread id + id of malloc call + offset**. This is deterministic, *given that* we can identify threads across runs. 

    - My guess is we actually cannot, without certain assumptions of the program. *How to persuade others that it is actually so?*

- For this particular project, we're dropping allocations not done by main (0) thread because that's less than 1% of *total alloc'ed memory*. 

    - This is not equivalent to "less than 1% of false sharing," but intuitively should be so. Especially when the matrices and locks are allocated by main thread in `eigen`.

- Final solution

    - For heap object, use **id of thread0 malloc call + offset**; ignore those cannot be assigned an identifier in this way. This ignores non-main-thread allocation.

    - For global object, use **symbol name + offset**.

    - For stack object, simply ignore all.

## Padding

