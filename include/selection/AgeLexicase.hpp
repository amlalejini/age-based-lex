#pragma once

#include <functional>
#include <algorithm>

#include "emp/base/vector.hpp"
#include "emp/math/Random.hpp"
#include "emp/math/random_utils.hpp"

#include "BaseSelect.hpp"

namespace selection {

struct AgeLexicaseSelect : public BaseSelect {
public:
  using score_fun_t = std::function<double(void)>;
protected:
  // emp::vector< emp::vector<score_fun_t> >& score_fun_sets; ///< Per-candidate, per-function
  // emp::vector< emp::vector<score_fun_t> >& age_fun_sets; ///< Per-candidate, per-function
  emp::vector< emp::vector<score_fun_t> > all_eval_criteria; ///< Per-candidate, per-function
  emp::vector<size_t> score_fun_ids;
  emp::vector<size_t> age_fun_ids;

  emp::Random& random;

  // --- INTERNAL ---
  emp::vector< emp::vector<double> > score_table;   ///< Used internally to track relevant scores for a given selection event

  emp::vector<size_t> score_ordering;               ///< Used internally to track function ordering. WARNING - Don't change values in this!
  emp::vector<size_t> candidate_idxs;               ///< Does not contain ids. Contains indices into score table.
  emp::vector<size_t> all_candidate_ids;
  emp::vector<size_t> all_fun_ids;
public:

  AgeLexicaseSelect(
    const emp::vector< emp::vector<score_fun_t> >& in_score_fun_sets,
    const emp::vector< emp::vector<score_fun_t> >& in_age_fun_sets,
    emp::Random& in_random
  ) :
    all_eval_criteria(in_score_fun_sets.size()),
    score_fun_ids(0),
    age_fun_ids(0),
    random(in_random)
  {
    emp_assert(in_score_fun_sets.size() == in_age_fun_sets.size());
    emp_assert(in_score_fun_sets.size() > 0);
    emp_assert(in_age_fun_sets.size() > 0);
    for (size_t cand_id = 0; cand_id < all_eval_criteria.size(); ++cand_id) {
      const auto& score_funs = in_score_fun_sets[cand_id];
      const auto& age_funs = in_age_fun_sets[cand_id];
      for (const auto& score_fun : score_funs) {
        if (cand_id == 0) score_fun_ids.emplace_back(all_eval_criteria[cand_id].size());
        all_eval_criteria[cand_id].emplace_back(score_fun);
      }
      for (const auto& age_fun : age_funs) {
        if (cand_id == 0) age_fun_ids.emplace_back(all_eval_criteria[cand_id].size());
        all_eval_criteria[cand_id].emplace_back(age_fun);
      }
    }

  }

  emp::vector<size_t>& operator()(size_t n) override;

  emp::vector<size_t>& operator()(
    size_t n,
    const emp::vector<size_t>& candidate_ids,
    const emp::vector<size_t>& score_fun_ids
  );

};

emp::vector<size_t>& AgeLexicaseSelect::operator()(size_t n) {
  // TODO - implement
  return selected;
}

emp::vector<size_t>& AgeLexicaseSelect::operator()(
  size_t n,
  const emp::vector<size_t>& candidate_ids,
  const emp::vector<size_t>& score_fun_ids
) {
  // TODO - implement
  std::cout << all_eval_criteria.size() << std::endl;
  // TEst
  for (size_t cand_id = 0; cand_id < all_eval_criteria.size(); ++cand_id) {
    std::cout << "Age fun scores for candidate " << cand_id << ":";
    for (size_t age_fun_id : age_fun_ids) {
      std::cout << " " << all_eval_criteria[cand_id][age_fun_id]();
    }
    std::cout << std::endl;
  }
  // exit(-1);
  // TODO - actually implement :)
  selected.resize(n, 0);
  std::copy(
    candidate_ids.begin(),
    candidate_ids.begin() + n,
    selected.begin()
  );

  return selected;
}

} // End selection namespace