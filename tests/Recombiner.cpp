#define CATCH_CONFIG_MAIN

#include "Catch2/single_include/catch2/catch.hpp"

#include "emp/matching/MatchBin.hpp"
#include "emp/bits/BitSet.hpp"

#include "sgp/cpu/mem/BasicMemoryModel.hpp"
#include "sgp/cpu/LinearFunctionsProgramCPU.hpp"

#include "program-synthesis/MutatorLinearFunctionsProgram.hpp"
#include "program-synthesis/RecombinerLinearFunctionsProgram.hpp"

TEST_CASE("RecombinerLinearFunctionsProgram") {
  constexpr size_t TAG_WIDTH = 16;
  constexpr int RANDOM_SEED = 2;
  using mem_model_t = sgp::cpu::mem::BasicMemoryModel;
  using tag_t = emp::BitSet<TAG_WIDTH>;
  using arg_t = int;
  using matchbin_t = emp::MatchBin<
    size_t,
    emp::HammingMetric<TAG_WIDTH>,
    emp::RankedSelector<>,
    emp::NopRegulator
  >;
  using hardware_t = sgp::cpu::LinearFunctionsProgramCPU<
    mem_model_t,
    arg_t,
    matchbin_t
  >;
  using inst_t = typename hardware_t::inst_t;
  using inst_lib_t = typename hardware_t::inst_lib_t;
  using program_t = typename hardware_t::program_t;
  using recomb_t = psynth::RecombinerLinearFunctionsProgram<
    hardware_t,
    tag_t,
    arg_t
  >;


  inst_lib_t inst_lib;

  inst_lib.AddInst("Nop-A", [](hardware_t & hw, const inst_t & inst) { ; }, "No operation!");
  inst_lib.AddInst("Nop-B", [](hardware_t & hw, const inst_t & inst) { ; }, "No operation!");
  inst_lib.AddInst("Nop-C", [](hardware_t & hw, const inst_t & inst) { ; }, "No operation!");

  emp::Random random(RANDOM_SEED);

  SECTION("WholeFunctionSwap") {
    recomb_t recombiner;
    recombiner.SetWholeFuncSwapRate(0.8);

    program_t prog_1;
    for (size_t f = 0; f < 3; ++f) {
      prog_1.PushFunction(tag_t());
      for (size_t i = 0; i < 8+f; ++i) {
        prog_1.PushInst(inst_lib, "Nop-A", {-1*(int)f, -1*(int)f, -1*(int)f}, {tag_t()});
      }
    }

    program_t prog_2;
    for (size_t f = 0; f < 8; ++f) {
      prog_2.PushFunction(tag_t());
      for (size_t i = 0; i < 3; ++i) {
        prog_2.PushInst(inst_lib, "Nop-B", {(int)f, (int)f, (int)f}, {tag_t()});
      }
    }

    std::cout << "--- Prog 1 before -- " << std::endl;
    prog_1.Print(std::cout, inst_lib);
    std::cout << std::endl;
    std::cout << "--- Prog 2 before -- " << std::endl;
    prog_2.Print(std::cout, inst_lib);
    std::cout << std::endl;
    recombiner.ApplyWholeFunctionRecombination(random, prog_1, prog_2);
    std::cout << "--- Prog 1 after -- " << std::endl;
    prog_1.Print(std::cout, inst_lib);
    std::cout << std::endl;
    std::cout << "--- Prog 2 after -- " << std::endl;
    prog_2.Print(std::cout, inst_lib);
    std::cout << std::endl;
  }

  SECTION("FindTwoPoints") {
    recomb_t recombiner;
    program_t prog;
    for (size_t f = 0; f < 3; ++f) {
      prog.PushFunction(tag_t());
      for (size_t i = 0; i < 2*(f+1); ++i) {
        prog.PushInst(inst_lib, "Nop-A", {-1*(int)f, -1*(int)f, -1*(int)f}, {tag_t()});
      }
    }
    prog.PushFunction(tag_t());
    prog.PushInst(inst_lib, "Nop-A", {0, 0, 0}, {tag_t()});

    for (size_t f = 0; f < prog.GetSize(); ++f) {
      std::cout << "prog[" << f << "], length = " << prog[f].GetSize() << std::endl;
      for (size_t i = 0; i < 10; ++i) {
        auto points = recombiner.FindTwoPoints(random, prog[f]);
        std::cout << "(" << points.first << "," << points.second << ")" << std::endl;
      }
    }
  }

  SECTION("TwoPointCrossover") {
    recomb_t recombiner;
    recombiner.SetFuncSeqCrossoverRate(0.0);

    program_t prog_1;
    for (size_t f = 0; f < 3; ++f) {
      prog_1.PushFunction(tag_t());
      for (size_t i = 0; i < 20; ++i) {
        prog_1.PushInst(inst_lib, "Nop-A", {-(int)f+1, (int)i, 0}, {tag_t()});
      }
    }

    program_t prog_2;
    for (size_t f = 0; f < 8; ++f) {
      prog_2.PushFunction(tag_t());
      for (size_t i = 0; i < 20; ++i) {
        prog_2.PushInst(inst_lib, "Nop-B", {(int)f+1, (int)i, 0}, {tag_t()});
      }
    }

    std::cout << "--- Prog 1 before -- " << std::endl;
    prog_1.Print(std::cout, inst_lib);
    std::cout << std::endl;
    std::cout << "--- Prog 2 before -- " << std::endl;
    prog_2.Print(std::cout, inst_lib);
    std::cout << std::endl;
    recombiner.ApplyFunctionSequenceRecombinationTwoPoint(random, prog_1, prog_2);
    std::cout << "--- Prog 1 after -- " << std::endl;
    prog_1.Print(std::cout, inst_lib);
    std::cout << std::endl;
    std::cout << "--- Prog 2 after -- " << std::endl;
    prog_2.Print(std::cout, inst_lib);
    std::cout << std::endl;
  }


}

