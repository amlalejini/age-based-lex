/* RecombinerLinearFunctionsProgram

Provides functions for recombining linear functions programs

*/
#pragma once

#include <algorithm>
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

public:

  void SetWholeFuncSwapRate(double rate) {
    emp_assert(rate >= 0.0 && rate <= 1.0);
    whole_func_swap_rate = rate;
  }

  void ApplyWholeFunctionRecombination(
    emp::Random& random,
    program_t& program_1,
    program_t& program_2
  ) {
    /* WhileFunctionRecombination:
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
      const bool swap_flip = random.P(whole_func_swap_rate);
      // std::cout << "i="<<i<<"; swap flip=" << swap_flip << std::endl;
      if (i > 0 && (whole_func_swap_rate <= 0.0 || !swap_flip)) {
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


};

}