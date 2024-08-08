# Plot Lowest Active Position individually for all 20 replicates

library(ggplot2)
library(dplyr)
library(tidyr)

setwd("C:/Users/HP/Documents/Education/Research/GPTP2024/runs2/compile")

generate_plot <- function(file_name, metric_name) {
  og_data <- read.csv(file_name, header=TRUE)
  
  # Making sure file_name is basename, remove extension: "C0_AgeControl_diversity_200_500_100"
  file_base <- tools::file_path_sans_ext(basename(file_name))
  # Extract parameters from file_name: ["C0", "AgeControl", diversity", "200", "500", "100"]
  file_parts <- strsplit(file_base, "_")[[1]]
  id <- file_parts[1]
  experiment <- file_parts[2]
  type <- file_parts[3]
  num_vals <- file_parts[4]
  pop_size <- file_parts[5]
  inject_count <- file_parts[6]
  
  column_name <- ""
  if (metric_name == "Lowest Active Position") {
    column_name <- "Lowest.Active.Pos"
  }
  else if (metric_name == "Maximum Fitness") {
    column_name <- "Maximum.Fitness"
  }
  else if (metric_name == "Collective Fitness") {
    column_name <- "Collective.Fitness"
  }
  else {
    column_name <- metric_name
  }
  
  # Make sure the necessary columns exist
  required_columns <- c("X", "Replicate", column_name)
  if (!all(required_columns %in% colnames(og_data))) {
    stop(paste("One or more required columns are missing in the file:", file_name))
  }
  
  # Identify extra headers
  extra_header_rows <- which(grepl("Average Fitness", og_data$Average.Fitness))
  # Remove all extra headers
  clean_data <- og_data[-extra_header_rows, ]
  
  # Convert target column to numeric
  clean_data[[column_name]] <- as.numeric(clean_data[[column_name]])
  
  plot <- ggplot(clean_data, aes(x = X, y = !!sym(column_name), group = Replicate, color = Replicate)) +
    geom_line() +
    labs(
      title = paste(metric_name, "Over Time"),
      subtitle = paste0("Experiment '", type, "' (", experiment, "), Cardinality ", num_vals, ", Pop. Size ", pop_size, ", Inject Size ", inject_count),
      x = "Generations (x1000)",
      y = metric_name
    ) +
    theme_minimal()
  
  # Save the plot
  metric_name_file <- gsub(" ", "_", metric_name)
  
  output_filename <- paste0(tools::file_path_sans_ext(file_name), "_", metric_name_file, "_plot.png")
  ggsave(output_filename, plot=plot, width=10, height=6, bg="white")
  
}

metrics <- c("Maximum Fitness", "Collective Fitness", "Lowest Active Position")
all_files <- list.files(pattern="*.csv")

for (file in all_files) {
  for (metric in metrics) {
    generate_plot(file, metric)
  }
}


# # --------------OLD--------------
# plot_title <- "Lowest Active Position Over Time"
# plot_subtitle <- "'Explore' diagnostic, Control run (C0)"
# plot_y <- "Lowest Active Position"
# cond <- read.csv("C46_AgeDiagnostics_explore_200_1000_400.csv", header=TRUE)
# 
# # Identify extra headers
# extra_header_rows <- which(grepl("Average Fitness", cond$Average.Fitness))
# # Remove all extra headers
# clean_data <- cond[-extra_header_rows, ]
# 
# clean_data <- clean_data %>%
#   mutate(Generations = as.numeric(Generations) / 1000)
# 
# # Remove all columns but Generations, Lowest.Active.Pos, and Replicate
# clean_data <- subset(clean_data, select=c(Generations, Maximum.Fitness, Replicate))
# clean_data$Maximum.Fitness = as.numeric(clean_data$Maximum.Fitness)
# # Change from long to wide format
# # pivoted_data <- clean_data %>%
#   # pivot_wider(names_from = Replicate, values_from = Maximum.Fitness, values_fn = list)
# 
# ggplot(clean_data, aes(x = Generations, y = Maximum.Fitness, group = Replicate, color = factor(Replicate))) +
#   geom_line() +
#   labs(
#     title = plot_title,
#     subtitle = plot_subtitle,
#     x = "Generations (x1000)",
#     y = plot_y
#   ) +
#   theme_minimal()
