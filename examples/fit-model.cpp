/*
 * eos - A 3D Morphable Model fitting library written in modern C++11/14.
 *
 * File: examples/fit-model.cpp
 *
 * Copyright 2016 Patrik Huber
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "eos/core/Image.hpp"
#include "eos/core/image/opencv_interop.hpp"
#include "eos/core/Landmark.hpp"
#include "eos/core/LandmarkMapper.hpp"
#include "eos/core/read_pts_landmarks.hpp"
#include "eos/core/write_obj.hpp"
#include "eos/fitting/fitting.hpp"
#include "eos/morphablemodel/Blendshape.hpp"
#include "eos/morphablemodel/MorphableModel.hpp"
#include "eos/render/draw_utils.hpp"
#include "eos/render/texture_extraction.hpp"
#include "eos/cpp17/optional.hpp"

#include "Eigen/Core"

#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <tuple>
#include <string>
#include <vector>
#include <iostream>

using namespace eos;
namespace po = boost::program_options;
namespace fs = boost::filesystem;
using eos::core::Landmark;
using eos::core::LandmarkCollection;
using cv::Mat;
using cv::Size;
using cv::Scalar;
using std::cout;
using std::endl;
using std::string;
using Eigen::Vector2f;
using Eigen::Vector4f;
using std::vector;
using std::tuple;
using std::get;
using std::nullopt;
using Eigen::VectorXf;
using Eigen::MatrixXf;

/**
 * This app demonstrates estimation of the camera and fitting of the shape
 * model of a 3D Morphable Model from an ibug LFPW image with its landmarks.
 * In addition to fit-model-simple, this example uses blendshapes, contour-
 * fitting, and can iterate the fitting.
 *
 * 68 ibug landmarks are loaded from the .pts file and converted
 * to vertex indices using the LandmarkMapper.
 */
int main(int argc, char* argv[])
{
    string modelfile, isomapfile, imagefile, landmarksfile, mappingsfile, contourfile, edgetopologyfile,
        blendshapesfile, outputbasename;
    try
    {
        po::options_description desc("Allowed options");
        // clang-format off
        desc.add_options()
            ("help,h", "display the help message")
            ("model,m", po::value<string>(&modelfile)->required()->default_value("../../../share/sfm_shape_3448.bin"),
                "a Morphable Model stored as cereal BinaryArchive")
            ("image,i", po::value<string>(&imagefile)->required()->default_value("../../../examples/data/blank.png"),
                "an input image")
            ("blendshapes,b", po::value<string>(&blendshapesfile)->required()->default_value("../../../share/expression_blendshapes_3448.bin"),
                "file with blendshapes")
            ("output,o", po::value<string>(&outputbasename)->required()->default_value("../../../share/out"),
                "basename for the output rendering and obj files");
        // clang-format on
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
        if (vm.count("help")) {
            cout << "Usage: fit-model [options]" << endl;
            cout << desc;
            return EXIT_SUCCESS;
        }
        po::notify(vm);
    } catch (const po::error& e) {
        cout << "Error while parsing command-line arguments: " << e.what() << endl;
        cout << "Use --help to display a list of options." << endl;
        return EXIT_FAILURE;
    }
    
    // Load the data
    // Mat image = cv::imread(imagefile);
    // Mat outimg = image.clone();
    
    int image_width = 711;
    int image_height = 639;
    
    Mat image(Size(image_width, image_height), CV_8UC3, Scalar(0));
    Mat outimg = image.clone();
    
    vector<float> pca_shape_coefficients = {-0.0379799,0.559008,0.602648,-0.152,0.21687,-0.759703,0.769095,
        -0.968627,-2.42433,-0.354739,2.29922,-0.591134,-0.574637,-1.82709,
        0.0282491,-1.06557,-2.25787,1.1133,-1.61851,-0.981673,-0.691006,
        0.670574,-0.441432,1.33156,-1.00528,0.798634,-0.0573042,-0.59704,
        0.825366,1.14771,0.380775,1.63973,-0.0666317,0.822097,-0.735586,
        0.106438,-0.628944,0.396325,1.21352,1.47966,-0.397452,0.0053387,
        0.846061,0.457958,0.43957,0.597302,0.123962,-0.0737794,-0.215417,
        0.891043,0.119919,0.773746,-0.0905794,0.212756,0.10064,0.225995,
        -0.0527107,0.186097,1.1114,0.307628,0.302768,0.0233245,0.0557738};

    vector<float> blendshape_coefficients = {0.912141,0,0,0,0.558107,0.371095};
    
    float r_x = -3.29358*0.01745;   // pitch
    float r_y = 60.869*0.01745;     // yaw
    float r_z = -49.4783*0.01745;   // roll
    
    const auto rot_mtx_x = glm::rotate(glm::mat4(1.0f), r_x, glm::vec3{1.0f, 0.0f, 0.0f});
    const auto rot_mtx_y = glm::rotate(glm::mat4(1.0f), r_y, glm::vec3{0.0f, 1.0f, 0.0f});
    const auto rot_mtx_z = glm::rotate(glm::mat4(1.0f), r_z, glm::vec3{0.0f, 0.0f, 1.0f});
    glm::mat3x3 rotation_matrix = rot_mtx_z * rot_mtx_x * rot_mtx_y;
    
    float tx = 170.454;
    float ty = 97.5417;
    float scale = 2.30562;

    fitting::ScaledOrthoProjectionParameters current_pose = {rotation_matrix, tx, ty, scale};

    morphablemodel::MorphableModel model;
    try {
        model = morphablemodel::load_model(modelfile);
    } catch (const std::runtime_error& e) {
        cout << "Error loading the Morphable Model: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    
    // The expression blendshapes:
    const vector<morphablemodel::Blendshape> blendshapes = morphablemodel::load_blendshapes(blendshapesfile);

    morphablemodel::MorphableModel morphable_model(
        model.get_shape_model(), blendshapes, model.get_color_model(), cpp17::nullopt,
        model.get_texture_coordinates());
    
    // Get affine_camera_matrix from pose data
    fitting::RenderingParameters rendering_params(current_pose, image_width, image_height);
    
    const Eigen::Matrix<float, 3, 4> affine_camera_matrix =
    fitting::get_3x4_affine_camera_matrix(rendering_params, image_width, image_height);
    
    VectorXf pca_shape = morphable_model.get_shape_model().draw_sample(pca_shape_coefficients);
    assert(morphable_model.has_separate_expression_model());
    
    const MatrixXf blendshapes_as_basis = morphablemodel::to_matrix(blendshapes);
    
    VectorXf combined_shape = pca_shape +
    blendshapes_as_basis * Eigen::Map<const VectorXf>(blendshape_coefficients.data(),
                                                      blendshape_coefficients.size());
    
    auto mesh = morphablemodel::sample_to_mesh(
        combined_shape, morphable_model.get_color_model().get_mean(),
        morphable_model.get_shape_model().get_triangle_list(),
        morphable_model.get_color_model().get_triangle_list(), morphable_model.get_texture_coordinates(),
        morphable_model.get_texture_triangle_indices());

    // Extract the texture from the image using given mesh and camera parameters:
    const core::Image4u isomap =
        render::extract_texture(mesh, affine_camera_matrix, core::from_mat(image), true);

    // Draw the fitted mesh as wireframe, and save the image:
    render::draw_wireframe(outimg, mesh, rendering_params.get_modelview(), rendering_params.get_projection(),
                           fitting::get_opencv_viewport(image_width, image_height));
    fs::path outputfile = outputbasename + ".png";
    cv::imwrite(outputfile.string(), outimg);

    // Save the mesh as textured obj:
    outputfile.replace_extension(".obj");
    core::write_textured_obj(mesh, outputfile.string());

    // And save the isomap:
    outputfile.replace_extension(".isomap.png");
    cv::imwrite(outputfile.string(), core::to_mat(isomap));

    cout << "Finished fitting and wrote result mesh and isomap to files with basename "
         << outputfile.stem().stem() << "." << endl;

    return EXIT_SUCCESS;
}
