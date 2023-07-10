from matplotlib import pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation

# Simulation parameters
Simulation_Steps = 3000
Number_Ants = 30
Number_Foods = 50
Board_Size = 1000

with open("log.txt", "r") as file:
    data = file.readlines()

# Colony
exec("Colony_xy = " + (data[0].split(":")[1]).strip())
Colony_radius = float((data[1].split(":"))[1].strip())

# Code to workout where the foods are
def strip_item(item):
    pair = item.split(":")[1].strip().split(",")
    pair[0] = float(pair[0].replace("(",""))
    pair[1] = float(pair[1].replace(")",""))
    return pair

Food = [strip_item(food_item) for food_item in data[2:2+Number_Foods]]

Ants = []
for i in range(Simulation_Steps):
    Ants_States = [strip_item(ant_item) for ant_item in data[2+Number_Foods+1 + i*(Number_Ants+1): 2 + Number_Foods+1 + i*(Number_Ants+1) + Number_Ants]]
    Ants.append(Ants_States)

# Create the initial plot
fig, ax = plt.subplots()
plt.scatter(np.transpose(Food)[0], np.transpose(Food)[1], color="green")
circle1 = plt.Circle((Colony_xy[0], Colony_xy[1]), Colony_radius, color='r') #ye of so little faith.
ax.add_patch(circle1)
ax.set_ylim(-Board_Size, Board_Size)
ax.set_xlim(-Board_Size, Board_Size)
# Initialize an empty scatter plot for the ants
ants_scatter = ax.scatter([], [], color='blue', marker='o')
# Function to update the scatter plot at each frame
def update(frame):
    ants_coords = np.transpose(Ants[frame])
    ants_scatter.set_offsets(Ants[frame])
    return ants_scatter,

# Create the animation
animation = FuncAnimation(fig, update, frames=Simulation_Steps, interval=200, blit=True)

# Display the animation
print(len(Ants))
plt.show()
