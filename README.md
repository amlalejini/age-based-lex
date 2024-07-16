# Using lineage age to augment search space exploration in lexicase selection

[![supplemental](https://img.shields.io/badge/go%20to-supplemental%20material-ff69b4)](https://lalejini.com/age-based-lex/bookdown/supp/)
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.12751839.svg)](https://doi.org/10.5281/zenodo.12751839)

## Overview

### Abstract

> Evolutionary algorithms often become stuck on a particular evolutionary trajectory, limiting the available outcomes and often excluding global optima.
Indeed, the starting state of a population (or early decisions) may limit the regions of search space that the algorithm will ultimately explore.
One mechanism used to ensure that more regions of a search space are considered is to regularly inject new random starting points, while giving special advantages to younger lineages to give them a chance to survive long enough to realize their potential.
In this chapter, we explore including periodic injections of random solutions into lexicase selection, along with age-based selection criteria.
We demonstrate this technique's potential for increased exploration using both program synthesis benchmark problems and construction of Sudoku boards with particular solving characteristics.
Our results are promising, but inconsistent, ranging from highly effective to no meaningful difference from controls.
Ultimately we provide directions for future research that might more fully realize the power of age-based criteria in lexicase selection.

## Repository guide

- `docs/` contains supplemental documentation for our methods.
- `experiments/` contains HPC job submission scripts, configuration files, and data analyses for all experiments.
- `include/` contains C++ implementations of experiment software (header only).
- `scripts/` contains generically useful scripts used in this work.
- `source/` contains .cpp files that can be compiled to run our experiments.

