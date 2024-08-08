import os
import pandas as pd
import numpy as np


def main():
    data_dir = "/mnt/home/suzuekar/MABE2/runs/GPTP2024/results"
    
    # TODO: Make sure all average CSVs across all sets are moved to this folder. For now, do this manually
    avg_dir = os.path.join(data_dir, "average")
    if not os.path.exists(avg_dir):
        print("Average folder does not exist")
        return
    
    # For average files, group .mabe file types with same conditions
    avg_condition_groups = {}
    for entry in os.listdir(avg_dir):
        conditions = entry.split('_')[2:] # e.g ["explore", "200", "500", "50.csv"]
        conditions[-1] = conditions[-1].replace(".csv", "") # e.g ["explore", "200", "500", "50"]
        conditions_str = "_".join(conditions) # e.g explore_200_500_50
        if conditions_str not in avg_condition_groups:
            avg_condition_groups[conditions_str] = [] 
        avg_condition_groups[conditions_str].append(os.path.join(avg_dir, entry))
        print(conditions_str, avg_condition_groups[conditions_str])

    # e.g. {"explore_200_500_50": [path_to_control.csv, path_to_diagnostic.csv, etc.]}
    for cond, files in avg_condition_groups.items():
        aggregate_file = os.path.join(avg_dir, f"{cond}.csv") # explore_200_500_50.csv

        for file in files: # [path_to_control.csv, path_to_diagnostic.csv, etc.]
            experiment_type = file.split('_')[1] # e.g. AgeControl
            file_data = pd.read_csv(file)

            if experiment_type == "AgeControl":
                file_data["Type"] = ["Control" for i in range(len(file_data))]
            elif experiment_type == "AgeDiagnostics":
                file_data["Type"] = ["Aged" for i in range(len(file_data))]
            elif experiment_type == "AgeControl-No-Inject":
                file_data["Type"] = ["Control-NoInject" for i in range(len(file_data))]
            elif experiment_type == "AgeControl-No-Replace":
                file_data["Type"] = ["Control-NoReplace" for i in range(len(file_data))]
            elif experiment_type == "AgeDiagnostics-No-Replace":
                file_data["Type"] = ["Aged-NoReplace" for i in range(len(file_data))]
            
            if os.path.exists(aggregate_file):
                file_data.to_csv(aggregate_file, mode="a")
            else:
                file_data.to_csv(aggregate_file)

if __name__ == "__main__":
    main()