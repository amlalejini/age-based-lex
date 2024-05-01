/* RecombinerLinearFunctionsProgram

Provides functions for recombining linear functions programs

*/
#pragma once

#include <algorithm>
#include <utility>
#include "emp/bits/BitSet.hpp"
#include "emp/base/vector.hpp"
#include "emp/debug/debug.hpp"
#include "emp/math/Random.hpp"
#include "emp/math/random_utils.hpp"


namespace psynth {

// Not generically implemented
template<
  typename HARDWARE_T,
  typename TAG_T,
  typename ARGUMENT_T
>
class RecombinerLinearFunctionsProgram {
public:

  RecombinerLinearFunctionsProgram() {
    emp_assert(false, "Not implemented!");
  }

};

template<typename HARDWARE_T, size_t TAG_W>
class RecombinerLinearFunctionsProgram<HARDWARE_T, emp::BitSet<TAG_W>, int> {
public:
  using tag_t = emp::BitSet<TAG_W>;
  using arg_t = int;
  using program_t = sgp::cpu::lfunprg::LinearFunctionsProgram<tag_t, arg_t>;
  using function_t = typename program_t::function_t;
  using inst_t = typename program_t::inst_t;
  using hardware_t = HARDWARE_T;
  using inst_lib_t = typename HARDWARE_T::inst_lib_t;

protected:

  double whole_func_swap_rate = 0.0;
  double per_func_seq_xover_rate = 0.0;

public:

  void SetWholeFuncSwapRate(double rate) {
    emp_assert(rate >= 0.0 && rate <= 1.0);
    whole_func_swap_rate = rate;
  }

  void SetFuncSeqCrossoverRate(double rate) {
    emp_assert(rate >= 0.0 && rate <= 1.0);
    per_func_seq_xover_rate = rate;
  }

  void SequenceCrossoverTwoPoint(
    emp::Random& random,
    function_t& func_1,
    function_t& func_2
  ) {
    // (1) Pick two random points in func_1
    const auto func_1_points = FindTwoPoints(random, func_1);
    // (2) Pick two random points in func_2
    const auto func_2_points = FindTwoPoints(random, func_2);
    // (3) Build new func_1
    // TODO
    // (4) Build new func_2
    // TODO
  }

  void ApplyWholeFunctionRecombination(
    emp::Random& random,
    program_t& program_1,
    program_t& program_2
  ) {
    /* WholeFunctionRecombination:
      - Swap functions across two programs.
      - First swap is guaranteed, subsequent swaps happen at per-function swap rate
    */
    emp_assert(program_1.GetSize() > 0);
    emp_assert(program_2.GetSize() > 0);
    // std::cout << "Apply whole function recombination" << std::endl;
    // Generate vectors of function ids to use for determining which functions to swap
    emp::vector<size_t> p1_func_ids(program_1.GetSize(), 0);
    std::iota(p1_func_ids.begin(), p1_func_ids.end(), 0);
    emp::vector<size_t> p2_func_ids(program_2.GetSize(), 0);
    std::iota(p2_func_ids.begin(), p2_func_ids.end(), 0);
    // Determine the maximum number of functions we could swap
    const size_t max_func_swaps = std::min(program_1.GetSize(), program_2.GetSize());

    emp::Shuffle(random, p1_func_ids);
    emp::Shuffle(random, p2_func_ids);

    for (size_t i = 0; i < max_func_swaps; ++i) {
      if (i > 0 && (whole_func_swap_rate <= 0.0 || !random.P(whole_func_swap_rate))) {
        // If not first swap, swap rate > 0, and coin flip says don't swap, continue
        continue;
      }
      // Get functions ids to be swapped
      const size_t p1_func_id = p1_func_ids[i];
      const size_t p2_func_id = p2_func_ids[i];
      // std::cout << "Swap p1[" << p1_func_id << "] and " << "p2["<< p2_func_id << "]" << std::endl;
      std::swap(program_1[p1_func_id], program_2[p2_func_id]);
    }
  }

  void ApplyFunctionSequenceRecombinationTwoPoint(
    emp::Random& random,
    program_t& program_1,
    program_t& program_2
  ) {
    /*
      Apply 2-point crossover to function sequences.
      First crossover (on random functions) is guaranteed.
      After first crossover, use per-function rate.
    */
    emp_assert(program_1.GetSize() > 0);
    emp_assert(program_2.GetSize() > 0);
    // std::cout << "Apply two-point recombination" << std::endl;
    // Generate vectors of function ids to use for determining which functions crossover
    emp::vector<size_t> p1_func_ids(program_1.GetSize(), 0);
    std::iota(p1_func_ids.begin(), p1_func_ids.end(), 0);
    emp::vector<size_t> p2_func_ids(program_2.GetSize(), 0);
    std::iota(p2_func_ids.begin(), p2_func_ids.end(), 0);
    // Determine the maximum number of functions we could xover
    const size_t max_func_crosses = std::min(program_1.GetSize(), program_2.GetSize());
    // Shuffle order of function ids to allow us to guarantee unique pairings.
    emp::Shuffle(random, p1_func_ids);
    emp::Shuffle(random, p2_func_ids);
    for (size_t i = 0; i < max_func_crosses; ++i) {
      if (i > 0 && (whole_func_swap_rate <= 0.0 || !random.P(per_func_seq_xover_rate))) {
        // If not first swap, swap rate > 0, and coin flip says don't swap, continue
        continue;
      }
      // Get functions ids to be recombined
      const size_t p1_func_id = p1_func_ids[i];
      const size_t p2_func_id = p2_func_ids[i];
      // std::cout << "Recombine p1[" << p1_func_id << "] and " << "p2["<< p2_func_id << "]" << std::endl;
      emp_assert(p1_func_id < program_1.GetSize());
      emp_assert(p2_func_id < program_2.GetSize());
      emp_assert(program_1[p1_func_id].GetSize() > 0);
      emp_assert(program_2[p2_func_id].GetSize() > 0);
      // Pick two points for p1
      const auto p1_crossover_points = FindTwoPoints(random, program_1[p1_func_id]);
      // std::cout << "  P1 crossover points: (" << p1_crossover_points.first << "," << p1_crossover_points.second << ")" << std::endl;
      // Pick two points for p2
      const auto p2_crossover_points = FindTwoPoints(random, program_2[p2_func_id]);
      // std::cout << "  P2 crossover points: (" << p2_crossover_points.first << "," << p2_crossover_points.second << ")" << std::endl;
      // Create new p1 function
      function_t new_p1_func(program_1[p1_func_id].GetTags());
      // - (1) Copy p1 0 to p1_crossover_points.first
      emp_assert(p1_crossover_points.first < program_1[p1_func_id].GetSize());
      for (size_t inst_i = 0; inst_i < p1_crossover_points.first; ++inst_i) {
        new_p1_func.PushInst(program_1[p1_func_id][inst_i]);
      }
      // - (2) Copy p2 starting at p2_crossover_points.first until (and including) p2_crossover_points.second
      emp_assert(p2_crossover_points.first < program_2[p2_func_id].GetSize());
      emp_assert(p2_crossover_points.second < program_2[p2_func_id].GetSize());
      for (size_t inst_i = p2_crossover_points.first; inst_i <= p2_crossover_points.second; ++inst_i) {
        new_p1_func.PushInst(program_2[p2_func_id][inst_i]);
      }
      // - (3) Copy p1 p1_crossover_points.second+1 to end of function
      for (size_t inst_i = p1_crossover_points.second + 1; inst_i < program_1[p1_func_id].GetSize(); ++inst_i) {
        new_p1_func.PushInst(program_1[p1_func_id][inst_i]);
      }

      // Create new p2 function
      function_t new_p2_func(program_2[p2_func_id].GetTags());
      // - (1) Copy from p2:
      //        0 to p2_crossover_points.first
      emp_assert(p2_crossover_points.first < program_2[p2_func_id].GetSize());
      for (size_t inst_i = 0; inst_i < p2_crossover_points.first; ++inst_i) {
        new_p2_func.PushInst(program_2[p2_func_id][inst_i]);
      }
      // - (2) Copy from p1:
      //        starting at p1_crossover_points.first until (and including) p1_crossover_points.second
      emp_assert(p1_crossover_points.first < program_1[p1_func_id].GetSize());
      emp_assert(p1_crossover_points.second < program_1[p1_func_id].GetSize());
      for (size_t inst_i = p1_crossover_points.first; inst_i <= p1_crossover_points.second; ++inst_i) {
        new_p2_func.PushInst(program_1[p1_func_id][inst_i]);
      }
      // - (3) Copy from p2:
      //        p2_crossover_points.second+1 to end of function
      for (size_t inst_i = p2_crossover_points.second + 1; inst_i < program_2[p2_func_id].GetSize(); ++inst_i) {
        new_p2_func.PushInst(program_2[p2_func_id][inst_i]);
      }

      program_1[p1_func_id] = new_p1_func;
      program_2[p2_func_id] = new_p2_func;
    }
  }

  std::pair<size_t, size_t> FindTwoPoints(
    emp::Random& random,
    const function_t& func
  ) {
    const size_t func_size = func.GetSize();
    emp_assert(func_size > 0);
    if (func_size == 1) {
      // If function only has 1 instruction, return start and finish as same
      return {0, 0};
    }

    size_t start = random.GetUInt(0, func_size);
    size_t end = random.GetUInt(0, func_size);
    if (start > end) {
      std::swap(start, end);
    } else if ((start == end) && (start == 0)) {
      // start and end both equal, set end to some percentage into function
      const double percent = random.GetDouble(0, 1.0);
      end = (size_t)(percent * (double)func_size);
      emp_assert(end < func_size);
    } else if ((start == end) && (start == (func_size - 1))) {
      // start and end both equal to end of function, move start up some percentage
      const double percent = random.GetDouble(0, 1.0);
      const size_t move_up = (size_t)(percent * (double)func_size);
      emp_assert(move_up < func_size);
      start -= move_up;
      emp_assert(start < end);
    } else if (start == end) {
      // start and end both equal and are both in the middle of the function
      start = 0;
    }
    emp_assert(start < end);
    emp_assert(end < func_size);
    return {start, end};
  }


};

}