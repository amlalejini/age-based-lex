#pragma once

#include <functional>
#include <algorithm>

#include "emp/base/vector.hpp"
#include "emp/math/Random.hpp"
#include "emp/math/random_utils.hpp"
#include "emp/datastructs/vector_utils.hpp"

#include "BaseSelect.hpp"

namespace selection {

struct AgeLexicaseSelect : public BaseSelect {
public:
  using score_fun_t = std::function<double(void)>;
protected:
  // emp::vector< emp::vector<score_fun_t> >& score_fun_sets; ///< Per-candidate, per-function
  // emp::vector< emp::vector<score_fun_t> >& age_fun_sets; ///< Per-candidate, per-function
  emp::vector< emp::vector<score_fun_t> > all_eval_criteria; ///< Per-candidate, per-function
  emp::vector<size_t> all_score_fun_ids;
  emp::vector<size_t> all_age_fun_ids;
  // TODO - create mapping from original age function ids to new age function ids

  emp::Random& random;

  size_t age_fun_order_limit = (size_t) -1; ///< Furthest back in lexicase selection order that age functions can appear

  // --- INTERNAL ---
  emp::vector< emp::vector<double> > score_table;   ///< Used internally to track relevant scores for a given selection event

  emp::vector<size_t> eval_criteria_ordering;               ///< Used internally to track function ordering. WARNING - Don't change values in this!
  emp::vector<size_t> candidate_idxs;               ///< Does not contain ids. Contains indices into score table.
  emp::vector<size_t> all_candidate_ids;
  emp::vector<size_t> all_fun_ids;

  emp::vector<size_t> age_fun_valid_locs;   ///< Possible locations in shuffle for age functions
  emp::vector<size_t> score_fun_ordering;   ///< Initial locations in shuffle for score functions

  void ShuffleEvalOrdering() {
    // std::cout << "-- ShuffleEvalOrdering --" << std::endl;
    // Shuffle age_fun_ordering and score_fun_ordering into each other
    const size_t num_age_funs = all_age_fun_ids.size();
    emp_assert(num_age_funs <= age_fun_valid_locs.size());

    emp::Shuffle(random, score_fun_ordering); /// Shuffle initial locations of
    emp::Shuffle(random, age_fun_valid_locs);   /// Shuffle locations of age functions
    eval_criteria_ordering.resize(score_table.size(), (size_t)-1);
    emp_assert(eval_criteria_ordering.size() == score_fun_ordering.size() + num_age_funs);
    // std::cout << "  Eval criteria: " << eval_criteria_ordering.size() << std::endl;
    // std::cout << "  Age funs: " << num_age_funs << std::endl;
    // std::cout << "  Score fun ordering (" << score_fun_ordering.size() << "): " << score_fun_ordering << std::endl;
    // std::cout << "  Age valid locations: " << age_fun_valid_locs << std::endl;
    // std::cout << "  (1) Eval criteria: " << eval_criteria_ordering.size() << std::endl;
    // (1) Copy score function ordering
    std::copy(
      score_fun_ordering.begin(),
      score_fun_ordering.end(),
      eval_criteria_ordering.begin()
    );
    // std::cout << "  (2) Eval criteria: " << eval_criteria_ordering << std::endl;
    // (2) Copy age functions into correction locations, moving replaced score functions to end
    size_t swap_pos = score_fun_ordering.size();
    size_t age_fun_id = score_fun_ordering.size(); // Age functions start after score functions
    for (size_t age_i = 0; age_i < num_age_funs; ++age_i) {
      const size_t age_loc = age_fun_valid_locs[age_i];
      emp_assert(age_loc < eval_criteria_ordering.size());
      // Move score fun to end
      eval_criteria_ordering[swap_pos] = eval_criteria_ordering[age_loc];
      // Place age function id
      eval_criteria_ordering[age_loc] = age_fun_id;
      ++age_fun_id;
      ++swap_pos;
    }
    // std::cout << "  (3) Eval criteria: " << eval_criteria_ordering << std::endl;
  }

public:

  AgeLexicaseSelect(
    const emp::vector< emp::vector<score_fun_t> >& in_score_fun_sets,
    const emp::vector< emp::vector<score_fun_t> >& in_age_fun_sets,
    emp::Random& in_random
  ) :
    all_eval_criteria(in_score_fun_sets.size()),
    all_score_fun_ids(0),
    all_age_fun_ids(0),
    random(in_random)
  {
    emp_assert(in_score_fun_sets.size() == in_age_fun_sets.size());
    emp_assert(in_score_fun_sets.size() > 0);
    emp_assert(in_age_fun_sets.size() > 0);
    for (size_t cand_id = 0; cand_id < all_eval_criteria.size(); ++cand_id) {
      const auto& score_funs = in_score_fun_sets[cand_id];
      const auto& age_funs = in_age_fun_sets[cand_id];
      for (const auto& score_fun : score_funs) {
        if (cand_id == 0) all_score_fun_ids.emplace_back(all_eval_criteria[cand_id].size());
        all_eval_criteria[cand_id].emplace_back(score_fun);
      }
      for (const auto& age_fun : age_funs) {
        if (cand_id == 0) all_age_fun_ids.emplace_back(all_eval_criteria[cand_id].size());
        all_eval_criteria[cand_id].emplace_back(age_fun);
      }
    }
    SetAgeFunOrderLimit(all_eval_criteria.size());
  }

  emp::vector<size_t>& operator()(size_t n) override;

  emp::vector<size_t>& operator()(
    size_t n,
    const emp::vector<size_t>& candidate_ids,
    const emp::vector<size_t>& score_fun_ids
  );

  void SetAgeFunOrderLimit(size_t k) {
    emp_assert(k <= all_eval_criteria.size());
    age_fun_order_limit = k;
  }

};

emp::vector<size_t>& AgeLexicaseSelect::operator()(size_t n) {
  emp_assert(all_eval_criteria.size() > 0);
  const size_t num_candidates = all_eval_criteria.size();
  // const size_t num_funs = all_score_fun_ids.size();
  // Use all candidates (fill out all candidate ids if doesn't match score fun set)
  if (all_candidate_ids.size() != num_candidates) {
    all_candidate_ids.resize(num_candidates, 0);
    std::iota(
      all_candidate_ids.begin(),
      all_candidate_ids.end(),
      0
    );
  }
  return (*this)(n, all_candidate_ids, all_score_fun_ids);
}

emp::vector<size_t>& AgeLexicaseSelect::operator()(
  size_t n,
  const emp::vector<size_t>& candidate_ids,
  const emp::vector<size_t>& score_fun_ids
) {
  // Store num_candidates and function count for easy use
  const size_t num_candidates = candidate_ids.size();
  const size_t fun_cnt = score_fun_ids.size() + all_age_fun_ids.size();
  emp_assert(all_eval_criteria.size() > 0);
  emp_assert(num_candidates > 0);
  emp_assert(num_candidates <= all_eval_criteria.size());
  emp_assert(fun_cnt > 0);
  emp_assert(fun_cnt <= all_eval_criteria.back().size());
  // Reset internal selected vector
  selected.resize(n, 0);
  // Update the score table
  score_table.resize(fun_cnt);
  // First with score function values
  for (size_t fun_i = 0; fun_i < score_fun_ids.size(); ++fun_i) {
    score_table[fun_i].resize(num_candidates);
    const size_t fun_id = score_fun_ids[fun_i];
    emp_assert(fun_id < all_score_fun_ids.size());
    emp_assert(emp::Has(all_score_fun_ids, fun_id));

    for (size_t cand_i = 0; cand_i < num_candidates; ++cand_i) {
      emp_assert(fun_cnt <= all_eval_criteria[cand_i].size());
      emp_assert(fun_id < all_eval_criteria[cand_i].size());
      const size_t cand_id = candidate_ids[cand_i];
      score_table[fun_i][cand_i] = all_eval_criteria[cand_id][fun_id]();
    }
  }
  // Next, with "age" function values (all age functions)
  for (size_t age_fun_i = 0; age_fun_i < all_age_fun_ids.size(); ++age_fun_i) {
    const size_t score_table_id = score_fun_ids.size() + age_fun_i;
    const size_t eval_fun_id = all_age_fun_ids[age_fun_i];
    emp_assert(score_table_id < score_table.size());
    score_table[score_table_id].resize(num_candidates);
    for (size_t cand_i = 0; cand_i < num_candidates; ++cand_i) {
      const size_t cand_id = candidate_ids[cand_i];
      emp_assert(cand_i < score_table[score_table_id].size());
      emp_assert(cand_id < all_eval_criteria.size());
      emp_assert(eval_fun_id < all_eval_criteria[cand_id].size());
      score_table[score_table_id][cand_i] = all_eval_criteria[cand_id][eval_fun_id]();
    }
  }

  // Come up with lexicase ordering
  // - Need to weave in the age functions to follow constraints
  // Generate ids (into score table) for score functions to shuffle together
  if (score_fun_ids.size() != score_fun_ordering.size()) {
    score_fun_ordering.resize(score_fun_ids.size());
    std::iota(
      score_fun_ordering.begin(),
      score_fun_ordering.end(),
      0
    );
  }
  // Generate correct ids (into score table) for age functions to shuffle together
  if (age_fun_valid_locs.size() != age_fun_order_limit) {
    age_fun_valid_locs.resize(age_fun_order_limit, 0);
    std::iota(
      age_fun_valid_locs.begin(),
      age_fun_valid_locs.end(),
      0
    );
  }

  // Update valid candidate indices into the score table
  if (num_candidates != candidate_idxs.size()) {
    candidate_idxs.resize(num_candidates);
    std::iota(
      candidate_idxs.begin(),
      candidate_idxs.end(),
      0
    );
  }
  emp::vector<size_t> cur_pool, next_pool;
  for (size_t sel_i = 0; sel_i < n; ++sel_i) {
    // Randomize the score ordering
    ShuffleEvalOrdering();
    // Step through each score
    cur_pool = candidate_idxs;
    int depth = -1;
    // For each score, filter the population down to only the best performers.
    for (size_t score_id : eval_criteria_ordering) {
      ++depth;
      double max_score = score_table[score_id][cur_pool[0]]; // Max score starts as first candidate's score on this function.
      next_pool.emplace_back(cur_pool[0]); // Seed the keeper pool with the first candidate.

      for (size_t i = 1; i < cur_pool.size(); ++i) {
        const size_t cand_idx = cur_pool[i];
        const double cur_score = score_table[score_id][cand_idx];
        if (cur_score > max_score) {
          max_score = cur_score;        // This is the new max score for this function
          next_pool.resize(1);          // Clear out candidates with former max score
          next_pool[0] = cand_idx;      // Add this candidate as only one with the new max
        } else if (cur_score == max_score) {
          next_pool.emplace_back(cand_idx);
        }
      }
      // Make next_pool into new cur_pool; make cur_pool allocated space for next_pool
      std::swap(cur_pool, next_pool);
      next_pool.resize(0);
      if (cur_pool.size() == 1) break; // Stop if we're down to just one candidate.
    }
    // Select a random survivor (all equal at this point)
    emp_assert(cur_pool.size() > 0);
    const size_t win_id = (cur_pool.size() == 1)
      ? cur_pool.back()
      : cur_pool[random.GetUInt(cur_pool.size())];
    emp_assert(win_id < candidate_ids.size());
    selected[sel_i] = candidate_ids[win_id]; // Transform win id (which is an index into score table) back into an actual candidate id
  }
  return selected;
}

} // End selection namespace