#pragma once

#include <map>
#include <set>
#include <unordered_set>
#include <optional>
#include <functional>
#include <utility>
#include <deque>
#include <algorithm>

#include "emp/base/Ptr.hpp"
#include "emp/Evolve/Systematics.hpp"
#include "emp/base/vector.hpp"

// TODO - Data structure for tracking phenotypes
// TODO - Subclass of systematics that makes it easy to search for relative
// TODO - make search more efficient!

// * what population members are having the phylogenetic approximation applied,
// *what taxon is being used for the approximation, and
// *how far they are apart phylogeneticallly.


namespace phylo {

// NOTE - might need to make this more generic!
struct taxon_info {

  using phen_t = emp::vector<double>;
  using has_phen_t = std::true_type;
  using has_mutations_t = std::true_type;
  using has_fitness_t = std::true_type;

  // Phenotype is set of test case scores
  // Need to set an eval vector

  emp::DataNode<
    double,
    emp::data::Current,
    emp::data::Range
  > fitness;            ///< Tracks fitness range for this taxon.

  phen_t phenotype;                   ///< Tracks phenotype associated with this taxon.
  emp::vector<bool> traits_evaluated; ///< Tracks whether a particular trait has been evaluated
  bool phen_recorded=false;

  std::unordered_map<std::string, int> mut_counts;

  const phen_t& GetPhenotype() const {
    return phenotype;
  }

  double GetTraitScore(size_t i) const {
    return phenotype[i];
  }

  double GetFitness() const {
    return fitness.GetMean();
  }

  const emp::vector<bool>& GetTraitsEvaluated() const {
    return traits_evaluated;
  }


  bool TraitEvaluated(size_t i) const {
    emp_assert(i < traits_evaluated.size());
    return traits_evaluated[i];
  }

  void RecordFitness(double fit) {
    fitness.Add(fit);
  }

  void RecordPhenotype(
    const phen_t& phen,
    const emp::vector<bool>& eval
  ) {
    emp_assert(phen.size() == eval.size());
    // If phenotype not recorded on this taxon, update phenotype.
    // Otherwise, merge incoming phenotype with previous phenotype.
    if (!phen_recorded) {
      phenotype = phen;
      traits_evaluated = eval;
      phen_recorded = true;
    } else {
      emp_assert(phen.size() == phenotype.size());
      emp_assert(eval.size() == traits_evaluated.size());
      for (size_t trait_id = 0; trait_id < phen.size(); ++trait_id) {
        // If this trait was already evaluated, don't update.
        if (traits_evaluated[trait_id]) continue;
        phenotype[trait_id] = phen[trait_id];
        traits_evaluated[trait_id] = eval[trait_id];
      }

    }
  }

};

namespace impl {

void FilterEvaluated(size_t min_pool_size, emp::vector<size_t>& pool, const emp::vector<bool>& evaluated) {
  // Loop over pool (backwards), removing items in pool that have been evaluated
  for (int i = pool.size() - 1; i <= 0 && pool.size() > min_pool_size; --i) {
    emp_assert(i < (int)pool.size(), i, pool.size());
    const size_t trait_id = pool[(size_t)i];
    emp_assert(trait_id < evaluated.size());
    if (evaluated[trait_id]) {
      // If trait has been evaluated on this taxon:
      // - remove it from available
      // - add it to excluded
      pool.pop_back();
    }
  }
  emp_assert(pool.size() >= min_pool_size);
}

}

} // end namespace phylo
