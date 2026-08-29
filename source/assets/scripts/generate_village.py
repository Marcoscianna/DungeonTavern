import math
import random

# ==========================================
# 1. PARAMETRI E BOUNDS RADURA / TAVERNA
# ==========================================
center_x = 15.0
center_z = -40.0
center_y_buildings = 0.0
forest_radius_min = 90.0  # Limite esterno (bosco)

# Area d'ingombro della Taverna + margine di rispetto
taverna_min_x = 0.63 - 5.0
taverna_max_x = 34.48 + 5.0
taverna_min_z = -6.66 - 5.0
taverna_max_z = 37.19 + 5.0

houses = ["house_1", "house_2", "house_3", "house_4", "house_5", "house_6", "house_7"]

# Mappatura rigorosa Modello Tenda -> Texture corrispondente
tent_info = [
    ("tent_1", "tex_medieval_tent_01"),
    ("tent_2", "tex_medieval_tent_02"),
    ("tent_3", "tex_medieval_tent_03")
]

tex_medieval = "tex_medieval"
elements_json = []

# ==========================================
# 2. DEFINIZIONE INGOMBRI E DISTANZE MAGGIORATE
# ==========================================
def get_building_radius(model_name):
    if model_name == "house_3":
        return 8.5   # Edificio grande
    elif "house" in model_name:
        return 6.5   # Casa standard
    else:
        return 5.5   # Tenda

# Aumentato il margine minimo inter-edificio per distanziarle maggiormente
MIN_GAP_BETWEEN_BUILDINGS = 5.0

# ==========================================
# 3. POPOLAMENTO EDIFICI
# ==========================================
target_buildings = 140
placed_buildings = []
attempts = 0
max_attempts = 35000

while len(placed_buildings) < target_buildings and attempts < max_attempts:
    attempts += 1

    r = random.uniform(18.0, forest_radius_min - 8.0)
    theta = random.uniform(0.0, 2 * math.pi)

    bx = round(center_x + r * math.cos(theta), 2)
    bz = round(center_z + r * math.sin(theta), 2)

    # Selezione equamente bilanciata
    if random.random() < 0.75:
        model = random.choice(houses)
        tex = tex_medieval
    else:
        # Estrazione bilanciata di uno dei 3 modelli di tenda
        model, tex = random.choice(tent_info)

    b_radius = get_building_radius(model)

    # 1. Controllo limite interno (Taverna)
    if (taverna_min_x - b_radius <= bx <= taverna_max_x + b_radius) and \
            (taverna_min_z - b_radius <= bz <= taverna_max_z + b_radius):
        continue

    # 2. Controllo limite esterno (Bosco)
    dist_from_center = math.hypot(bx - center_x, bz - center_z)
    if dist_from_center > (forest_radius_min - b_radius - 2.0):
        continue

    # 3. Controllo collisione con distanza aumentata
    overlap = False
    for px, pz, p_radius in placed_buildings:
        min_distance = b_radius + p_radius + MIN_GAP_BETWEEN_BUILDINGS
        if math.hypot(bx - px, bz - pz) < min_distance:
            overlap = True
            break
    if overlap:
        continue

    # Orientamento: la facciata guarda verso il centro della radura / taverna
    face_angle = round((math.degrees(math.atan2(center_z - bz, center_x - bx)) - 90.0) % 360.0, 1)

    placed_buildings.append((bx, bz, b_radius))
    b_idx = len(placed_buildings)

    entry = f"""             {{
        "id": "village_building_{b_idx}",
        "model": "{model}",
        "texture": ["{tex}"],
        "translate": [{bx}, {center_y_buildings}, {bz}],
        "eulerAngles": [90.0, {face_angle}, 0.0],
        "scale": [1.0, 1.0, 1.0]
     }}"""
    elements_json.append(entry)

print(",\n".join(elements_json))