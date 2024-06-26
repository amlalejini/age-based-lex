#pragma once

#include <string>
#include <utility>
#include <algorithm>
#include <functional>
#include <iostream>
#include <sys/stat.h>
#include <limits>
#include <fstream>
#include <ranges>

#include "emp/Evolve/World.hpp"
#include "emp/Evolve/Systematics.hpp"
#include "emp/bits/BitSet.hpp"
#include "emp/matching/MatchBin.hpp"
#include "emp/base/Ptr.hpp"
#include "emp/control/Signal.hpp"
#include "emp/datastructs/set_utils.hpp"

#include "sgp/cpu/lfunprg/LinearFunctionsProgram.hpp"
#include "sgp/cpu/LinearFunctionsProgramCPU.hpp"
#include "sgp/cpu/mem/BasicMemoryModel.hpp"
#include "sgp/inst/InstructionLibrary.hpp"
#include "sgp/EventLibrary.hpp"
#include "sgp/inst/lfpbm/InstructionAdder.hpp"

#include "../phylogeny/phylogeny_utils.hpp"
#include "../utility/GroupManager.hpp"
#include "../utility/printing.hpp"
#include "../selection/SelectionSchemes.hpp"
#include "../utility/pareto.hpp"

#include "ProgSynthConfig.hpp"
#include "ProgSynthOrg.hpp"
#include "Event.hpp"
#include "ProblemManager.hpp"
#include "ProgSynthHardware.hpp"
#include "MutatorLinearFunctionsProgram.hpp"
#include "SelectedStatistics.hpp"
#include "program_utils.hpp"
#include "ProgSynthTaxonInfo.hpp"
#include "RecombinerLinearFunctionsProgram.hpp"

// TODO - Allow for non-functional fitness functions to be used in lexicase selection
// Features:
// - Every K updates, inject N random individuals
// - Configure age appearance limit in lexicase shuffle
// - Recombination (every k updates, inject N individuals generated from recombination)
// - Add extra data tracking

namespace psynth {

namespace world_defs {

constexpr size_t TAG_SIZE = 32;
constexpr size_t FUNC_NUM_TAGS = 1;
constexpr size_t INST_TAG_CNT = 1;
constexpr size_t INST_ARG_CNT = 3;
using TAG_T = emp::BitSet<TAG_SIZE>;
using INST_ARG_T = int;
using PROGRAM_T = sgp::cpu::lfunprg::LinearFunctionsProgram<TAG_T, INST_ARG_T>;
using ORGANISM_T = ProgSynthOrg<PROGRAM_T>;
using MEMORY_MODEL_T = sgp::cpu::mem::BasicMemoryModel;
using MATCHBIN_T = emp::MatchBin<
  size_t,
  emp::HammingMetric<TAG_SIZE>,
  emp::RankedSelector<>,
  emp::NopRegulator
>;

}

class ProgSynthWorld : public emp::World<world_defs::ORGANISM_T> {
public:
  // --- Type aliases --
  using org_t = world_defs::ORGANISM_T;
  using base_t = emp::World<org_t>;
  using genome_t = typename org_t::genome_t;
  using phenotype_t = typename org_t::phenotype_t;
  using hw_memory_model_t = world_defs::MEMORY_MODEL_T;
  using hw_matchbin_t = world_defs::MATCHBIN_T;
  using program_t = world_defs::PROGRAM_T;
  using inst_t = typename program_t::inst_t;
  using inst_arg_t = world_defs::INST_ARG_T;
  using tag_t = world_defs::TAG_T;
  using hardware_t = sgp::cpu::LinearFunctionsProgramCPU<
    hw_memory_model_t,
    inst_arg_t,
    hw_matchbin_t,
    ProgSynthHardwareComponent<tag_t>
  >;
  using inst_lib_t = sgp::inst::InstructionLibrary<hardware_t, inst_t>;
  using event_lib_t = sgp::EventLibrary<hardware_t>;
  using base_event_t = typename event_lib_t::event_t;
  using mutator_t = MutatorLinearFunctionsProgram<hardware_t, tag_t, inst_arg_t>;
  using selection_fun_t = std::function<
    emp::vector<size_t>&(
      size_t,
      const emp::vector<size_t>&,
      const emp::vector<size_t>&
    )
  >;
  // using taxon_info_t = phylo::phenotype_info;
  using taxon_info_t = ProgSynthTaxonInfo;
  using systematics_t = emp::Systematics<
    org_t,
    genome_t,
    taxon_info_t
  >;
  using taxon_t = typename systematics_t::taxon_t;

  using config_t = ProgSynthConfig;

  using recombiner_t = psynth::RecombinerLinearFunctionsProgram<
    hardware_t,
    tag_t,
    inst_arg_t
  >;

  static constexpr size_t TAG_SIZE = world_defs::TAG_SIZE;
  static constexpr size_t FUNC_NUM_TAGS = world_defs::FUNC_NUM_TAGS;
  static constexpr size_t INST_TAG_CNT = world_defs::INST_TAG_CNT;
  static constexpr size_t INST_ARG_CNT = world_defs::INST_ARG_CNT;

  struct PhenUnionInfo {
    emp::BitVector phen_union;
    std::pair<size_t, size_t> org_ids;
    PhenUnionInfo(
      size_t org_a_id,
      const emp::BitVector& org_a_phen,
      size_t org_b_id,
      const emp::BitVector& org_b_phen
    ) :
      phen_union(org_a_phen.OR(org_b_phen)),
      org_ids(org_a_id, org_b_id)
    { ; }
  };

protected:
  const config_t& config;

  bool world_configured = false;                ///< Has the world been configured?

  emp::Ptr<hardware_t> eval_hardware = nullptr; ///< Hardware used for program evaluation
  inst_lib_t inst_lib;                          ///< SGP instruction library
  event_lib_t event_lib;                        ///< SGP event library
  emp::Ptr<mutator_t> mutator = nullptr;        ///< Handles SGP program mutation

  ProblemManager<hardware_t> problem_manager; ///< Manages interface to program synthesis problem
  size_t event_id_numeric_input_sig = 0;      ///< SGP event ID for numeric input signals

  size_t total_training_cases = 0;   ///< Tracks the total number of training cases being used
  size_t total_testing_cases = 0;    ///< Tracks the total number of testing cases
  size_t total_test_evaluations = 0; ///< Tracks the total number of "test case" evaluations across all organisms since the beginning of the run.
  // size_t total_test_estimations = 0;
  bool found_solution = false;
  int solution_id = -1;
  // bool force_full_compete = false;

  size_t test_order_barrier = 0; ///< Used to mark which tests have been moved to front this generation

  size_t max_fit_id = 0;     ///< Tracks the "elite" organism each generation (based on trait estimates)
  double max_fit = 0.0;      ///< Tracks the "elite" organism aggregate score each generation (based on trait estimates)

  std::function<bool(void)> stop_run;               ///< Returns whether we've hit configured stopping condition
  std::function<bool(void)> is_final_update;        ///< Returns whether we're on the final update for this run
  std::function<bool(size_t)> check_org_solution;   ///< Checks whether a given organism is a solution (needs to be configured based on evalution mode)

  emp::Signal<void(size_t)> begin_org_evaluation_sig; ///< Triggered at beginning of an organism's evaluation (in DoEvaluation). Handles phenotype / internal tracking resets
  emp::Signal<void(size_t)> do_org_evaluation_sig;    ///< Evaluates organism on trigger
  emp::Signal<void(size_t)> end_org_evaluation_sig;

  emp::Signal<void(org_t&)> begin_program_eval_sig; ///< Triggered at beginning of program evaluation (program loaded on hardware).

  emp::Signal<void(org_t&, size_t, bool)> begin_program_test_sig;  ///< Triggered right before evaluating a program on a particular test
  emp::Signal<void(org_t&, size_t)> do_program_test_sig;           ///< Evaluates program on particular test on trigger
  emp::Signal<void(org_t&, size_t)> end_program_test_sig;          ///< Evaluates program output. Use *ONLY* for training cases.

  emp::vector<
    emp::vector< std::function<double(void)> >
  > nonperf_fit_funs; // Are folded into fit_fun_set along with training case results

  emp::vector<
    emp::vector< std::function<double(void)> >
  > fit_fun_set;       ///< Per-organism, per-test

  emp::vector<
    std::function<double(void)>
  > agg_score_fun_set; ///< Per-organism, aggregate score

  // std::function<double(size_t, size_t)> estimate_test_score; ///< Estimates test score
  // std::function<double(const phylo::TraitEstInfo&)> adjust_estimate;

  emp::vector<bool> pop_training_coverage;    ///< Per-trait population coverage
  double pop_num_training_cases_covered;
  emp::vector<double> org_aggregate_scores;   ///< Per-organism aggregate scores (using estimated score)
  emp::vector<size_t> org_training_coverage;  ///< Per-organism training case coverage
  emp::vector<size_t> org_num_training_cases; ///< Per-organism number of training cases organism has been evaluated against
  emp::vector< emp::vector<double> > org_training_scores;    ///< Per-organism, scores for each training case
  emp::vector< emp::vector<bool> > org_training_evaluations; ///< Per-organism, evaluated on training case?
  // emp::vector<emp::BitVector> org_training_passes;

  // std::unordered_set<size_t> performance_criteria_ids;
  std::unordered_set<size_t> nonperformance_criteria_ids;

  emp::Ptr<utils::GroupManager> org_groupings = nullptr;    ///< Manages organism groupings. # org groupings should equal # test groupings
  emp::Ptr<utils::GroupManager> test_groupings = nullptr;   ///< Manages test groupings. # org groupings should equal # organism groupings

  emp::vector<size_t> all_org_ids;
  emp::vector<size_t> all_training_case_ids;            ///< Contains ids of all training cases
  emp::Ptr<selection::BaseSelect> selector = nullptr;   ///< Pointer to selector
  emp::vector<size_t> selected_parent_ids;              ///< Contains ids of all selected organisms
  emp::vector<size_t> all_testing_case_ids;             ///< Contains ids of all testing cases (order will be manipualted as we go)

  std::function<void(void)> run_selection_routine;      ///< Runs selection (needs to be configured differently depending on whether we're estimating fitness or not). Calls selection_fun.
  selection_fun_t selection_fun;                        ///< Interface to selection call

  emp::Ptr<systematics_t> systematics_ptr = nullptr;    ///< Pointer to phylogeny tracker

  // -- Output --
  std::string output_dir; ///< Directory where we dump output

  emp::Ptr<emp::DataFile> summary_file_ptr = nullptr;         ///< Manages summary output file
  emp::Ptr<emp::DataFile> phylodiversity_file_ptr = nullptr;  ///< Manages phylodiversity output file
  emp::Ptr<emp::DataFile> elite_file_ptr = nullptr;           ///< Manages elite output file

  SelectedStatistics selection_stats; ///< Utility struct that manages selection statistics

  size_t num_to_inject = 0; ///< How many new organisms to inject this generation?
  size_t num_to_select = 0;

  std::function<void(void)> calc_next_gen_sources;
  std::function<void(void)> inject_orgs;
  recombiner_t recombiner;
  // emp::vector<std::pair<size_t, size_t>> complementary_phen_pairs;
  emp::vector<size_t> complementary_pair_ids; // Ids meaningful only in context of inject_org function
  emp::vector<size_t> front_pair_ids;

  // emp::vector<size_t> recombine_sources; ///< IDs of orgs to recombine
  // std::function<void(void)> identify_

  // -- Internal functions --
  void Setup();
  void SetupProblem();
  void SetupSelection();
  // void SetupNonPerformanceFitFuns();
  void SetupEvaluation();
  void SetupMutator();
  void SetupPhylogenyTracking();
  void SetupDataCollection();
  void SetupStoppingCondition();
  void SetupVirtualHardware();
  void SetupInstructionLibrary();
  void SetupEventLibrary();
  void SetupOrgInjection();

  void SetupOrgInjection_None();
  void SetupOrgInjection_Random();
  void SetupOrgInjection_RecombineRandom();
  void SetupOrgInjection_RecombineComplementary();

  void SetupEvaluation_Full();
  void SetupEvaluation_Cohort();
  void SetupEvaluation_DownSample();

  void SetupSelection_Lexicase();
  void SetupSelection_AgeLexicase();
  void SetupSelection_Tournament();
  void SetupSelection_Truncation();
  void SetupSelection_None();
  void SetupSelection_Random();

  void SetupStoppingCondition_Generations();
  void SetupStoppingCondition_Evaluations();

  void SetupDataCollection_Phylodiversity();
  void SetupDataCollection_Summary();
  void SetupDataCollection_Elite();

  void InitializePopulation();
  void InitializePopulation_LoadSingle();
  void InitializePopulation_Random();

  void DoEvaluation();
  void DoSelection();
  void DoUpdate();
  void DoInjections();

  void SnapshotConfig();
  void SnapshotSolution();
  void SnapshotPhylogeny();
  void SnapshotPhyloGenotypes();

  void FindParetoFrontPairs(const emp::vector< ProgSynthWorld::PhenUnionInfo >& phen_pairs);

public:

  ProgSynthWorld(
    const config_t& in_config
  ) :
    base_t("ProgSynthWorld", false),
    config(in_config)
  {
    NewRandom(config.SEED());
    Setup();
  }

  ~ProgSynthWorld() {
    if (eval_hardware != nullptr) { eval_hardware.Delete(); }
    if (mutator != nullptr) { mutator.Delete(); }
    if (selector != nullptr) { selector.Delete(); }
    if (phylodiversity_file_ptr != nullptr) { phylodiversity_file_ptr.Delete(); }
    if (summary_file_ptr != nullptr) { summary_file_ptr.Delete(); }
    if (elite_file_ptr != nullptr) { elite_file_ptr.Delete(); }
    if (org_groupings != nullptr) { org_groupings.Delete(); }
    if (test_groupings != nullptr) { test_groupings.Delete(); }
  }

  /// @brief Run one step of evolutionary algorithm. Must configure world first.
  void RunStep();

  /// @brief Run evolutionary algorithm forward until configured stopping condition.
  //         Must configure world first.
  void Run();

  /// @brief Instead of running evolutionary search, run a mutation analysis on loaded 'ancestor' file.
  void RunMutationAnalysis();

  const config_t& GetConfig() const { return config; }

  size_t GetNumTrainingCases() const {
    emp_assert(world_configured);
    return total_training_cases;
  }

};

void ProgSynthWorld::RunStep() {
  emp_assert(world_configured);
  calc_next_gen_sources(); // Compute num to select vs inject
  emp_assert((num_to_select + num_to_inject) == config.POP_SIZE());
  DoEvaluation();
  DoSelection();
  DoInjections();
  DoUpdate();
}

void ProgSynthWorld::Run() {
  emp_assert(world_configured);
  // If mutation analysis mode, run mutation analysis and return.
  if (config.MUTATION_ANALYSIS_MODE()) {
    RunMutationAnalysis();
    return;
  }
  // Otherwise, run evolution!
  while (!stop_run()) {
    RunStep();
  }
}

void ProgSynthWorld::DoEvaluation() {
  emp_assert(pop_training_coverage.size() == total_training_cases);
  emp_assert(org_aggregate_scores.size() == config.POP_SIZE());
  emp_assert(fit_fun_set.size() == config.POP_SIZE());
  emp_assert(org_training_scores.size() == config.POP_SIZE());
  // emp_assert(org_training_passes.size() == config.POP_SIZE());
  emp_assert(org_training_coverage.size() == config.POP_SIZE());
  emp_assert(org_num_training_cases.size() == config.POP_SIZE());

  // Reset ID of true max fitness organism
  max_fit_id = config.POP_SIZE() + 1;
  max_fit = 0.0;

  // Update test and organism groupings
  org_groupings->UpdateGroupings();
  test_groupings->UpdateGroupings();

  // Reset population-level training case coverage
  std::fill(
    pop_training_coverage.begin(),
    pop_training_coverage.end(),
    false
  );

  pop_num_training_cases_covered = 0.0;

  test_order_barrier = 0;

  // Evaluate each organism
  for (size_t org_id = 0; org_id < GetSize(); ++org_id) {
    // --- Begin organism evaluation: ---
    // - Reset org aggregate score (world)
    // - Reset org training coverage (world)
    // - Reset org num training cases (world)
    // - Reset org training scores (world)
    // - Reset org phenotype (org)
    begin_org_evaluation_sig.Trigger(org_id);

    // --- Do organism evaluation: ---
    // - Trigger begin_program_eval_sig
    //   - Load program into evaluation hardware
    // - For each test (in grouping):
    //   - Trigger begin_program_test_sig
    //     - Reset eval hardware matchbin
    //     - Reset eval hardwdare state (ResetHardwareState)
    //     - Reset eval hardware custom component
    //     - Use problem manager to load test input onto hardware
    //   - Trigger do_program_test_sig
    //     - Execute program for configured number of CPU cycles
    //   - Trigger end_program_test_sig
    //     - Use problem manager to evaluate hardware output
    //     - org.UpdatePhenotype()
    //     - Update world performance tracking:
    //       - org_training_scores
    //       - org_training_evaluations
    //       - org_training_coverage
    //       - org_num_training_cases
    //       - pop_training_coverage
    // - Increment total test evaluations
    do_org_evaluation_sig.Trigger(org_id);

    // --- End organism evaluation: ---
    // - Check if need to update approx_max_fit org
    // - Check if need to check candidate against testing set
    //   - If so, check if organism is a solution using the testing set
    // Record taxon information
    end_org_evaluation_sig.Trigger(org_id);
  }

  // Estimate aggregate fitness for each organism
  // Need to do this separately to ensure all possible descendants of current taxa have been evaluated
  // Otherwise, estimation can crash

  // org_training_scores
  // org_training_evaluations
  for (size_t org_id = 0; org_id < GetSize(); ++org_id) {
    double agg_score = 0.0;
    for (size_t test_id = 0; test_id < total_training_cases; ++test_id) {
      agg_score += org_training_scores[org_id][test_id] * (size_t)org_training_evaluations[org_id][test_id];
    }
    org_aggregate_scores[org_id] = agg_score;

    // Update identity of elite organism if necessary
    if ((agg_score > max_fit) || (max_fit_id >= config.POP_SIZE()) ) {
      max_fit = agg_score;
      max_fit_id = org_id;
    }
  }

  // If we found a solution, guarantee that the found solution is marked as the elite organism
  if (found_solution) {
    max_fit_id = solution_id;
    max_fit = org_aggregate_scores[solution_id];
  }

}

void ProgSynthWorld::DoSelection() {
  // Run configured selection routine
  run_selection_routine();
  emp_assert(selected_parent_ids.size() + num_to_inject == config.POP_SIZE());
  // std::cout << "DoSelection(): " << selected_parent_ids.size() << std::endl;
  // Each selected parent id reproduces
  for (size_t id : selected_parent_ids) {
    DoBirth(GetGenomeAt(id), id);
  }
}

void ProgSynthWorld::DoInjections() {
  // std::cout << "DoInjections(), num_to_inject: " << num_to_inject << std::endl;
  if (num_to_inject > 0) {
    inject_orgs();
  }
}

void ProgSynthWorld::DoUpdate() {
  // (1) Compute any per-generation statistics / intervals?
  emp_assert(config.PRINT_INTERVAL() > 0);
  const size_t cur_update = GetUpdate();
  const bool final_update = is_final_update();
  const bool print_interval = (config.PRINT_INTERVAL() == 1) || (!(cur_update % config.PRINT_INTERVAL()) || final_update);
  const bool summary_data_interval = !(cur_update % config.OUTPUT_SUMMARY_DATA_INTERVAL()) || final_update;
  const bool snapshot_interval = !(cur_update % config.SNAPSHOT_INTERVAL()) || final_update;
  const bool track_phylo = config.TRACK_PHYLOGENY();

  // (2) File output
  if (summary_data_interval) {
    // Update selection statistics
    selection_stats.Calculate(
      selected_parent_ids,
      *this
    );
    pop_num_training_cases_covered = std::accumulate(
      pop_training_coverage.begin(),
      pop_training_coverage.end(),
      0
    );
    // Update summary file
    summary_file_ptr->Update();
    elite_file_ptr->Update();
    if (track_phylo) {
      phylodiversity_file_ptr->Update();
    }
  }

  if (snapshot_interval && track_phylo) {
    SnapshotPhylogeny();
  }

  if (found_solution) {
    std::cout << "Found solution!" << std::endl;
    SnapshotSolution();
  }

  if (track_phylo && final_update && (cur_update % config.OUTPUT_SUMMARY_DATA_INTERVAL())) {
    GetFile(output_dir + "systematics.csv").Update();
  }

  // (3) Print status
  if (print_interval) {
    std::cout << "update: " << GetUpdate() << "; ";
    std::cout << "best score (" << max_fit_id << "): " << max_fit;
    std::cout << "; pop size: " << GetSize();
    std::cout << std::endl;
  }

  // (4) Update!
  Update();
}

void ProgSynthWorld::Setup() {
  std::cout << "--- Setting up ProgSynth ---" << std::endl;
  world_configured = false;

  // Reset the world
  Reset();
  total_test_evaluations = 0;
  found_solution = false;

  // Configure output directory path, create directory
  output_dir = config.OUTPUT_DIR();
  mkdir(output_dir.c_str(), ACCESSPERMS);
  if(output_dir.back() != '/') {
      output_dir += '/';
  }

  // Setup the population structure
  SetPopStruct_Mixed(true);


  // Configure world to set organism ID on placement
  OnPlacement(
    [this](size_t pos) {
      auto& org = GetOrg(pos);
      org.SetPopID(pos);
      org.ResetPhenotype(total_training_cases);
      emp_assert(org.GetPopID() == pos);
    }
  );

  // Setup the program synthesis problem
  SetupProblem();
  // Setup the instruction library
  SetupInstructionLibrary();
  // Setup the event library
  SetupEventLibrary();
  // Setup the virtual hardware used to evaluate programs
  SetupVirtualHardware();
  // Setup the program mutator
  SetupMutator();
  // Setup evaluation
  SetupEvaluation();
  // Setup selection
  SetupSelection();
  // Setup org injection mode
  SetupOrgInjection();
  // Setup stopping condition
  SetupStoppingCondition();
  // Setup phylogeny tracking
  if (config.TRACK_PHYLOGENY()) {
    SetupPhylogenyTracking();
  }
  // Setup data collection
  SetupDataCollection();
  // Initialize population!
  InitializePopulation();
  // SetAutoMutate!
  SetAutoMutate();
  // Output a snapshot of the run configuration
  SnapshotConfig();
  world_configured = true;
}

void ProgSynthWorld::SetupProblem() {
  std::cout << "Setting up problem." << std::endl;
  if (!problem_manager.IsValidProblem(config.PROBLEM())) {
    std::cout << "Unknown problem: " << config.PROBLEM() << std::endl;
    exit(-1);
  }
  // Configure the problem via the problem manager
  problem_manager.ConfigureProblem(config.PROBLEM());
  problem_manager.LoadTrainingSet(config.TRAINING_SET_PATH());
  problem_manager.LoadTestingSet(config.TESTING_SET_PATH());
  // Print out loaded training/testing set sizes
  std::cout << "  - Loaded training set size: " << problem_manager.GetTrainingSetSize() << std::endl;
  std::cout << "  - Loaded testing set size: " << problem_manager.GetTestingSetSize() << std::endl;
}

void ProgSynthWorld::SetupInstructionLibrary() {
  std::cout << "Setting up instruction library." << std::endl;
  // Reset instruction library
  inst_lib.Clear();
  sgp::inst::lfpbm::InstructionAdder<hardware_t> inst_adder;
  // Add default instructions (except Fork and Terminate)
  inst_adder.AddAllDefaultInstructions(
    inst_lib,
    {"Fork", "Terminate"}
  );
  // Add problem-specific instructions
  problem_manager.AddProblemInstructions(inst_lib);
  // Add early exit instruction
  inst_lib.AddInst(
    "Exit",
    [](hardware_t& hw, const inst_t& inst) {
      auto& component = hw.GetCustomComponent();
      component.FlagStopEval();
    },
    "Early exit"
  );

}

void ProgSynthWorld::SetupEventLibrary() {
  std::cout << "Setting up event library." << std::endl;
  event_lib.Clear();
  // Add default event set
  event_id_numeric_input_sig = event_lib.AddEvent(
    "NumericInputSignal",
    [this](hardware_t& hw, const base_event_t& e) {
      const NumericMessageEvent<TAG_SIZE>& event = static_cast<const NumericMessageEvent<TAG_SIZE>&>(e);
      auto thread_id = hw.SpawnThreadWithTag(event.GetTag());
      if (thread_id && event.GetData().size()) {
        // If message resulted in thread being spawned, load message into local working space.
        auto& thread = hw.GetThread(thread_id.value());
        // Wait, wait. Does this thread have calls on the call stack?
        if (thread.GetExecState().call_stack.size()) {
          auto& call_state = thread.GetExecState().GetTopCallState();
          auto& mem_state = call_state.GetMemory();
          for (auto mem : event.GetData()) {
            mem_state.SetWorking(mem.first, mem.second);
          }
        }
      }
    }
  );
  // Configure problem-specific events
  problem_manager.AddProblemEvents(event_lib);
}

void ProgSynthWorld::SetupVirtualHardware() {
  std::cout << "Setting up virtual hardware." << std::endl;
  // TODO - Implement ability to run hardware in parallel
  if (eval_hardware == nullptr) {
    eval_hardware = emp::NewPtr<hardware_t>(*random_ptr, inst_lib, event_lib);
  }
  // Configure the SGP CPU
  eval_hardware->Reset();
  eval_hardware->SetActiveThreadLimit(config.MAX_ACTIVE_THREAD_CNT());
  eval_hardware->SetThreadCapacity(config.MAX_THREAD_CAPACITY());
  // Configure input tag to all 0s
  tag_t input_tag;
  input_tag.Clear();
  eval_hardware->GetCustomComponent().SetInputTag(input_tag);
  // Configure problem-specific hardware component.
  problem_manager.AddProblemHardware(*eval_hardware);
  // Hardware should be in a valid thread state after configuration.
  emp_assert(eval_hardware->ValidateThreadState());
}

void ProgSynthWorld::SetupMutator() {
  std::cout << "Setting up program mutator." << std::endl;
  if (mutator != nullptr) {
    mutator.Delete();
  }
  mutator = emp::NewPtr<mutator_t>(inst_lib);
  mutator->ResetLastMutationTracker();
  // Set program constraints
  mutator->SetProgFunctionCntRange(
    {config.PRG_MIN_FUNC_CNT(), config.PRG_MAX_FUNC_CNT()}
  );
  mutator->SetProgFunctionInstCntRange(
    {config.PRG_MIN_FUNC_INST_CNT(), config.PRG_MAX_FUNC_INST_CNT()}
  );
  mutator->SetProgInstArgValueRange(
    {config.PRG_INST_MIN_ARG_VAL(), config.PRG_INST_MAX_ARG_VAL()}
  );
  const size_t total_inst_limit = 2 * config.PRG_MAX_FUNC_INST_CNT() * config.PRG_MAX_FUNC_CNT();
  mutator->SetTotalInstLimit(total_inst_limit);
  mutator->SetFuncNumTags(FUNC_NUM_TAGS);
  mutator->SetInstNumTags(INST_TAG_CNT);
  mutator->SetInstNumArgs(INST_ARG_CNT);
  // Set mutation rates
  mutator->SetRateInstArgSub(config.MUT_RATE_INST_ARG_SUB());
  mutator->SetRateInstTagBF(config.MUT_RATE_INST_TAG_BF());
  mutator->SetRateInstSub(config.MUT_RATE_INST_SUB());
  mutator->SetRateInstIns(config.MUT_RATE_INST_INS());
  mutator->SetRateInstDel(config.MUT_RATE_INST_DEL());
  mutator->SetRateSeqSlip(config.MUT_RATE_SEQ_SLIP());
  mutator->SetRateFuncDup(config.MUT_RATE_FUNC_DUP());
  mutator->SetRateFuncDel(config.MUT_RATE_FUNC_DEL());
  mutator->SetRateFuncTagBF(config.MUT_RATE_FUNC_TAG_BF());
  mutator->SetRateInstTagSingleBF(config.MUT_RATE_INST_TAG_SINGLE_BF());
  mutator->SetRateFuncTagSingleBF(config.MUT_RATE_FUNC_TAG_SINGLE_BF());
  mutator->SetRateInstTagSeqRand(config.MUT_RATE_INST_TAG_SEQ_RAND());
  mutator->SetRateFuncTagSeqRand(config.MUT_RATE_FUNC_TAG_SEQ_RAND());

  // Set world mutation function
  SetMutFun(
    [this](org_t& org, emp::Random& rnd) {
      mutator->ResetLastMutationTracker(); // Reset mutator's recorded mutations.
      const size_t mut_cnt = mutator->ApplyAll(
        rnd,
        org.GetGenome().GetProgram()
      );
      org.GetGenome().IncAge(1);
      return mut_cnt;
    }
  );
}

// TODO - if using age, add non-performance criteria to all test groupings
void ProgSynthWorld::SetupEvaluation() {
  std::cout << "Configuring evaluation (mode: " << config.EVAL_MODE() << ")" << std::endl;
  if (org_groupings != nullptr) {
    org_groupings.Delete();
  }
  if (test_groupings != nullptr) {
    test_groupings.Delete();
  }

  org_groupings = emp::NewPtr<utils::GroupManager>(*random_ptr);
  test_groupings = emp::NewPtr<utils::GroupManager>(*random_ptr);

  // Total tests is equal to number of tests we loaded into the testing set.
  total_training_cases = problem_manager.GetTrainingSetSize();
  total_testing_cases = problem_manager.GetTestingSetSize();
  // Allocate space for tracking organism training coverage
  org_training_coverage.clear();
  org_training_coverage.resize(config.POP_SIZE(), 0);
  // Allocate space for tracking number of training cases an organism is evaluated against
  org_num_training_cases.clear();
  org_num_training_cases.resize(config.POP_SIZE(), 0);
  // Allocate space for tracking organism aggregate scores
  org_aggregate_scores.clear();
  org_aggregate_scores.resize(config.POP_SIZE(), 0.0);
  // Allocate space for tracking population-wide test coverage
  pop_training_coverage.clear();
  pop_training_coverage.resize(total_training_cases, false);
  // Allocate space for tracking organism test scores
  org_training_scores.clear();
  org_training_scores.resize(
    config.POP_SIZE(),
    emp::vector<double>(total_training_cases, 0.0)
  );
  // org_training_passes.clear();
  // org_training_passes.resize(
  //   config.POP_SIZE(),
  //   emp::BitVector(total_training_cases, false)
  // );
  // Allocate space for tracking organism test evaluations
  org_training_evaluations.clear();
  org_training_evaluations.resize(
    config.POP_SIZE(),
    emp::vector<bool>(total_training_cases, false)
  );

  // Create vector with all ids for population
  all_org_ids.resize(config.POP_SIZE());
  std::iota(
    all_org_ids.begin(),
    all_org_ids.end(),
    0
  );

  // Setup organism group manager.
  emp::vector<size_t> possible_org_ids(config.POP_SIZE(), 0);
  std::iota(
    possible_org_ids.begin(),
    possible_org_ids.end(),
    0
  );
  org_groupings->SetPossibleIDs(possible_org_ids);

  // Setup test group manager
  all_training_case_ids.resize(total_training_cases, 0);
  std::iota(
    all_training_case_ids.begin(),
    all_training_case_ids.end(),
    0
  );

  // TODO also configure shared evaluation ids
  test_groupings->SetPossibleIDs(all_training_case_ids);

  all_testing_case_ids.resize(total_testing_cases, 0);
  std::iota(
    all_testing_case_ids.begin(),
    all_testing_case_ids.end(),
    0
  );

  // Clear out all actions associated with organism evaluation.
  begin_org_evaluation_sig.Clear();
  do_org_evaluation_sig.Clear();
  end_org_evaluation_sig.Clear();

  begin_program_eval_sig.Clear();
  begin_program_test_sig.Clear();
  do_program_test_sig.Clear();
  end_program_test_sig.Clear();

  // Configure organism evaluation signals
  begin_org_evaluation_sig.AddAction(
    [this](size_t org_id) {
      // 0-out aggregate score
      org_aggregate_scores[org_id] = 0.0;
      // 0-out training coverage
      org_training_coverage[org_id] = 0;
      // 0-out num training cases evaluated against
      org_num_training_cases[org_id] = 0;
      // 0 out organism's training score
      std::fill(
        org_training_scores[org_id].begin(),
        org_training_scores[org_id].end(),
        0.0
      );

      // Reset phenotype
      auto& org = GetOrg(org_id);
      org.ResetPhenotype(total_training_cases);
    }
  );

  end_org_evaluation_sig.AddAction(
    [this](size_t org_id) {
      auto& org = GetOrg(org_id);

      // Record taxon information
      if (config.TRACK_PHYLOGENY()) {
        taxon_t& taxon = *(systematics_ptr->GetTaxonAt(org_id));
        taxon.GetData().RecordFitness(org.GetPhenotype().GetAggregateScore());
        taxon.GetData().RecordPhenotype(
          org.GetPhenotype().GetTestScores(),
          org_training_evaluations[org_id]
        );
      }

      // Check if candidate for testing against testing set
      // - Need to check if num_passes == number of tests evaluated on
      // - If so, add to vector of ids to be tested whether they are solutions
      // - Allow each evaluation mode to implement the actual testing
      if ((org_training_coverage[org_id] == org_num_training_cases[org_id]) && !found_solution) {
        found_solution = check_org_solution(org_id);
        solution_id = org_id;
      }

    }
  );

  begin_program_eval_sig.AddAction(
    [this](org_t& org) {
      // Load program onto evaluation hardware unit
      eval_hardware->SetProgram(org.GetGenome().GetProgram());
    }
  );

  begin_program_test_sig.AddAction(
    [this](org_t& org, size_t test_id, bool training) {
      eval_hardware->ResetMatchBin();      // Reset the matchbin between tests
      eval_hardware->ResetHardwareState(); // Reset hardware execution state information (global memory, threads, etc)
      eval_hardware->GetCustomComponent().Reset(); // Reset custom component
      // Load test input via problem manager
      problem_manager.InitCase(
        *eval_hardware,
        org,
        test_id,
        training
      );
      emp_assert(eval_hardware->ValidateThreadState());
    }
  );

  do_program_test_sig.AddAction(
    [this](org_t& org, size_t test_id) {
      emp_assert(eval_hardware->ValidateThreadState());
      // Step the hardware forward to process the input signal
      for (size_t step = 0; step < config.EVAL_CPU_CYCLES_PER_TEST(); ++step) {
        eval_hardware->SingleProcess();
        // Stop early if no active or pending threads
        const size_t num_active_threads = eval_hardware->GetNumActiveThreads();
        const size_t num_pending_threads = eval_hardware->GetNumPendingThreads();
        const bool stop_early = eval_hardware->GetCustomComponent().GetStopEval();
        if (!(num_active_threads || num_pending_threads) || stop_early) {
          break;
        }
      }
    }
  );

  end_program_test_sig.AddAction(
    [this](org_t& org, size_t test_id) {
      const size_t org_id = org.GetPopID();
      TestResult result = problem_manager.EvaluateOutput(
        *eval_hardware,
        org,
        test_id,
        true
      );
      // Record result on organism phenotype
      org.UpdatePhenotype(test_id, result);
      // World performance tracking
      org_training_scores[org_id][test_id] = result.score;
      org_training_evaluations[org_id][test_id] = true;
      org_training_coverage[org_id] += (size_t)result.is_correct;
      org_num_training_cases[org_id] += 1;
      pop_training_coverage[test_id] = pop_training_coverage[test_id] || result.is_correct;
    }
  );

  // Configure the aggregate score functions
  agg_score_fun_set.clear();
  for (size_t org_id = 0; org_id < config.POP_SIZE(); ++org_id) {
    agg_score_fun_set.emplace_back(
      [this, org_id]() {
        emp_assert(org_id < org_aggregate_scores.size());
        return org_aggregate_scores[org_id];
      }
    );
  }
  // std::cout << "Non perf shared ids: " << nonperf_fit_fun_shared_ids << std::endl;
  // Configure the fitness functions (per-organism, per-test)
  fit_fun_set.clear();
  fit_fun_set.resize(config.POP_SIZE(), emp::vector<std::function<double(void)>>(0));
  for (size_t org_id = 0; org_id < config.POP_SIZE(); ++org_id) {
    for (size_t test_id = 0; test_id < total_training_cases; ++test_id) {
      fit_fun_set[org_id].emplace_back(
        [this, org_id, test_id]() -> double {
          emp_assert(org_id < org_training_evaluations.size());
          emp_assert(org_id < org_training_scores.size());
          emp_assert(test_id < org_training_evaluations[org_id].size());
          emp_assert(test_id < org_training_scores[org_id].size());
          if (org_training_evaluations[org_id][test_id]) {
            return org_training_scores[org_id][test_id];
          } else {
            return 0.0;
          }
        }
      );
    }
    // // Configure non-performance ids
    // for (size_t nonperf_i = 0; nonperf_i < nonperf_fit_fun_shared_ids.size(); ++nonperf_i) {
    //   const size_t fit_id = fit_fun_set[org_id].size();
    //   fit_fun_set[org_id].emplace_back(
    //     [this, org_id, nonperf_i]() {
    //       return nonperf_fit_funs[nonperf_i](org_id);
    //     }
    //   );
    //   nonperformance_criteria_ids.emplace(fit_id);
    // }
  }


  // Setup evaluation mode
  // force_full_compete = false;
  if (config.EVAL_MODE() == "full") {
    SetupEvaluation_Full();
  } else if (config.EVAL_MODE() == "cohort") {
    SetupEvaluation_Cohort();
  } else if (config.EVAL_MODE() == "down-sample") {
    SetupEvaluation_DownSample();
  } else {
    std::cout << "Unknown EVAL_MODE: " << config.EVAL_MODE() << std::endl;
    exit(-1);
  }

  // Configure check to see if organism is a solution or not
  check_org_solution = [this](size_t org_id) {
    auto& org = GetOrg(org_id);
    begin_program_eval_sig.Trigger(org);
    for (size_t i = 0; i < all_testing_case_ids.size(); ++i) {
      const size_t test_id = all_testing_case_ids[i]; // Get the current test id
      // Handle test input:
      begin_program_test_sig.Trigger(org, test_id, false);
      // Run the program:
      do_program_test_sig.Trigger(org, test_id);
      // Manually check program output
      TestResult result = problem_manager.EvaluateOutput(
        *eval_hardware,
        org,
        test_id,
        false
      );
      if (!result.is_correct) {
        // Move the current test to the beginning
        if (i > test_order_barrier) {
          emp_assert(test_order_barrier < all_testing_case_ids.size());
          std::swap(all_testing_case_ids[i], all_testing_case_ids[test_order_barrier]);
          ++test_order_barrier;
        }
        return false;
      }
    }
    return true;
  };

}

void ProgSynthWorld::SetupEvaluation_Full() {
  std::cout << "Configuring evaluation mode: full" << std::endl;
  emp_assert(total_training_cases > 0);

  // Initialize the test groupings with one group that holds all tests.
  test_groupings->SetSingleGroupMode();
  // Initialize the organism groupings with one group that holds all organisms.
  org_groupings->SetSingleGroupMode();
  emp_assert(test_groupings->GetNumGroups() == org_groupings->GetNumGroups());

  // Configure organism evaluation
  do_org_evaluation_sig.AddAction(
    [this](size_t org_id) {
      emp_assert(org_id < GetSize());
      emp_assert(test_groupings->GetNumGroups() == 1);
      auto& org = GetOrg(org_id);
      begin_program_eval_sig.Trigger(org);
      const auto& test_group = test_groupings->GetGroup(0);
      const auto& test_ids = test_group.GetMembers();
      for (size_t i = 0; i < test_ids.size(); ++i) {
        const size_t test_id = test_ids[i]; // Get test id from group.
        // Handles test input:
        begin_program_test_sig.Trigger(org, test_id, true);
        // Runs the program:
        do_program_test_sig.Trigger(org, test_id);
        // Handles test output evaluation, updates phenotype:
        end_program_test_sig.Trigger(org, test_id);
        ++total_test_evaluations;
      }
    }
  );

}

void ProgSynthWorld::SetupEvaluation_Cohort() {
  std::cout << "Configuring evaluation mode: cohort" << std::endl;
  emp_assert(total_training_cases > 0);
  emp_assert(config.NUM_COHORTS() > 0);
  emp_assert(config.POP_SIZE() > 0);

  const size_t num_cohorts = config.NUM_COHORTS();
  // Configure test groupings
  test_groupings->SetCohortsMode(num_cohorts);
  std::cout << "Number of cohorts: " << num_cohorts << std::endl;
  std::cout << "Test cohorts:" << std::endl;
  for (size_t group_id = 0; group_id < test_groupings->GetNumGroups(); ++group_id) {
    std::cout << "  Test group " << group_id << " size: " << test_groupings->GetGroup(group_id).GetSize() << std::endl;
  }

  // Compute organism cohort sizes
  org_groupings->SetCohortsMode(num_cohorts);
  std::cout << "Organism cohorts:" << std::endl;
  for (size_t group_id = 0; group_id < org_groupings->GetNumGroups(); ++group_id) {
    std::cout << "  Org group " << group_id << " size: " << org_groupings->GetGroup(group_id).GetSize() << std::endl;
  }

  do_org_evaluation_sig.AddAction(
    [this](size_t org_id) {
      emp_assert(org_id < GetSize());
      auto& org = GetOrg(org_id);
      const size_t group_id = org_groupings->GetMemberGroupID(org_id);
      // Trigger begin program evaluation signal
      begin_program_eval_sig.Trigger(org);
      // Evaluate organism on all tests in appropriate group
      emp_assert(group_id < test_groupings->GetNumGroups());
      emp_assert(group_id < org_groupings->GetNumGroups());
      auto& test_group = test_groupings->GetGroup(group_id);
      const auto& cohort_test_ids = test_group.GetMembers();
      for (size_t i = 0; i < cohort_test_ids.size(); ++i) {
        const size_t test_id = cohort_test_ids[i];
        emp_assert(test_id < org.GetPhenotype().GetTestScores().size());
        begin_program_test_sig.Trigger(org, test_id, true);
        do_program_test_sig.Trigger(org, test_id);
        end_program_test_sig.Trigger(org, test_id);
        ++total_test_evaluations;
      }
    }
  );

}

void ProgSynthWorld::SetupEvaluation_DownSample() {
  std::cout << "Configuring evaluation mode: down-sample" << std::endl;
  emp_assert(config.TEST_DOWNSAMPLE_RATE() > 0);
  emp_assert(config.TEST_DOWNSAMPLE_RATE() <= 1.0);
  emp_assert(total_training_cases > 0);

  size_t sample_size = (size_t)(config.TEST_DOWNSAMPLE_RATE() * (double)total_training_cases);
  sample_size = (sample_size == 0) ? sample_size + 1 : sample_size;
  emp_assert(sample_size > 0);
  emp_assert(sample_size <= total_training_cases);

  std::cout << "Down-sample: sample_size = " << sample_size << std::endl;

  // Configure test groupings
  test_groupings->SetDownSampleMode(sample_size);
  std::cout << "Test groups (initial):" << std::endl;
  for (size_t group_id = 0; group_id < test_groupings->GetNumGroups(); ++group_id) {
    std::cout << "  Test group " << group_id << " size: " << test_groupings->GetGroup(group_id).GetSize() << std::endl;
  }

  // Configure organism groupings
  org_groupings->SetSingleGroupMode();

  // Configure organism evaluation
  do_org_evaluation_sig.AddAction(
    [this](size_t org_id) {
      emp_assert(org_id < GetSize());
      auto& org = GetOrg(org_id);
      begin_program_eval_sig.Trigger(org);
      const auto& test_group = test_groupings->GetGroup(0);
      const auto& test_ids = test_group.GetMembers();
      for (size_t i = 0; i < test_ids.size(); ++i) {
        const size_t test_id = test_ids[i]; // Test test id from group
        // Handles test input:
        begin_program_test_sig.Trigger(org, test_id, true);
        // Runs the program:
        do_program_test_sig.Trigger(org, test_id);
        // Handles test output evaluation, updates phenotype:
        end_program_test_sig.Trigger(org, test_id);
        ++total_test_evaluations;
      }
    }
  );
}

void ProgSynthWorld::SetupSelection() {
  std::cout << "Configuring parent selection routine" << std::endl;
  // TODO - Can I have different worlds share selection routine setups?
  emp_assert(selector == nullptr);

  run_selection_routine = [this]() {
    // Resize parent ids to hold pop_size parents
    selected_parent_ids.resize(num_to_select, 0);
    emp_assert(test_groupings->GetNumGroups() == org_groupings->GetNumGroups());
    const size_t num_groups = org_groupings->GetNumGroups();
    // For each grouping, select a number of parents equal to group size
    size_t num_selected = 0;
    for (size_t group_id = 0; group_id < num_groups; ++group_id) {
      auto& org_group = org_groupings->GetGroup(group_id);
      auto& test_group = test_groupings->GetGroup(group_id);
      // Adjust to handle fact that might be selecting < pop size
      emp_assert(num_to_select >= num_selected);
      const size_t n = (num_selected + org_group.GetSize() > num_to_select) ?
        num_to_select - num_selected :
        org_group.GetSize();
      if (n == 0 && num_selected == num_to_select) {
        break;
      } else if (n == 0) {
        continue;
      }
      auto& selected = selection_fun(
        n,
        org_group.GetMembers(),
        test_group.GetMembers()
      );
      emp_assert(selected.size() == n);
      emp_assert(n + num_selected <= selected_parent_ids.size());
      std::copy(
        selected.begin(),
        selected.end(),
        selected_parent_ids.begin() + num_selected
      );
      num_selected += n;
    }
    emp_assert(num_selected == num_to_select);
  };

  if (config.SELECTION() == "lexicase" ) {
    SetupSelection_Lexicase();
  } else if (config.SELECTION() == "age-lexicase") {
    SetupSelection_AgeLexicase();
  } else if (config.SELECTION() == "tournament" ) {
    SetupSelection_Tournament();
  } else if (config.SELECTION() == "truncation" ) {
    SetupSelection_Truncation();
  } else if (config.SELECTION() == "none" ) {
    SetupSelection_None();
  } else if (config.SELECTION() == "random" ) {
    SetupSelection_Random();
  } else {
    std::cout << "Unknown selection scheme: " << config.SELECTION() << std::endl;
    exit(-1);
  }
}

void ProgSynthWorld::SetupSelection_Lexicase() {

  selector = emp::NewPtr<selection::LexicaseSelect>(
    fit_fun_set,
    *random_ptr
  );

  selection_fun = [this](
    size_t n,
    const emp::vector<size_t>& org_group,
    const emp::vector<size_t>& test_group
  ) -> emp::vector<size_t>& {
    // Cast selector to lexicase selection
    auto& sel = *(selector.Cast<selection::LexicaseSelect>());
    return sel(n, org_group, test_group);
  };

}

void ProgSynthWorld::SetupSelection_AgeLexicase() {

  nonperf_fit_funs.clear();
  nonperf_fit_funs.resize(
    config.POP_SIZE(),
    emp::vector<std::function<double(void)>>(0)
  );

  // Add genome age as a training case
  for (size_t org_id = 0; org_id < config.POP_SIZE(); ++org_id) {
    nonperf_fit_funs[org_id].emplace_back(
      [this, org_id]() -> double {
        return -1.0 * (double)GetOrg(org_id).GetGenome().GetAge();
      }
    );
  }

  selector = emp::NewPtr<selection::AgeLexicaseSelect>(
    fit_fun_set,
    nonperf_fit_funs,
    *random_ptr
  );

  auto& sel = *(selector.Cast<selection::AgeLexicaseSelect>());
  sel.SetAgeFunOrderLimit(config.AGE_LEX_AGE_ORDER_LIMIT());

  selection_fun = [this](
    size_t n,
    const emp::vector<size_t>& org_group,
    const emp::vector<size_t>& test_group
  ) -> emp::vector<size_t>& {
    // Cast selector to lexicase selection
    auto& sel = *(selector.Cast<selection::AgeLexicaseSelect>());
    return sel(n, org_group, test_group);
  };

}

void ProgSynthWorld::SetupSelection_Tournament() {
  selector = emp::NewPtr<selection::TournamentSelect>(
    agg_score_fun_set,
    *random_ptr,
    config.TOURNAMENT_SIZE()
  );

  selection_fun = [this](
    size_t n,
    const emp::vector<size_t>& org_group,
    const emp::vector<size_t>& test_group
  ) -> emp::vector<size_t>& {
    // Cast selector to lexicase selection
    auto& sel = *(selector.Cast<selection::TournamentSelect>());
    return sel(n, org_group);
  };
}

void ProgSynthWorld::SetupSelection_Truncation() {
  // TODO - trunction selection
  emp_assert(false);
}

void ProgSynthWorld::SetupSelection_None() {
  // TODO - No selection
  emp_assert(false);
}

void ProgSynthWorld::SetupSelection_Random() {
  // TODO - Random selection
  emp_assert(false);
}

void ProgSynthWorld::SetupOrgInjection() {
  std::cout << "Configuring organism recombination mode: " << config.ORG_INJECTION_MODE() << std::endl;

  // Functions to configure:
  // - injectorgs
  // - calc_next_gen_sources

  if (config.ORG_INJECTION_MODE() == "none") {
    SetupOrgInjection_None();
  } else if (config.ORG_INJECTION_MODE() == "random") {
    SetupOrgInjection_Random();
  } else if (config.ORG_INJECTION_MODE() == "recombine-random") {
    SetupOrgInjection_RecombineRandom();
  } else if (config.ORG_INJECTION_MODE() == "recombine-complement") {
    SetupOrgInjection_RecombineComplementary();
  } else {
    std::cout << "Unknown ORG_INJECTION_MODE: " << config.ORG_INJECTION_MODE() << std::endl;
    exit(-1);
  }
}

void ProgSynthWorld::SetupOrgInjection_None() {
  calc_next_gen_sources = [this]() {
    num_to_inject = 0;
    num_to_select = config.POP_SIZE();
  };

  inject_orgs = [this]() {
    return;
  };
}

void ProgSynthWorld::SetupOrgInjection_Random() {
  calc_next_gen_sources = [this]() {
    emp_assert(config.ORG_INJECTION_COUNT() < config.POP_SIZE());
    // Default to 0 inject, select pop size
    num_to_inject = 0;
    num_to_select = config.POP_SIZE();
    const size_t cur_update = GetUpdate();
    // if update 0, don't perform injection
    if (cur_update == 0) return;
    const size_t interval = config.ORG_INJECTION_INTERVAL();
    if ((cur_update % interval) == 0) {
      num_to_inject = config.ORG_INJECTION_COUNT();
      num_to_select = config.POP_SIZE() - num_to_inject;
    }
    emp_assert(num_to_inject + num_to_select == config.POP_SIZE());
  };

   inject_orgs = [this]() {
    // std::cout << "Num pops: " << pops.size() << std::endl;
    for (size_t i = 0; i < num_to_inject; ++i) {
      // Inject in 'next pop' at end of pop vector
      // emp::WorldPosition inject_pos(pops[1].size(), 1);
      InjectAt(
        {
          sgp::cpu::lfunprg::GenRandLinearFunctionsProgram<hardware_t, TAG_SIZE>(
            *random_ptr,
            inst_lib,
            {config.PRG_MIN_FUNC_CNT(), config.PRG_MAX_FUNC_CNT()},
            FUNC_NUM_TAGS,
            {config.PRG_MIN_FUNC_INST_CNT(), config.PRG_MAX_FUNC_INST_CNT()},
            INST_TAG_CNT,
            INST_ARG_CNT,
            {config.PRG_INST_MIN_ARG_VAL(), config.PRG_INST_MAX_ARG_VAL()}
          )
        },
        {pops[1].size(), 1}
      );
    }
  };
}

void ProgSynthWorld::SetupOrgInjection_RecombineRandom() {
  // Configure recombiner
  recombiner.SetFuncSeqCrossoverRate(config.RECOMB_PER_FUNC_SEQ_RECOMB_RATE());

  calc_next_gen_sources = [this]() {
    emp_assert(config.ORG_INJECTION_COUNT() < config.POP_SIZE());
    // Default to 0 inject, select pop size
    num_to_inject = 0;
    num_to_select = config.POP_SIZE();
    const size_t cur_update = GetUpdate();
    // if update 0, don't perform injection
    if (cur_update == 0) return;
    const size_t interval = config.ORG_INJECTION_INTERVAL();
    if ((cur_update % interval) == 0) {
      num_to_inject = config.ORG_INJECTION_COUNT();
      num_to_select = config.POP_SIZE() - num_to_inject;
    }
  };

  // Limit to selected?

  // Or pure random?

  inject_orgs = [this]() {
    // https://stackoverflow.com/questions/2288171/how-to-get-2-random-different-elements-from-a-c-vector
    size_t num_injected = 0;
    while (num_injected < num_to_inject) {
      size_t pop_id_1 = random_ptr->GetUInt(GetSize() - 1);
      size_t pop_id_2 = random_ptr->GetUInt(GetSize());
      if (pop_id_1 == pop_id_2) {
        pop_id_2 = GetSize() - 1;
      }
      // Make two new genomes to work with
      genome_t genome_1(GetOrg(pop_id_1).GetGenome());
      genome_t genome_2(GetOrg(pop_id_2).GetGenome());
      // Recombine!
      recombiner.ApplyFunctionSequenceRecombinationTwoPoint(
        *random_ptr,
        genome_1.GetProgram(),
        genome_2.GetProgram()
      );
      genome_1.SetAge(0);
      genome_2.SetAge(0);
      InjectAt(genome_1, {pops[1].size(), 1});
      ++num_injected;
      if (num_injected < num_to_inject) {
        InjectAt(genome_2, {pops[1].size(), 1});
        ++num_injected;
      }
    }

  };
}

void ProgSynthWorld::SetupOrgInjection_RecombineComplementary() {
  // Configure recombiner
  recombiner.SetFuncSeqCrossoverRate(config.RECOMB_PER_FUNC_SEQ_RECOMB_RATE());

  calc_next_gen_sources = [this]() {
    emp_assert(config.ORG_INJECTION_COUNT() < config.POP_SIZE());
    // Default to 0 inject, select pop size
    num_to_inject = 0;
    num_to_select = config.POP_SIZE();
    const size_t cur_update = GetUpdate();
    // if update 0, don't perform injection
    if (cur_update == 0) return;
    const size_t interval = config.ORG_INJECTION_INTERVAL();
    if ((cur_update % interval) == 0) {
      num_to_inject = config.ORG_INJECTION_COUNT();
      num_to_select = config.POP_SIZE() - num_to_inject;
    }
  };

  // Limit to selected?

  // Or pure random?

  inject_orgs = [this]() {
    // (1) Analyze population
    emp::vector<PhenUnionInfo> phen_pairs;
    std::unordered_set<emp::BitVector> unique_phen_unions;
    // Union phenotypes of all possible pairings
    for (size_t i = 0; i < GetSize(); ++i) {
      for (size_t j = i+1; j < GetSize(); ++j) {
        // TODO - optimize
        emp::BitVector union_phen(GetOrg(i).GetPhenotype().GetTestPasses().OR(GetOrg(j).GetPhenotype().GetTestPasses()));
        if (emp::Has(unique_phen_unions, union_phen)) {
          continue;
        }
        phen_pairs.emplace_back(
          i,
          GetOrg(i).GetPhenotype().GetTestPasses(),
          j,
          GetOrg(j).GetPhenotype().GetTestPasses()
        );
        unique_phen_unions.emplace(union_phen);
      }
    }
    // std::cout << "InjectOrgs" << std::endl;
    // std::cout << "phen_pairs.size(): " << phen_pairs.size() << std::endl;
    // TODO - filter phen_pairs to just unique unions?
    FindParetoFrontPairs(phen_pairs);

    // std::cout << "front ("<< front_pair_ids.size() << "): " << std::endl; // << front_pair_ids << std::endl;

    emp_assert(front_pair_ids.size() > 0);
    size_t num_injected = 0;
    while (num_injected < num_to_inject) {
      // Select random pairing on pareto front
      const size_t front_id = random_ptr->GetUInt(front_pair_ids.size());
      emp_assert(front_id < phen_pairs.size());
      const size_t pop_id_1 = phen_pairs[front_id].org_ids.first;
      const size_t pop_id_2 = phen_pairs[front_id].org_ids.second;

      // Make two new genomes to work with
      genome_t genome_1(GetOrg(pop_id_1).GetGenome());
      genome_t genome_2(GetOrg(pop_id_2).GetGenome());
      // Recombine!
      recombiner.ApplyFunctionSequenceRecombinationTwoPoint(
        *random_ptr,
        genome_1.GetProgram(),
        genome_2.GetProgram()
      );
      genome_1.SetAge(0);
      genome_2.SetAge(0);
      InjectAt(genome_1, {pops[1].size(), 1});
      ++num_injected;
      if (num_injected < num_to_inject) {
        InjectAt(genome_2, {pops[1].size(), 1});
        ++num_injected;
      }
    }

  };
}

void ProgSynthWorld::SetupStoppingCondition() {
  std::cout << "Configuring stopping condition" << std::endl;
  if (config.STOP_MODE() == "generations") {
    SetupStoppingCondition_Generations();
  } else if (config.STOP_MODE() == "evaluations") {
    SetupStoppingCondition_Evaluations();
  } else {
    std::cout << "Unknown STOP_MODE: " << config.STOP_MODE() << std::endl;
    exit(-1);
  }
}

void ProgSynthWorld::SetupStoppingCondition_Generations() {
  stop_run = [this]() {
    return (GetUpdate() > config.MAX_GENS()) || found_solution;
  };
  is_final_update = [this]() {
    return (GetUpdate() >= config.MAX_GENS()) || found_solution;
  };
}

void ProgSynthWorld::SetupStoppingCondition_Evaluations() {
  stop_run = [this]() {
    return (total_test_evaluations > config.MAX_EVALS()) || found_solution;
  };
  is_final_update = [this]() {
    return (total_test_evaluations >= config.MAX_EVALS()) || found_solution;
  };
}

void ProgSynthWorld::InitializePopulation() {
  std::cout << "Initializing the population." << std::endl;
  // Clear the current population
  Clear();
  // Initialize population according to configuration
  if (config.POP_INIT_MODE() == "random") {
    InitializePopulation_Random();
  } else if (config.POP_INIT_MODE() == "load-single") {
    InitializePopulation_LoadSingle();
  } else {
    std::cout << "Unknown POP_INIT_MODE: " << config.POP_INIT_MODE() << std::endl;
    exit(-1);
  }
}

void ProgSynthWorld::InitializePopulation_LoadSingle() {
  std::ifstream prg_fstream(config.ANCESTOR_FILE_PATH());
  if (!prg_fstream.is_open()) {
    std::cout << "Failed to open ancestor file: " << config.ANCESTOR_FILE_PATH() << std::endl;
    exit(-1);
  }
  // Load ancestor program from file (print format)
  program_t ancestor = LoadLinearFunctionsProgram_PrintFormat<inst_lib_t, TAG_SIZE>(
    prg_fstream,
    inst_lib
  );
  // Inject POP_SIZE number of copies of loaded program into population.
  for (size_t i = 0; i < config.POP_SIZE(); ++i) {
    Inject({ancestor});
  }
}

void ProgSynthWorld::InitializePopulation_Random() {
  for (size_t i = 0; i < config.POP_SIZE(); ++i) {
    Inject(
      {
        sgp::cpu::lfunprg::GenRandLinearFunctionsProgram<hardware_t, TAG_SIZE>(
          *random_ptr,
          inst_lib,
          {config.PRG_MIN_FUNC_CNT(), config.PRG_MAX_FUNC_CNT()},
          FUNC_NUM_TAGS,
          {config.PRG_MIN_FUNC_INST_CNT(), config.PRG_MAX_FUNC_INST_CNT()},
          INST_TAG_CNT,
          INST_ARG_CNT,
          {config.PRG_INST_MIN_ARG_VAL(), config.PRG_INST_MAX_ARG_VAL()}
        )
      }
    );
  }
}

void ProgSynthWorld::SetupPhylogenyTracking() {
  if (config.MUTATION_ANALYSIS_MODE() || !config.TRACK_PHYLOGENY()) {
    return;
  }

  std::cout << "Configure phylogeny tracking" << std::endl;
  emp_assert(systematics_ptr == nullptr);
  // Create new systematics tracker
  systematics_ptr = emp::NewPtr<systematics_t>(
    [](const org_t& org) {
      return org.GetGenome();
    }
  );

  // Add phylogeny snapshot functions
  // Fitness (aggregate score)
  systematics_ptr->AddSnapshotFun(
    [](const taxon_t& taxon) {
      return emp::to_string(taxon.GetData().GetFitness());
    },
    "fitness"
  );

  // Phenotype
  systematics_ptr->AddSnapshotFun(
    [](const taxon_t& taxon) {
      std::stringstream ss;
      utils::PrintVector(ss, taxon.GetData().GetPhenotype(), true);
      return ss.str();
    },
    "phenotype"
  );

  // -- taxon estimation information --
  // traits_evaluated
  systematics_ptr->AddSnapshotFun(
    [this](const taxon_t& taxon) {
      std::stringstream ss;
      utils::PrintVector(ss, taxon.GetData().traits_evaluated, true);
      return ss.str();
    },
    "training_cases_evaluated"
  );

  // True scores on training cases
  systematics_ptr->AddSnapshotFun(
    [this](const taxon_t& taxon) -> std::string {
      emp_assert(taxon.GetData().true_training_scores_computed);
      if (taxon.GetData().true_training_scores.size() == 0) {
        return "\"[]\"";
      }
      std::stringstream ss;
      utils::PrintVector(ss, taxon.GetData().true_training_scores, true);
      return ss.str();
    },
    "training_cases_true_scores"
  );

  systematics_ptr->AddSnapshotFun(
    [this](const taxon_t& taxon) -> std::string {
      emp_assert(taxon.GetData().true_training_scores_computed);
      return emp::to_string(taxon.GetData().true_agg_score);
    },
    "true_agg_score"
  );

  systematics_ptr->AddEvolutionaryDistinctivenessDataNode();
  systematics_ptr->AddPairwiseDistanceDataNode();
  systematics_ptr->AddPhylogeneticDiversityDataNode();

  AddSystematics(systematics_ptr, "genotype");
  SetupSystematicsFile(
    "genotype",
    output_dir + "systematics.csv"
  ).SetTimingRepeat(config.OUTPUT_SUMMARY_DATA_INTERVAL());

}

void ProgSynthWorld::SetupDataCollection() {
  if (config.MUTATION_ANALYSIS_MODE()) {
    return;
  }
  std::cout << "Configure data tracking" << std::endl;
  if (config.TRACK_PHYLOGENY()) {
    SetupDataCollection_Phylodiversity();
  }
  SetupDataCollection_Summary();
  SetupDataCollection_Elite();
}

void ProgSynthWorld::SetupDataCollection_Phylodiversity() {
  emp_assert(config.TRACK_PHYLOGENY());
  // Create phylodiversity file
  phylodiversity_file_ptr = emp::NewPtr<emp::DataFile>(
    output_dir + "phylodiversity.csv"
  );

  phylodiversity_file_ptr->AddVar(update, "update", "Generation");
  phylodiversity_file_ptr->AddVar(total_test_evaluations, "evaluations", "Test evaluations so far");
  phylodiversity_file_ptr->AddStats(*systematics_ptr->GetDataNode("evolutionary_distinctiveness") , "genotype_evolutionary_distinctiveness", "evolutionary distinctiveness for a single update", true, true);
  phylodiversity_file_ptr->AddStats(*systematics_ptr->GetDataNode("pairwise_distance"), "genotype_pairwise_distance", "pairwise distance for a single update", true, true);
  phylodiversity_file_ptr->AddCurrent(*systematics_ptr->GetDataNode("phylogenetic_diversity"), "genotype_current_phylogenetic_diversity", "current phylogenetic_diversity", true, true);
  phylodiversity_file_ptr->PrintHeaderKeys();
}

void ProgSynthWorld::SetupDataCollection_Summary() {
  // Create summary file
  summary_file_ptr = emp::NewPtr<emp::DataFile>(
    output_dir + "summary.csv"
  );
  // update
  summary_file_ptr->AddVar(update, "update", "Generation");
  // total training cases
  summary_file_ptr->AddVar(
    total_test_evaluations,
    "evaluations",
    "Test evaluations so far"
  );

  summary_file_ptr->AddVar(
    found_solution,
    "found_solution",
    "Did we find a solution?"
  );
  // pop-wide trait coverage
  summary_file_ptr->AddVar(
    pop_num_training_cases_covered,
    "pop_training_coverage",
    "True population-wide training case coverage"
  );
  // approx max aggregate score
  summary_file_ptr->AddFun<double>(
    [this]() -> double {
      return max_fit;
    },
    "max_approx_agg_score",
    "Maximum approximated aggregate score"
  );
  // num unique selected
  summary_file_ptr->AddVar(
    selection_stats.num_unique_cand_selected,
    "num_unique_selected",
    "Number of unique candidates selected to be parents"
  );
  // entropy selected ids
  summary_file_ptr->AddVar(
    selection_stats.entropy_cand_selected,
    "entropy_selected_ids",
    "Entropy of candidate IDs selected"
  );
  // parents trait coverage
  summary_file_ptr->AddVar(
    selection_stats.parents_num_tests_covered,
    "parents_training_coverage",
    "True training case coverage of selected parents"
  );
  // trait coverage loss
  summary_file_ptr->AddFun<size_t>(
    [this]() {
      emp_assert(pop_num_training_cases_covered >= selection_stats.parents_num_tests_covered);
      return pop_num_training_cases_covered - selection_stats.parents_num_tests_covered;
    },
    "training_coverage_loss",
    "(true) population training coverage - (true) parent training coverage"
  );
  // Average genome age of selected
  summary_file_ptr->AddFun<double>(
    [this]() -> double {
      emp_assert(selection_stats.parent_genome_ages.size() > 0);
      return emp::Mean(selection_stats.parent_genome_ages);
    },
    "mean_age_selected"
  );

  summary_file_ptr->PrintHeaderKeys();
}

void ProgSynthWorld::SetupDataCollection_Elite() {
  // Create elite file
  elite_file_ptr = emp::NewPtr<emp::DataFile>(
    output_dir + "elite.csv"
  );

  elite_file_ptr->AddVar(update, "update", "Generation");
  elite_file_ptr->AddVar(
    total_test_evaluations,
    "evaluations",
    "Test evaluations so far"
  );
  // ID
  elite_file_ptr->AddVar(
    max_fit_id,
    "elite_id",
    "Population ID of the elite organism"
  );

  elite_file_ptr->AddFun<size_t>(
    [this]() -> size_t {
      const auto& org = GetOrg(max_fit_id);
      return org.GetGenome().GetAge();
    },
    "elite_age"
  );

  // True aggregate fitness
  elite_file_ptr->AddFun<double>(
    [this]() -> double {
      const auto& org = GetOrg(max_fit_id);
      return org.GetPhenotype().GetAggregateScore();
    },
    "eval_agg_score",
    "Elite organism's evaluated aggregate score"
  );
  // org_training_coverage
  elite_file_ptr->AddFun<size_t>(
    [this]() -> size_t {
      return org_training_coverage[max_fit_id];
    },
    "eval_training_coverage",
    "Coverage on evaluated training cases"
  );
  // org_num_training_cases
  elite_file_ptr->AddFun<size_t>(
    [this]() -> size_t {
      return org_num_training_cases[max_fit_id];
    },
    "num_training_cases_evaluated",
    "Number of training cases that this organism was evaluated against"
  );
  // Org training cases evaluated on
  elite_file_ptr->AddFun<std::string>(
    [this]() -> std::string {
      std::stringstream ss;
      utils::PrintVector(
        ss,
        org_training_evaluations[max_fit_id],
        true
      );
      return ss.str();
    },
    "org_training_evaluations",
    "Training cases this organism was evaluated against"
  );
  // Approximate training scores
  elite_file_ptr->AddFun<std::string>(
    [this]() -> std::string {
      std::stringstream ss;
      utils::PrintVector(
        ss,
        org_training_scores[max_fit_id],
        true
      );
      return ss.str();
    },
    "approx_training_scores",
    "Scores on training cases (approximate where not evaluated)"
  );
  // Number of instructions (total)
  elite_file_ptr->AddFun<size_t>(
    [this]() -> size_t {
      auto& org = GetOrg(max_fit_id);
      return org.GetGenome().GetProgram().GetInstCount();
    },
    "num_instructions",
    "Number of instructions in organism genome"
  );
  // Num functions
  elite_file_ptr->AddFun<size_t>(
    [this]() -> size_t {
      auto& org = GetOrg(max_fit_id);
      return org.GetGenome().GetProgram().GetSize();
    },
    "num_functions",
    "Number of functions in organism genome"
  );

  // elite_file_ptr->AddFun<std::string>(
  //   [this]() -> std::string {
  //     std::stringstream ss;
  //     auto& org = GetOrg(approx_max_fit_id);
  //     ss << "\"";
  //     PrintProgramJSON(
  //       ss,
  //       org.GetGenome().GetProgram(),
  //       inst_lib
  //     );
  //     ss << "\"";
  //     return ss.str();
  //   },
  //   "genome",
  //   "Organism program (JSON format)"
  // );

  elite_file_ptr->PrintHeaderKeys();
}

void ProgSynthWorld::SnapshotConfig() {
  emp::DataFile snapshot_file(output_dir + "run_config.csv");
  std::function<std::string(void)> get_param;
  std::function<std::string(void)> get_value;

  snapshot_file.AddFun<std::string>(
    [&get_param]() { return get_param(); },
    "parameter"
  );
  snapshot_file.AddFun<std::string>(
    [&get_value]() { return get_value(); },
    "value"
  );
  snapshot_file.PrintHeaderKeys();

  // Snapshot everything from config file
  for (const auto& entry : config) {
    get_param = [&entry]() { return entry.first; };
    get_value = [&entry]() { return emp::to_string(entry.second->GetValue()); };
    snapshot_file.Update();
  }
  // Snapshot misc. other details
  emp::vector<std::pair<std::string, std::string>> misc_params = {
    std::make_pair("TAG_SIZE", emp::to_string(TAG_SIZE)),
    std::make_pair("FUNC_NUM_TAGS", emp::to_string(FUNC_NUM_TAGS)),
    std::make_pair("INST_TAG_CNT", emp::to_string(INST_TAG_CNT)),
    std::make_pair("INST_ARG_CNT", emp::to_string(INST_ARG_CNT)),
    std::make_pair("sgp_program_type", "LinearFunctionsProgram"),
    std::make_pair("matchbin_metric", "HammingMetric"),
    std::make_pair("matchbin_selector", "RankedSelector"),
    std::make_pair("total_training_cases", emp::to_string(problem_manager.GetTrainingSetSize())),
    std::make_pair("total_testing_cases", emp::to_string(problem_manager.GetTestingSetSize()))
  };

  for (const auto& entry : misc_params) {
    get_param = [&entry]() { return entry.first; };
    get_value = [&entry]() { return entry.second; };
    snapshot_file.Update();
  }

  // Snapshot the instruction set
  get_param = []() { return "instruction_set"; };
  get_value = [this]() {
    std::stringstream ss;
    ss << "\"[";
    for (size_t i = 0; i < inst_lib.GetSize(); ++i) {
      if (i) ss << ",";
      ss << inst_lib.GetName(i);
    }
    ss << "]\"";
    return ss.str();
  };
  snapshot_file.Update();

}

void ProgSynthWorld::SnapshotSolution() {
  std::ofstream outfile;
  outfile.open(output_dir + "solution.sgp");
  emp_assert(found_solution);
  auto& solution = GetOrg(solution_id);
  solution.GetGenome().GetProgram().Print(
    outfile,
    inst_lib
  );
  outfile.close();
}

void ProgSynthWorld::SnapshotPhylogeny() {
  emp_assert(config.TRACK_PHYLOGENY());
  emp_assert(systematics_ptr != nullptr);

  const auto& active_taxa = systematics_ptr->GetActive();
  const auto& ancestor_taxa = systematics_ptr->GetAncestors();
  const auto& outside_taxa = systematics_ptr->GetOutside();
  std::unordered_set<size_t> ids_output;
  // Consolidate active/ancestor/outside taxa pointers into a single vector
  auto phylo_taxa(active_taxa);
  for (const emp::Ptr<taxon_t>& taxon : ancestor_taxa) {
    phylo_taxa.insert(taxon);
  }
  for (const emp::Ptr<taxon_t>& taxon : outside_taxa) {
    phylo_taxa.insert(taxon);
  }

  // Evaluate all taxa on full training set (if they haven't already been evaluated)
  for (const emp::Ptr<taxon_t>& taxon : phylo_taxa) {
    const size_t taxon_id = taxon->GetID();
    // If already seen this taxon, skip.
    if (emp::Has(ids_output, taxon_id)) continue;
    ids_output.emplace(taxon_id);
    // If already computed taxon scores, skip.
    if (taxon->GetData().true_training_scores_computed) continue;
    // Need to evaluate taxon on full training set
    const auto& genome = taxon->GetInfo();
    // Build new org.
    org_t org(genome);
    org.ResetPhenotype(problem_manager.GetTrainingSetSize());
    auto& scores = taxon->GetData().true_training_scores;
    scores.resize(problem_manager.GetTrainingSetSize(), 0.0);
    begin_program_eval_sig.Trigger(org);
    // Evaluate org on each training case.
    for (size_t i = 0; i < all_training_case_ids.size(); ++i) {
      const size_t training_id = all_training_case_ids[i];
      // Handle training input
      begin_program_test_sig.Trigger(org, training_id, true);
      // Run the program
      do_program_test_sig.Trigger(org, training_id);
      // Check program output
      TestResult result = problem_manager.EvaluateOutput(
        *eval_hardware,
        org,
        training_id,
        true
      );
      org.UpdatePhenotype(training_id, result);
      scores[training_id] = result.score;
    }
    taxon->GetData().true_training_scores_computed = true;
    taxon->GetData().true_agg_score = org.GetPhenotype().GetAggregateScore();
  }

  systematics_ptr->Snapshot(output_dir + "phylo_" + emp::to_string(GetUpdate()) + ".csv");
  if (config.RECORD_PHYLO_GENOTYPES()) {
    SnapshotPhyloGenotypes();
  }
}

void ProgSynthWorld::SnapshotPhyloGenotypes() {
  std::ofstream outfile;
  outfile.open(output_dir + "phylo_genotypes_" + emp::to_string(GetUpdate()) + ".sgp");

  const auto& active_taxa = systematics_ptr->GetActive();
  const auto& ancestor_taxa = systematics_ptr->GetAncestors();
  const auto& outside_taxa = systematics_ptr->GetOutside();
  std::unordered_set<size_t> ids_output;

  // Write active taxa genotypes
  for (const emp::Ptr<taxon_t>& taxon : active_taxa) {
    const size_t taxon_id = taxon->GetID();
    if (emp::Has(ids_output, taxon_id)) continue;
    outfile << "!" << taxon_id << "\n";
    taxon->GetInfo().GetProgram().Print(
      outfile,
      inst_lib
    );
    outfile << "\n";
    ids_output.emplace(taxon_id);
  }

  // Write ancestor taxa genotypes
  for (const emp::Ptr<taxon_t>& taxon : ancestor_taxa) {
    const size_t taxon_id = taxon->GetID();
    if (emp::Has(ids_output, taxon_id)) continue;
    outfile << "!" << taxon_id << "\n";
    taxon->GetInfo().GetProgram().Print(
      outfile,
      inst_lib
    );
    outfile << "\n";
    ids_output.emplace(taxon_id);
  }

  // Write outside taxa genotypes
  for (const emp::Ptr<taxon_t>& taxon : outside_taxa) {
    const size_t taxon_id = taxon->GetID();
    if (emp::Has(ids_output, taxon_id)) continue;
    outfile << "!" << taxon_id << "\n";
    taxon->GetInfo().GetProgram().Print(
      outfile,
      inst_lib
    );
    outfile << "\n";
    ids_output.emplace(taxon_id);
  }
}

void ProgSynthWorld::RunMutationAnalysis() {
  std::cout << "Running a mutation analysis on the focal genotypes." << std::endl;

  std::string genotype_id = ""; // Tracks current focal genotype
  size_t mutant_id = 0;   // Tracks current mutant id

  ////////////////////////////////////////////////
  // Configure output
  ////////////////////////////////////////////////
  emp::DataFile mutant_file(output_dir + "mutant_analysis.csv");
  mutant_file.AddVar(genotype_id, "genotype_id");
  mutant_file.AddVar(mutant_id, "mutant_id");
  mutant_file.AddFun<bool>(
    [this, &mutant_id]() {
      return mutant_id == 0;
    },
    "is_original"
  );
  mutant_file.AddFun<double>(
    [this, &mutant_id]() {
      return org_aggregate_scores[mutant_id];
    },
    "agg_score"
  );
  mutant_file.AddFun<size_t>(
    [this, &mutant_id]() {
      return org_training_coverage[mutant_id];
    },
    "training_case_coverage"
  );
  mutant_file.AddFun<std::string>(
    [this, &mutant_id]() -> std::string {
      std::stringstream ss;
      utils::PrintVector(
        ss,
        org_training_scores[mutant_id],
        true
      );
      return ss.str();
    },
    "training_case_scores"
  );
  mutant_file.AddFun<size_t>(
    [this, &mutant_id]() {
      auto& org = GetOrg(mutant_id);
      return org.GetGenome().GetProgram().GetInstCount();
    },
    "num_instructions"
  );
  mutant_file.AddFun<size_t>(
    [this, &mutant_id]() {
      auto& org = GetOrg(mutant_id);
      return org.GetGenome().GetProgram().GetSize();
    },
    "num_functions"
  );
  mutant_file.PrintHeaderKeys();


  ////////////////////////////////////////////////
  // Load the focal organism(s)
  ////////////////////////////////////////////////
  std::ifstream prg_fstream(config.FOCAL_GENOTYPES_FPATH());
  if (!prg_fstream.is_open()) {
    std::cout << "Failed to open focal genotypes file: " << config.FOCAL_GENOTYPES_FPATH() << std::endl;
    exit(-1);
  }
  emp::vector<
    std::pair<program_t, std::string>
  > focal_programs = LoadLinearFunctionsPrograms_PrintFormat<inst_lib_t, TAG_SIZE>(
    prg_fstream,
    inst_lib
  );

  std::cout << "Generating " << config.NUM_MUTANTS() << " mutants for " << focal_programs.size() << " genotypes." << std::endl;

  // Run analysis on each focal genotype
  for (size_t prog_i = 0; prog_i < focal_programs.size(); ++prog_i) {
    std::cout << "  Analyzing genotype " << prog_i << " / " << focal_programs.size() - 1 << std::endl;
    genotype_id = focal_programs[prog_i].second;
    // Clear out existing population.
    Clear();
    emp_assert(GetSize() == 0);

    auto& focal_program = focal_programs[prog_i].first;

    // Inject focal program into population at position 0.
    Inject({focal_program});

    // Ensure uniqueness.
    std::set<program_t> mutants({focal_program});

    ////////////////////////////////////////////////
    // Generate N unique mutants
    ////////////////////////////////////////////////
    // (<= because focal program included in mutants)
    while (mutants.size() <= config.NUM_MUTANTS()) {
      // Create a copy of the focal program
      program_t mutant_program(focal_program);
      mutator->ResetLastMutationTracker(); // Reset mutator's recorded mutations.
      mutator->ApplyAll(
        *random_ptr,
        mutant_program
      );
      // If we've seen this program before, skip adding it.
      if (emp::Has(mutants, mutant_program)) {
        continue;
      }
      Inject({mutant_program});
      mutants.emplace(mutant_program);
    }
    emp_assert(GetSize() == (config.NUM_MUTANTS() + 1));

    ////////////////////////////////////////////////
    // Run each mutant against the full training set
    ////////////////////////////////////////////////
    // Reset tracking vectors
    org_training_scores.clear();
    org_training_scores.resize(
      GetSize(),
      emp::vector<double>(total_training_cases, 0.0)
    );
    org_aggregate_scores.clear();
    org_aggregate_scores.resize(
      GetSize(),
      0.0
    );
    org_training_coverage.clear();
    org_training_coverage.resize(
      GetSize(),
      0
    );
    // For each mutant:
    for (size_t org_id = 0; org_id < GetSize(); ++org_id) {
      // -- Begin org evaluation --
      // 0-out scores / coverage
      std::fill(
        org_training_scores[org_id].begin(),
        org_training_scores[org_id].end(),
        0.0
      );
      org_aggregate_scores[org_id] = 0.0;
      org_training_coverage[org_id] = 0.0;
      // Reset phenotype
      auto& org = GetOrg(org_id);
      org.ResetPhenotype(total_training_cases);
      // -- Program evaluation --
      eval_hardware->SetProgram(org.GetGenome().GetProgram());
      // Run program on each training case:
      for (size_t i = 0; i < total_training_cases; ++i) {
        const size_t test_id = all_training_case_ids[i];
        // Begin program test
        begin_program_test_sig.Trigger( org, test_id, true);
        // Do program test
        do_program_test_sig.Trigger(org, test_id);
        // End program test
        // - Evaluate output
        TestResult result = problem_manager.EvaluateOutput(
          *eval_hardware,
          org,
          test_id,
          true
        );
        // - Update organism phenotype
        org.UpdatePhenotype(test_id, result);
        // - Update tracking
        org_training_scores[org_id][test_id] = result.score;
        org_aggregate_scores[org_id] += result.score;
        org_training_coverage[org_id] += (size_t)result.is_correct;
      }
    }

    ////////////////////////////////////////////////
    // Write to mutant analysis file
    ////////////////////////////////////////////////
    for (mutant_id = 0; mutant_id < GetSize(); ++mutant_id) {
      mutant_file.Update();
    }

  }
}

// Re-implementation of utils::find_pareto_front
void ProgSynthWorld::FindParetoFrontPairs(const emp::vector< ProgSynthWorld::PhenUnionInfo >& phen_pairs) {

    const size_t num_candidates = phen_pairs.size();


    // emp::vector<size_t> all_candidates(num_candidates);
    complementary_pair_ids.resize(num_candidates);
    std::iota(
      complementary_pair_ids.begin(),
      complementary_pair_ids.end(),
      0
    );

    // -- Find the pareto front --
    // Initialize the front with the first candidate
    front_pair_ids.clear();
    auto& front = front_pair_ids;
    // emp::vector<size_t> front({complementary_pair_ids[0]});
    front.emplace_back(complementary_pair_ids[0]);

    for (size_t pair_i = 1; pair_i < complementary_pair_ids.size(); ++pair_i) {
      const size_t cur_pair_id = complementary_pair_ids[pair_i];
      bool dominated = false; // tracks whether this candidate has been dominated by anything on the front
      for (int front_i = 0; front_i < (int)front.size(); ++front_i) {
        const size_t front_id = front[front_i];
        // emp_assert(score_table[cur_candidate_id].size() == score_table[front_id].size());
        emp_assert(phen_pairs[cur_pair_id].phen_union.GetSize() == phen_pairs[front_id].phen_union.GetSize());
        // note, this could be made more efficient (by computing sharing domination comparisons)
        if (utils::dominates(phen_pairs[cur_pair_id].phen_union, phen_pairs[front_id].phen_union)) {
          // candidate dominates member of the front, remove member of the front
          std::swap(front[front_i], front[front.size()-1]); // Put dominated member of the front in the back
          front.pop_back(); // Remove dominated member of the front
          --front_i; // need to look at this index again
        } else if (utils::dominates( phen_pairs[front_id].phen_union, phen_pairs[cur_pair_id].phen_union)) {
          dominated = true;
          break;
        }
      }
      if (!dominated) front.emplace_back(cur_pair_id);
    }
    // return front;
  }


}
