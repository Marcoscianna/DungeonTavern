import math
import random

center_y_road = 0.3
center_y_buildings = 0.0
taverna_door_x = 18.0

road_straight = "road"
road_turn = "road_turn"
tex_medieval = "tex_medieval"

houses = ["house_1", "house_2", "house_3", "house_4", "house_5", "house_6", "house_7"]
tents = ["tent_1", "tent_2", "tent_3"]

tent_textures = {
    "tent_1": "tex_medieval_tent_01",
    "tent_2": "tex_medieval_tent_02",
    "tent_3": "tex_medieval_tent_03"
}

elements_json = []
road_positions = []
step_length = 8.0

# ==========================================
# 1. TRACCIATO PRINCIPALE (ESCLUDE LA ZONA DIETRO LA TAVERNA)
# ==========================================
path_commands = [
    ("STRAIGHT", 5),
    ("TURN_RIGHT", 1),
    ("STRAIGHT", 6),
    ("TURN_RIGHT", 1),
    ("STRAIGHT", 5),
    ("TURN_RIGHT", 1),
    ("STRAIGHT", 4)
]

curr_x = taverna_door_x
curr_z = -22.0
current_heading = 180.0  # Diretto verso Sud
segment_idx = 1

for cmd, count in path_commands:
    if cmd == "STRAIGHT":
        rad = math.radians(current_heading)
        dx = math.sin(rad) * step_length
        dz = math.cos(rad) * step_length

        for _ in range(count):
            # Filtro di sicurezza: Impedisce posizionamento strada dietro la taverna
            if not (curr_z > -26.0 and -8.0 <= curr_x <= 45.0):
                road_positions.append((curr_x, curr_z, current_heading))
                entry = f"""             {{
                    "id": "road_seg_{segment_idx}",
                    "model": "{road_straight}",
                    "texture": ["{tex_medieval}"],
                    "translate": [{round(curr_x, 2)}, {center_y_road}, {round(curr_z, 2)}],
                    "eulerAngles": [90.0, {round(current_heading, 1)}, 0.0],
                    "scale": [1.0, 1.0, 1.0]
                 }}"""
                elements_json.append(entry)
                segment_idx += 1
            curr_x += dx
            curr_z += dz

    elif cmd == "TURN_RIGHT":
        if not (curr_z > -26.0 and -8.0 <= curr_x <= 45.0):
            road_positions.append((curr_x, curr_z, current_heading))
            turn_angle = (current_heading - 180.0) % 360.0

            entry = f"""             {{
                "id": "road_turn_{segment_idx}",
                "model": "{road_turn}",
                "texture": ["{tex_medieval}"],
                "translate": [{round(curr_x, 2)}, {center_y_road}, {round(curr_z, 2)}],
                "eulerAngles": [90.0, {round(turn_angle, 1)}, 0.0],
                "scale": [1.0, 1.0, 1.0]
             }}"""
            elements_json.append(entry)
            segment_idx += 1

        current_heading = (current_heading + 90.0) % 360.0
        rad = math.radians(current_heading)
        curr_x += math.sin(rad) * step_length
        curr_z += math.cos(rad) * step_length

# ==========================================
# 2. DIRAMAZIONE VICOLO (ESCLUDE L'AREA DIETRO LA TAVERNA)
# ==========================================
branch_x, branch_z = taverna_door_x - 16.0, -22.0  # Spostato sul fianco Ovest
branch_heading = 0.0  # Diretto verso Nord

for i in range(3):
    bx = branch_x
    bz = branch_z + (i * step_length)
    if not (bz > -26.0 and -8.0 <= bx <= 45.0):
        road_positions.append((bx, bz, branch_heading))
        entry = f"""             {{
            "id": "road_branch_{i+1}",
            "model": "{road_straight}",
            "texture": ["{tex_medieval}"],
            "translate": [{round(bx, 2)}, {center_y_road}, {round(bz, 2)}],
            "eulerAngles": [90.0, {round(branch_heading, 1)}, 0.0],
            "scale": [1.0, 1.0, 1.0]
         }}"""
        elements_json.append(entry)

# ==========================================
# 3. POPOLAMENTO EDIFICI
# ==========================================
num_buildings = 50
offset_from_road = 12.0

placed_buildings = []
attempts = 0
max_attempts = 6000

while len(placed_buildings) < num_buildings and attempts < max_attempts:
    attempts += 1
    rx, rz, r_heading = random.choice(road_positions)
    side = random.choice([-1, 1])
    perp_angle = math.radians(r_heading + side * 90)

    depth_var = random.uniform(0.0, 5.0)

    if random.random() < 0.70:
        model = random.choice(houses)
        tex = tex_medieval
    else:
        model = random.choice(tents)
        tex = tent_textures[model]

    is_big = (model == "house_3")
    min_road_dist = 14.5 if is_big else 12.0
    building_radius = 8.5 if is_big else 6.5

    bx = round(rx + (offset_from_road + depth_var) * math.sin(perp_angle), 2)
    bz = round(rz + (offset_from_road + depth_var) * math.cos(perp_angle), 2)

    # Restrizione Taverna rigorosa
    if bz > -26.0 and (-8.0 <= bx <= 45.0):
        continue

    # Collisione Strada
    too_close_to_road = False
    for r_x, r_z, _ in road_positions:
        if math.hypot(bx - r_x, bz - r_z) < min_road_dist:
            too_close_to_road = True
            break
    if too_close_to_road:
        continue

    # Collisione Edifici
    overlap = False
    for px, pz, p_model in placed_buildings:
        other_radius = 8.5 if p_model == "house_3" else 6.5
        required_dist = building_radius + other_radius
        if math.hypot(bx - px, bz - pz) < required_dist:
            overlap = True
            break
    if overlap:
        continue

    placed_buildings.append((bx, bz, model))
    b_idx = len(placed_buildings)

    face_angle = round((r_heading + (90 if side == -1 else -90)) % 360, 1)

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