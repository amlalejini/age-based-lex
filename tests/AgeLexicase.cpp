#define CATCH_CONFIG_MAIN

#include "Catch2/single_include/catch2/catch.hpp"

#include <functional>
#include <algorithm>

#include "emp/math/Random.hpp"
#include "selection/AgeLexicase.hpp"

TEST_CASE("AgeLexicase") {
  const int seed = 2;
  const size_t pop_size = 20;
  const size_t num_fit_funs = 10;
  const size_t num_age_funs = 3;
  // Create performance functions
  emp::vector<
    emp::vector<std::function<double(void)>>
  > fit_funs(pop_size, {});
  for (size_t pop_i = 0; pop_i < pop_size; ++pop_i) {
    for (size_t fit_i = 0; fit_i < num_fit_funs; ++fit_i) {
      fit_funs[pop_i].emplace_back(
        [fit_i](){ return fit_i; }
      );
    }
  }

  // Create age functions
  emp::vector<
    emp::vector<std::function<double(void)>>
  > age_funs(pop_size, {});
  for (size_t pop_i = 0; pop_i < pop_size; ++pop_i) {
    for (size_t age_i = 0; age_i < num_age_funs; ++age_i) {
      age_funs[pop_i].emplace_back(
        [age_i](){ return age_i; }
      );
    }
  }

  emp::Random rnd(seed);
  selection::AgeLexicaseSelect selector(
    fit_funs,
    age_funs,
    rnd
  );
  selector.SetAgeFunOrderLimit(5);

  emp::vector<size_t> selectable_ids(pop_size, 0);
  std::iota(
    selectable_ids.begin(),
    selectable_ids.end(),
    0
  );
  emp::vector<size_t> score_fun_ids(num_fit_funs);
  std::iota(
    score_fun_ids.begin(),
    score_fun_ids.end(),
    0
  );

  auto selected = selector(
    10,
    selectable_ids,
    score_fun_ids
  );

  std::cout << selected << std::endl;


}