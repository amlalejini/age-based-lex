# Plot averaged Lowest Active Position

library(ggplot2)
library(dplyr)
library(rlang)

setwd("C:/Users/HP/Documents/Education/Research/GPTP2024/runs2/average")


generate_plot <- function(file_name, metric_name) {
  og_data <- read.csv(file_name, header=TRUE)
  
  # Making sure file_name is basename, remove extension: "diversity_200_500_100"
  file_base <- tools::file_path_sans_ext(basename(file_name))
  # Extract parameters from file_name: ["diversity", "200", "500", "100"]
  file_parts <- strsplit(file_base, "_")[[1]]
  type <- file_parts[1]
  num_vals <- file_parts[2]
  pop_size <- file_parts[3]
  inject_count <- file_parts[4]

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
  required_columns <- c("X", "Type", column_name)
  if (!all(required_columns %in% colnames(og_data))) {
    stop(paste("One or more required columns are missing in the file:", file_name))
  }
  
  # Identify extra headers
  extra_header_rows <- which(grepl("Average Fitness", og_data$Average.Fitness))
  # Remove all extra headers
  clean_data <- og_data[-extra_header_rows, ]

  # Convert target column to numeric
  clean_data[[column_name]] <- as.numeric(clean_data[[column_name]])
  
  
  plot <- ggplot(clean_data, aes(x = X, y = !!sym(column_name), group = Type, color = Type)) +
    geom_line() +
    labs(
      title = paste("Averaged", metric_name, "Over Time"),
      subtitle = paste0("Experiment '", type, "', Cardinality ", num_vals, ", Pop. Size ", pop_size, ", Inject Size ", inject_count),
      x = "Generations (x1000)",
      y = metric_name
    )+
    theme_minimal()
  
  # Save the plot
  metric_name_file <- gsub(" ", "_", metric_name)
  
  output_filename <- paste0(tools::file_path_sans_ext(file_name), "_", metric_name_file, "_plot.png")
  ggsave(output_filename, plot=plot, width=10, height=6, bg="white")
}

metrics <- c("Maximum Fitness", "Collective Fitness", "Lowest Active Position")
all_files <- list.files(pattern="*.csv")

# Exclude files starting with "C"
all_files <- all_files[!grepl("^C", all_files)]
print(all_files)

for (file in all_files) {
  for (metric in metrics) {
    generate_plot(file, metric)
  }
}

# --------------OLD--------------
# col <- 3 # Which column are we plotting over time?
# plot_title <- "Lowest Active Position Over Time"
# plot_subtitle <- "Averaged across 20 replicates, 'Exploit' diagnostic"
# plot_y <- "Lowest Active Position"
# 
# control <- read.csv("C12_AgeControl_exploit_200_500_50.csv")
# aged <- read.csv("C48_AgeDiagnostics_exploit_200_500_50.csv")
# control_id <- "C12 (Control)"
# aged_id <- "C48 (Aged)"
# 
# 
# combined <- bind_rows(
#   mutate(control, id=control_id),
#   mutate(aged, id=aged_id)
# )
# 
# average <- ggplot(combined, aes(combined[,1], combined[,col], group=id)) +
#   geom_line(aes(color=id)) +
#   labs(title=plot_title, subtitle=plot_subtitle, x="Generation", y=plot_y)
# plot(average)
# 
# 
# 
