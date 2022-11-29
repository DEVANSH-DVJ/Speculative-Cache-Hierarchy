import matplotlib.pyplot as plt

y_val = [0.985827147327113, 0.494544311729459, 0.513623549956103, 0.304248033235752, 0.070709042602669]

x_val = ['ALL', 'NO L2C', 'NO LLC', 'NO L2C & NO LLC', 'NONE']

plt.figure(figsize=(8, 6))
plt.bar(x_val, y_val)
plt.title('Effect of SCH at Cache levels on performance')
plt.xlabel('Speculative Cache Hierarchy Status')
plt.ylabel('Normalised performance (gmean)')
plt.axhline(1.0, color='k', linestyle='--', label='Baseline')
plt.ylim(0.0, 1.1)
plt.grid()
plt.legend(loc='upper right')
plt.savefig('all_spec_diff_setup.png')

y_val = [0.9862465334678127, 0.9856328238156746, 0.9858243275357741, 0.9858271473271127, 0.9858271473271127]

x_val = ['0.0625x', '0.125x', '0.25x', '0.5x', '1x']

plt.clf()
plt.bar(x_val, y_val)
plt.title('Effect of SCH Size on performance')
plt.xlabel('Speculative Cache Hierarchy Size')
plt.ylabel('Normalised performance (gmean)')
plt.axhline(1.0, color='k', linestyle='--', label='Baseline')
plt.ylim(0.8, 1.05)
plt.grid()
plt.legend(loc='upper right')
plt.savefig('all_spec_diff_size.png')
