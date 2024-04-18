#pragma once

namespace psynth {

template<typename PROGRAM_T>
struct ProgSynthGenome {
  using this_t = ProgSynthGenome<PROGRAM_T>;
  using program_t = PROGRAM_T;

  program_t program;
  size_t age; ///< Not used for equality check (age shouldn't determine equality of two genotypes)

  ProgSynthGenome(const program_t& p) : program(p), age(0) {}
  ProgSynthGenome(const this_t&) = default;
  ProgSynthGenome(this_t&&) = default;

  // NOTE: Operator == does not use age for equality
  bool operator==(const this_t& other) const {
    return program == other.program;
  }

  bool operator!=(const this_t& other) const {
    return !(*this == other);
  }

  bool operator<(const this_t& other) const {
    return program < other.program;
  }

  const program_t& GetProgram() const { return program; }
  program_t& GetProgram() { return program; }

  size_t GetAge() const  { return age; }
  void SetAge(size_t new_age) { age = new_age; }
  size_t IncAge(size_t amount = 1)  {
    age += amount;
    return age;
  }
};

}