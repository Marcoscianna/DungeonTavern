import math

def generate_skydome_obj(radius=500.0, rings=32, sectors=32, filename="skydome.obj"):
    vertices = []
    uvs = []
    faces = []

    # 1. Generazione Vertici e Coordinate UV
    for r in range(rings + 1):
        v = r / rings
        phi = v * math.pi  # da 0 a pi (da polo Nord a polo Sud)

        for s in range(sectors + 1):
            u = s / sectors
            theta = u * 2.0 * math.pi  # da 0 a 2*pi

            # Coordinate 3D
            x = radius * math.sin(phi) * math.cos(theta)
            y = radius * math.cos(phi)
            z = radius * math.sin(phi) * math.sin(theta)

            vertices.append((x, y, z))
            uvs.append((u, 1.0 - v))  # Inversione Y per la texture

    # 2. Generazione Facce (INVERTITE per vedere il cielo dall'interno)
    for r in range(rings):
        for s in range(sectors):
            first = r * (sectors + 1) + s
            second = first + sectors + 1

            # Ordine invertito (first, second, second+1) -> Normali verso l'interno
            faces.append((first + 1, second + 1, second + 2))
            faces.append((first + 1, second + 2, first + 2))

    # 3. Scrittura del file OBJ
    with open(filename, "w") as f:
        f.write("# Skydome Procedurale Invertito per Texture Equirettangolari\n")
        f.write("o Skydome\n")

        for vx, vy, vz in vertices:
            f.write(f"v {vx:.4f} {vy:.4f} {vz:.4f}\n")

        for tu, tv in uvs:
            f.write(f"vt {tu:.4f} {tv:.4f}\n")

        for f1, f2, f3 in faces:
            # Associa vertice e coordinata UV (f/vt)
            f.write(f"f {f1}/{f1} {f2}/{f2} {f3}/{f3}\n")

    print(f"Modello '{filename}' generato con successo!")

if __name__ == "__main__":
    generate_skydome_obj(radius=500.0, rings=32, sectors=32)