import pandas as pd
import sys

xsl_file = pd.read_excel(sys.argv[1], sheet_name= 'compressed')
xsl_file['commSats_codes'] = -1
xsl_file['satCode_synt_comm'] = -1
xsl_file = xsl_file.fillna(-1)
new = xsl_file[['satCode','time_sec','is_vel_north','A_and_B_bits']]
new.to_csv(f'compressed2.csv', encoding='utf-8', index=False)
