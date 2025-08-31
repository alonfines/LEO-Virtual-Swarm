import pandas as pd

def deleteNotvalids(input_csv_file,output_csv_file):
    # Read the CSV file into a DataFrame
    df = pd.read_csv(input_csv_file)
    # Move the third column to the fourth column
    df.insert(3, 'bit_genererated', df.iloc[:, 2])

    df[df.columns[2]] = True

    df.to_csv(output_csv_file, index=False)

def zero_coloum(input_csv_file,output_csv_file):
    # Read the CSV file into a DataFrame
    df = pd.read_csv(input_csv_file)

    df[df.columns[1]] = 12900

    df.to_csv(output_csv_file, index=False)

def deletezeros(input_csv_file,output_csv_file):
    # Read the CSV file into a DataFrame
    df = pd.read_csv(input_csv_file)
    
    # Filter rows where the fourth column is not equal to 0
    df = df[df.iloc[:, 3] != 0]
    
    # Write the modified DataFrame back to the CSV file
    df.to_csv(output_csv_file, index=False)

def DeleteNotRelSats(input_csv_file, output_csv_file):
    # Read the CSV file into a DataFrame
    df = pd.read_csv(input_csv_file)
    
    # Filter the DataFrame to keep only rows where the first cell value is less than or equal to 400
    filtered_df = df[df.iloc[:, 0] != 303]
    filtered_df = filtered_df[filtered_df.iloc[:, 0] != 304]
    
    # Write the filtered DataFrame to the output CSV file
    filtered_df.to_csv(output_csv_file, index=False)


# Provide the input CSV file path
input_csv_file = "C:\\Users\\alonfi\\Downloads\\LEO-Virtual-Swarm\\sheets\\scenerio3_traffic_gen.csv"
output_csv_file = "C:\\Users\\alonfi\\Downloads\\LEO-Virtual-Swarm\\sheets\\scenerio3_traffic_gen_copy.csv"

# Call the function to perform the operation
DeleteNotRelSats(input_csv_file, output_csv_file)
# deletezeros(input_csv_file, output_csv_file)
# zero_coloum(input_csv_file, output_csv_file)