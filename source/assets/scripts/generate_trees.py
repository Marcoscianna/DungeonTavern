import math
import random

# Parametri del cerchio / bosco
center_x = 15.0
center_y = 0.0  # Altezza del terreno
center_z = -40.0

models = ["vegetation1", "vegetation6", "vegetation16"]
num_trees = 1000  # Numero totale di alberi
radius_min = 90.0  # Raggio interno del bosco
radius_max = 150.0  # Raggio esterno per dare profondità

trees_json = []

for i in range(num_trees):
    # Angolo distribuito in cerchio con un po' di variazione random
    angle = (2 * math.pi / num_trees) * i + random.uniform(-0.1, 0.1)
    radius = random.uniform(radius_min, radius_max)

    # Calcolo coordinate X e Z
    x = round(center_x + radius * math.cos(angle), 2)
    z = round(center_z + radius * math.sin(angle), 2)

    # Scelta casuale del modello e rotazione casuale sull'asse Y
    model = random.choice(models)
    rot_y = round(random.uniform(0.0, 360.0), 1)
    scale = round(random.uniform(0.8, 1.2), 2)  # Variazione di dimensione

    tree_entry = f"""             {{
                "id": "tree_{i+1}",
                "model": "{model}",
                "texture": ["tex_vegetation"],
                "translate": [{x}, {center_y}, {z}],
                "eulerAngles": [0.0, {rot_y}, 0.0],
                "scale": [{scale}, {scale}, {scale}]
             }}"""
    trees_json.append(tree_entry)

print(",\n".join(trees_json))