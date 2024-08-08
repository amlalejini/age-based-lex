'''
Compile and average data
'''

import os
import pandas as pd
import numpy as np

SEED_OFFSET = 10000
REPLICATES = 20
SET = "0"

def parse_conditions(dir_path):
    # Example of a directory name: /mnt/.../C0_AgeControl_explore_200_500_50_10000
    # First term is condition/treatment ID
    # Last term is array-task-dependent seed, which differs among replicates
    base_name = dir_path.split('/')[-1]
    conditions = base_name.split('_')
    condition_id = conditions[0].replace("C","")
    array_id = int(conditions[-1]) - SEED_OFFSET - int(condition_id)*20 + 1
    return (conditions, array_id)


def aggregate_data(condition_folders, avg_file, compile_file):
    # Takes in a list of replicate folders with the same condition and paths to an average and compiled output file
    # Averages and compiles all replicates from the same condition

    aggregated_data = None

    # Iterate through replicates of the same condition
    for folder in condition_folders:
        conditions, array_id = parse_conditions(folder)
        print(array_id)

        # Locate run data in the current condition folder
        run_file = os.path.join(folder, f"run_{array_id}.csv")

        if os.path.exists(run_file):
            run_data = pd.read_csv(run_file)

            if aggregated_data is None:
                aggregated_data = run_data
            else:
                aggregated_data += run_data              
            
            # Add "Generation" column
            run_data['Generations'] = [(i*1000 + 1000) for i in range(len(run_data))]
            # Add "Replicate" column
            run_data['Replicate'] = [array_id for i in range(len(run_data))]

            if os.path.exists(compile_file):
                # Append run data to a single compile file
                run_data.to_csv(compile_file, mode="a")
            else:
                run_data.to_csv(compile_file)
            
            # Remove "Generations" and "Replicate" columns from run_data
            run_data.drop(columns=['Generations', 'Replicate'], inplace=True)

        else:
            print(f"Run file {run_file} not found.")     
    
    # Write averaged data 
    if aggregated_data is not None:
        aggregated_data /= len(condition_folders)
        aggregated_data.to_csv(avg_file)


def main():
    # Directory holding all run folders
    data_dir = os.path.join("/mnt/scratch/suzuekar/GPTP2024/", SET)

    # Directory for compiled data
    compile_dir = os.path.join(data_dir, "compile")
    if not os.path.exists(compile_dir):
        os.makedirs(compile_dir)
    
    # Directory for average data
    avg_dir = os.path.join(data_dir, "average")
    if not os.path.exists(avg_dir):
        os.makedirs(avg_dir)

    # Paths for condition folders
    condition_folders = [os.path.join(data_dir, entry) for entry in sorted(os.listdir(data_dir))
                         if os.path.isdir(os.path.join(data_dir, entry))
                         and entry not in ["jobs", "compile", "average"]]
    
    # Group condition folders by experimental configuration
    condition_groups = {} # e.g. {"conditions_str": [folders]}
    for folder in condition_folders:
        conditions, array_id = parse_conditions(folder)
        conditions_str = "_".join(conditions[:-1])
        if conditions_str not in condition_groups:
            condition_groups[conditions_str] = []
        condition_groups[conditions_str].append(folder) # {"C0_AgeControl_explore_200_500_50": [replicate1, replicate2,...etc]}

    # Generate average and compile files for each experimental configuration
    for cond, folders in condition_groups.items():
        avg_file = os.path.join(avg_dir, f"{cond}.csv")
        compile_file = os.path.join(compile_dir, f"{cond}.csv")
        aggregate_data(folders, avg_file, compile_file)    


if __name__ == "__main__":
    main()