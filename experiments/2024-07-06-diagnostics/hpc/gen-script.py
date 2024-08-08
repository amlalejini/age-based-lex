'''
Generate slurm job submission scripts - one per condition
'''

import argparse, os, sys, pathlib
from pathlib import Path
from datetime import datetime
# from asyncio import base_events
# from email.policy import default
from pyvarco import CombinationCollector
# TODO: make sure sys path is pointing to the right directory with the utilities script in it
sys.path.append(os.path.join(pathlib.Path(os.path.dirname(os.path.abspath(__file__))).parents[2], "scripts"))
# TODO: I know Alex has his own utilities script, so we'll see
import utilities as utils

default_seed_offset = 0 # Corresponds to JOB_SEED_OFFSET
# default_account = "default"
default_partition = "general_short"
default_num_replicates = 1 # How many runs PER conditions
default_job_time_request = "03:50:00" 
default_job_mem_request = "16G" 

job_name = "aged_lexicase"
executable = "MABE"
# exec_dir = home_dir
env = "aged_lexicase" # Conda environment name

base_script_filename = "./base-script.txt"


###################################################################
# Define conditions
###################################################################

# Create combo object to collect all conditions we'll run
combos = CombinationCollector()

fixed_parameters = {
    "num_gens":"1000000",
    "print_step":"1000",
    "inject_step":"50000",
    "elite_count":"0",
}

special_decorators = ["__DYNAMIC", "__COPY_OVER"]

# Register parameters that vary across treatments/runs
combos.register_var("filename")
combos.register_var("diagnostic__COPY_OVER")
combos.register_var("pop_inject_ratio__COPY_OVER")
combos.register_var("num_vals__COPY_OVER")

combos.add_val(
    "filename",
    [
        # "AgeControl.mabe",
        # "AgeDiagnostics.mabe"
        "AgeControl-No-Inject.mabe",
        "AgeControl-No-Replace.mabe",
        "AgeDiagnostics-No-Replace.mabe"
    ]
)

combos.add_val(
    "diagnostic__COPY_OVER",
    [
        "-s diagnostics.diagnostic=\\\"explore\\\"",
        # "-s diagnostics.diagnostic=\\\"exploit\\\"",
        "-s diagnostics.diagnostic=\\\"diversity\\\""
    ]
)

combos.add_val(
    "pop_inject_ratio__COPY_OVER",
    [
        "-s pop_size=500 -s inject_count=50",
        "-s pop_size=1000 -s inject_count=100",
        "-s pop_size=500 -s inject_count=100",
        "-s pop_size=1000 -s inject_count=200",
        "-s pop_size=500 -s inject_count=200",
        "-s pop_size=1000 -s inject_count=400"
    ]
)
combos.add_val(
    "num_vals__COPY_OVER",
    [
        "-s num_vals=200",
        "-s num_vals=500",        
    ]
)


def main():
    parser = argparse.ArgumentParser(description="Run submission script.")
    parser.add_argument("--data_dir", type=str, help="Where is the output directory for phase one of each run?")
    parser.add_argument("--config_dir", type=str, help="Where is the configuration directory for experiment?")
    parser.add_argument("--repo_dir", type=str, help="Where is the repository for this experiment?")
    parser.add_argument("--exec_dir", type=str, help="Where is the directory for the executable?")
    parser.add_argument("--job_dir", type=str, default=None, help="Where to output these job files? If none, put in 'jobs' directory inside of the data_dir")
    parser.add_argument("--replicates", type=int, default=default_num_replicates, help="How many replicates should we run of each condition?")
    parser.add_argument("--seed_offset", type=int, default=default_seed_offset, help="Value to offset random number seeds by")
    parser.add_argument("--account", type=str, default=default_account, help="Value to use for the slurm ACCOUNT")
    parser.add_argument("--partition", type=str, default=default_partition, help="Value to use for the slurm ACCOUNT")
    parser.add_argument("--time_request", type=str, default=default_job_time_request, help="How long to request for each job on hpc?")
    parser.add_argument("--mem", type=str, default=default_job_mem_request, help="How much memory to request for each job?")
    parser.add_argument("--runs_per_subdir", type=int, default=-1, help="How many replicates to clump into job subdirectories")

    # Load in command line arguments
    args = parser.parse_args()
    data_dir = args.data_dir
    config_dir = args.config_dir
    job_dir = args.job_dir
    repo_dir = args.repo_dir
    exec_dir = args.exec_dir
    num_replicates = args.replicates
    # hpc_account = args.account
    hpc_partition = args.partition
    seed_offset = args.seed_offset
    job_time_request = args.time_request
    job_memory_request = args.mem
    runs_per_subdir = args.runs_per_subdir

    # Load in the base slurm file
    base_sub_script = ""
    with open(base_script_filename, 'r') as fp:
        base_sub_script = fp.read()

    # Get list of all combinations to run
    combo_list = combos.get_combos()

    # Calculate how many jobs we have, and what the last id will be
    num_jobs = num_replicates * len(combo_list)
    runs_per_subdir = runs_per_subdir if runs_per_subdir > 0 else 2 * num_jobs
    print(f'Generating {num_jobs} across {len(combo_list)} files!')
    print(f' - Data directory: {data_dir}')
    print(f' - Config directory: {config_dir}')
    print(f' - Repository directory: {repo_dir}')
    print(f' - Replicates: {num_replicates}')
    # print(f' - Account: {hpc_account}')
    print(f' - Parititon: {hpc_partition}')
    print(f' - Time Request: {job_time_request}') # TODO: Make job time request vary between population sizes etc
    print(f' - Seed offset: {seed_offset}')

    # If no job_dir provided, default to data_dir/jobs
    if job_dir == None:
        job_dir = os.path.join(data_dir, "jobs")

    # Create job file for each condition
    cur_job_id = 0 
    cond_i = 0 # Condition ID
    generated_files = set() 
    cur_subdir_run_cnt = 0 # Number of runs from jobs in current subdir (can't exceed runs_per_subdir)
    cur_run_subdir_id = 0 # Subdir ID
    for condition_dict in combo_list:
        # Seed
        cur_seed = seed_offset + (cur_job_id * num_replicates)

        # Retrieve info from current problem
        filename = condition_dict["filename"][:-5] # removes ".mabe" extension
        diagnostic_name = condition_dict["diagnostic__COPY_OVER"].split('=')[-1].strip("\\\"") # exploit, explore, or diversity
        cardinality = condition_dict["num_vals__COPY_OVER"].split('=')[-1]
        pop_size = condition_dict["pop_inject_ratio__COPY_OVER"].split()[1].split('=')[-1]
        inject_size = condition_dict["pop_inject_ratio__COPY_OVER"].split()[3].split('=')[-1]

        # Filename for job scripts
        # e.g: C1_AgeControl_explore_200_10000_500
        filename_prefix = f'C{cond_i}_{filename}_{diagnostic_name}_{cardinality}_{pop_size}_{inject_size}'
        # TODO: Find a better naming convention for RUN_DIR and job scripts

        # Adjust job request time
        # if diagnostic_name == "explore" or diagnostic_name == "exploit":
        #     if pop_size == "1000":
        #         job_time_request = "08:00:00"
        #     else:
        #         job_time_request = "03:50:00"
        # elif diagnostic_name == "diversity":
        #     if pop_size == "1000":
        #         job_time_request = "16:00:00"            
        #     else:
        #         job_time_request = "08:00:00"
        # else:
        #     job_time_request = "03:50:00"

        # Replace base script
        file_str = base_sub_script
        file_str = file_str.replace("<<ENV>>", env)

        file_str = file_str.replace("<<CONFIG_DIR>>", config_dir)
        file_str = file_str.replace("<<REPO_DIR>>", repo_dir)
        file_str = file_str.replace("<<DATA_DIR>>", data_dir)
        file_str = file_str.replace("<<EXEC>>", executable)
        file_str = file_str.replace("<<EXEC_DIR>>", exec_dir)

        file_str = file_str.replace("<<TIME_REQUEST>>", job_time_request)
        file_str = file_str.replace("<<MEMORY_REQUEST>>", job_memory_request)
        file_str = file_str.replace("<<JOB_NAME>>", job_name)
        file_str = file_str.replace("<<JOB_SEED_OFFSET>>", str(cur_seed))
        file_str = file_str.replace("<<PARTITION_NAME>>", hpc_partition)

        ###################################################################
        # Configure the run
        ###################################################################
        # Replace path to run directories
        # new_path = os.path.join(data_dir, f'{filename_prefix}_'+'${SEED}')
        file_str = file_str.replace("<<RUN_DIR>>", \
            os.path.join(data_dir, f'{filename_prefix}_'+'${SEED}'))
            # Use same naming convention for RUN_DIR and script files
        # This doesn't create folders, they're created when we submit the 

        # Parameters not marked with "COPY_OVER" and "DYNAMIC"
        run_param_info = {key:condition_dict[key] for key in condition_dict if not any([dec in key for dec in special_decorators])}
        # Add fixed paramters
        # for param in fixed_parameters:
            # if param in run_param_info: continue
            # run_param_info[param] = fixed_parameters[param]
        # Set random number seed
        # run_param_info["SEED"] = '${SEED}'

        ###################################################################
        # Build commandline parameters string
        ###################################################################
        fields = list(run_param_info.keys())
        fields.sort()
        # Build command string from parameters not marked with "COPY_OVER" and "DYNAMIC"
        set_params = [f"--{field} {run_param_info[field]}" for field in fields]
        # Copy exactly strings from parameters marked with "COPY_OVER"
        copy_params = [condition_dict[key] for key in condition_dict if "__COPY_OVER" in key]

        # Fixed parameters
        fixed_fields = list(fixed_parameters.keys())
        # If exploit diagnostic, set to 100k generations instead of 1mil
        # TODO: There might be a better way to do this
        if diagnostic_name == 'exploit':
            fixed_parameters["num_gens"] = "100000"
        else: # Ensure that fixed_parameters["num_gens"] isn't permanently changed
            fixed_parameters["num_gens"] = "1000000"
        fixed_params = [f"-s {fixed_field}={fixed_parameters[fixed_field]}" for fixed_field in fixed_fields]

        other_params = ["-s random_seed=${SEED}", "-s fit_file.filename=\\\"${RUN_DIR}/run_${SLURM_ARRAY_TASK_ID}.csv\\\""]
        
        # "--filename AgeControl.mabe -s num_vals=200...."
        run_params = " ".join(set_params + other_params + copy_params + fixed_params)

        ###################################################################

        # Add run commands to run the experiment
        cfg_run_commands = ''
        # Set the run
        cfg_run_commands += f'RUN_PARAMS="{run_params}"\n' 

        # By default, add all commands to submission file.
        array_id_run_info = {
            array_id: {
                "experiment": True
            }
            for array_id in range(1, num_replicates+1)
        }
        # NOTE: So this holds the seed for every array ID
        # NOTE: I'm guessing this is from 1-20, same as number of replicates?
        array_id_to_seed = {array_id:(cur_seed + (array_id - 1)) for array_id in array_id_run_info}

        # Track which array ids need to be included. If none, don't need to output this file.
        active_array_ids = [] # TODO: What are active and inactive array IDs?
        inactive_array_ids = []
        run_sub_logic = ""
        # NOTE - this is setup to (fairly) easily incorporate job patching logic, but not currently incorporated
        for array_id in range(1, num_replicates+1):
            # If this run is totally done, make note and continue.
            # if not any([array_id_run_info[array_id][field] for field in array_id_run_info[array_id]]):
            #     inactive_array_ids.append(array_id)
            #     continue
            # This run is not done already. Make note.
            active_array_ids.append(array_id) # Append current array ID to active list

            # This will be written to the script
            run_logic = "if [[ ${SLURM_ARRAY_TASK_ID} -eq "+str(array_id)+" ]] ; then\n"

            # (1) Run experiment executable
            run_commands = ''
            run_commands += 'echo "${EXEC} ${RUN_PARAMS}" > cmd.log\n'
            run_commands += 'echo "SLURM_JOB_ID=${SLURM_JOB_ID}" > slurm.log\n'
            run_commands += 'echo "SLURM_ARRAY_TASK_ID=${SLURM_ARRAY_TASK_ID}" >> slurm.log\n'
            run_commands += './${EXEC} ${RUN_PARAMS} > run-${SLURM_ARRAY_TASK_ID}.log\n'

            run_logic += run_commands
            # run_logic += analysis_commands
            run_logic += "fi\n\n"
            run_sub_logic += run_logic

        # -- Set the SLURM array id range parameter --
        array_id_range_param = ""
        if len(active_array_ids) == num_replicates:
            array_id_range_param = f"1-{num_replicates}"
        else:
            array_id_range_param = ",".join([str(array_id) for array_id in active_array_ids])

        # -- add run commands to file str --
        file_str = file_str.replace("<<ARRAY_ID_RANGE>>", array_id_range_param)
        file_str = file_str.replace("<<CFG_RUN_COMMANDS>>", cfg_run_commands) # What is this for?
        file_str = file_str.replace("<<RUN_COMMANDS>>", run_sub_logic)

        ###################################################################
        # Write job submission file (if any of the array ids are active)
        ###################################################################
        # Report active/inactive
        print(f"RUN_C{cond_i}:")
        print(f" - Active: " + ", ".join([f"RUN_C{cond_i}_{array_id_to_seed[array_id]}" for array_id in active_array_ids]))
        print(f" - Inactive: " + ", ".join([f"RUN_C{cond_i}_{array_id_to_seed[array_id]}" for array_id in inactive_array_ids]))

        # Make "set" folders
        cur_job_dir = job_dir if args.runs_per_subdir == -1 else os.path.join(job_dir, f"set-{cur_run_subdir_id}")
        if len(active_array_ids): # If any of array ids are active
            utils.mkdir_p(cur_job_dir)
            with open(os.path.join(cur_job_dir, f'{filename_prefix}.sh'), 'w') as fp:
                fp.write(file_str)

        # Update condition id and current job id
        cur_job_id += 1
        cond_i += 1
        cur_subdir_run_cnt += num_replicates
        if cur_subdir_run_cnt > (runs_per_subdir - num_replicates):
            cur_subdir_run_cnt = 0
            cur_run_subdir_id += 1

if __name__ == "__main__":
    main()