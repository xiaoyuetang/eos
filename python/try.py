import eos
# import trimesh
import numpy as np

def main():
    """Demo for running the eos fitting from Python."""
    landmarks = read_pts('ibug/image_003_1.pts')
    image_width = 711
    image_height = 639
    yaw, roll, pitch = 60.869, -49.4783, -3.29358
    affine_camera_matrix = [yaw, roll, pitch]
    tx, ty, scale = 170.454, 97.5417, 2.30562
    vertex_indices = [tx, ty, scale]
    pca_shape_coefficients = [-0.0379799,0.559008,0.602648,-0.152,0.21687,-0.759703,0.769095,-0.968627,-2.42433,-0.354739,2.29922,-0.591134,-0.574637,-1.82709,0.0282491,-1.06557,-2.25787,1.1133,-1.61851,-0.981673,-0.691006,0.670574,-0.441432,1.33156,-1.00528,0.798634,-0.0573042,-0.59704,0.825366,1.14771,0.380775,1.63973,-0.0666317,0.822097,-0.735586,0.106438,-0.628944,0.396325,1.21352,1.47966,-0.397452,0.0053387,0.846061,0.457958,0.43957,0.597302,0.123962,-0.0737794,-0.215417,0.891043,0.119919,0.773746,-0.0905794,0.212756,0.10064,0.225995,-0.0527107,0.186097,1.1114,0.307628,0.302768,0.0233245,0.0557738]
    blendshape_coeffs = [0.912141,0,0,0,0.558107,0.371095]

    model = eos.morphablemodel.load_model("share/sfm_shape_3448.bin")
    blendshapes = eos.morphablemodel.load_blendshapes("share/expression_blendshapes_3448.bin")

    mesh = eos.fitting.fit_shape(affine_camera_matrix, model, blendshapes,
                        landmarks, pca_shape_coefficients, blendshape_coeffs)


    # Or for example extract the texture map, like this:
    # import cv2
    # image = cv2.imread('ibug/image_003_1.jpg')
    # isomap = eos.render.extract_texture(mesh, pose, image)

    # help(eos)


def read_pts(filename):
    """A helper function to read the 68 ibug landmarks from a .pts file."""
    lines = open(filename).read().splitlines()
    lines = lines[3:71]

    landmarks = []
    ibug_index = 1  # count from 1 to 68 for all ibug landmarks
    for l in lines:
        coords = l.split()
        landmarks.append(eos.core.Landmark(str(ibug_index), [float(coords[0]), float(coords[1])]))
        ibug_index = ibug_index + 1

    return landmarks

if __name__ == "__main__":
    main()
