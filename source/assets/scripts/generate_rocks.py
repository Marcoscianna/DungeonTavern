import math
import random

# Parametri del cerchio / montagne
center_x = 15.0
center_y = 0.0  # Altezza del terreno
center_z = -40.0

# Modello e texture della roccia presi dal tuo JSON
models = ["stone17"]
textures = ["tex_dungeon"]

num_rocks = 80  # Numero di montagne rocciose
radius_min = 180.0  # Subito dopo il raggio esterno degli alberi
radius_max = 210.0  # Spessore dell'anello montuoso

rocks_json = []

for i in range(num_rocks):
    # Angolo distribuito in cerchio con leggera variazione
    angle = (2 * math.pi / num_rocks) * i + random.uniform(-0.05, 0.05)
    radius = random.uniform(radius_min, radius_max)

    # Calcolo coordinate X e Z
    x = round(center_x + radius * math.cos(angle), 2)
    z = round(center_z + radius * math.sin(angle), 2)

    # Variazioni casuali di rotazione e scala gigante
    model = random.choice(models)
    tex = random.choice(textures)
    rot_x = round(random.uniform(-10.0, 10.0), 1)
    rot_y = round(random.uniform(0.0, 360.0), 1)

    # Scala base grande con variazione in altezza per creare picchi diversi
    scale_xz = round(random.uniform(8.0, 14.0), 2)
    scale_y = round(scale_xz * random.uniform(1.0, 1.6), 2)

    rock_entry = f"""             {{
                "id": "mountain_{i+1}",
                "model": "{model}",
                "texture": ["{tex}"],
                "translate": [{x}, {center_y}, {z}],
                "eulerAngles": [{rot_x}, {rot_y}, 0.0],
                "scale": [{scale_xz}, {scale_y}, {scale_xz}]
             }}"""
    rocks_json.append(rock_entry)

print(",\n".join(rocks_json))