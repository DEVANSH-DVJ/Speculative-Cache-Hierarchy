import matplotlib.pyplot as plt

y_val = [0.985827147327113,0.070709042602669,0.494544311729459,0.304248033235752,0.513623549956103]

x_val = ["ALL-1x","NONE","NOL2C","NOL2C-NOLLC","NOLLC"]

plt.figure(figsize=(19.20,9.83))
plt.bar(x_val, y_val)
plt.xlabel("Evaluation Method Used")
plt.ylabel("Normalised performance (gmean)")
axhline(1)
plt.savefig("res"+".png")
plt.show()


