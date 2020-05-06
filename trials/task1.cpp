/*
 * eos - A 3D Morphable Model fitting library written in modern C++11/14.
 *
 * File: examples/task1.cpp
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

#include "opencv2/opencv.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace eos;
using eos::core::Landmark;
using eos::core::LandmarkCollection;
using cv::Mat;
using std::cout;
using std::endl;
//using std::string;
using std::vector;


int main(int argc, char* argv[])
{
    std::string modelfile, blendshapesfile, outputbasename;
    
    {
        modelfile = std::string("../../../share/sfm_shape_3448.bin");
        blendshapesfile = std::string("../../../share/expression_blendshapes_3448.bin");
        outputbasename = std::string("../../../share/out");
    }
    
    ////////////////////////////////////////////////////////// data ///////////////////////////////////////////////////////////////////
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
    
    float yaw = 60.869;     // yaw
    float roll = -49.4783;   // roll
    float pitch = -3.29358;   // pitch
    
    float tx = 170.454;
    float ty = 97.5417;
    float scale = 2.30562;
    ////////////////////////////////////////////////////////// end of data ///////////////////////////////////////////////////////////////////

    int image_width = 711;
    int image_height = 639;
    
    /////////////////////////////////////////////////////////// loading model ///////////////////////////////////////////////////////////////
    morphablemodel::MorphableModel model;
    try
    {
        model = morphablemodel::load_model(modelfile);
    } catch (const std::runtime_error& e)
    {
        cout << "Error loading the Morphable Model: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    
    // The expression blendshapes:
    const vector<morphablemodel::Blendshape> blendshapes = morphablemodel::load_blendshapes(blendshapesfile);

    morphablemodel::MorphableModel morphable_model(
        model.get_shape_model(), blendshapes, model.get_color_model(), cpp17::nullopt,
        model.get_texture_coordinates());
    /////////////////////////////////////////////////////////// end of  loading model ///////////////////////////////////////////////////////////////
  
    /////////////////////////////////////////////////////////  construct  a mesh and the pose:///////////////////////////////////////////////////////
    const auto rot_mtx_x = glm::rotate(glm::mat4(1.0f), pitch/180*3.14159f, glm::vec3{1.0f, 0.0f, 0.0f});
    const auto rot_mtx_y = glm::rotate(glm::mat4(1.0f), yaw/180*3.14159f, glm::vec3{0.0f, 1.0f, 0.0f});
    const auto rot_mtx_z = glm::rotate(glm::mat4(1.0f), roll/180*3.14159f, glm::vec3{0.0f, 0.0f, 1.0f});
    const auto rotation_matrix = rot_mtx_z * rot_mtx_x * rot_mtx_y;
    
    fitting::ScaledOrthoProjectionParameters current_pose = {rotation_matrix, tx, ty, scale};
    fitting::RenderingParameters rendering_params = fitting::RenderingParameters(current_pose, image_width, image_height);
    
    auto current_pca_shape = morphable_model.get_shape_model().draw_sample(pca_shape_coefficients);
    
    assert(morphable_model.has_separate_expression_model());

    auto current_combined_shape =
        current_pca_shape +
        draw_sample(morphable_model.get_expression_model().value(), blendshape_coefficients);
    auto current_mesh = morphablemodel::sample_to_mesh(
        current_combined_shape, morphable_model.get_color_model().get_mean(),
        morphable_model.get_shape_model().get_triangle_list(),
        morphable_model.get_color_model().get_triangle_list(), morphable_model.get_texture_coordinates(),
        morphable_model.get_texture_triangle_indices());
    /////////////////////////////////////////////////////////  end of construct  a mesh and the pose:///////////////////////////////////////////////////////
    
    ////////////////////////////////////////////////// draw ///////////////////////////////////////////////////
    cv::Mat outimg(image_height,image_width,CV_8UC3);
    outimg = 0;
    // Draw the fitted mesh as wireframe, and save the image:
    render::draw_wireframe(outimg, current_mesh, rendering_params.get_modelview(), rendering_params.get_projection(),
                           fitting::get_opencv_viewport(image_width, image_height));
    
    cv::imwrite(outputbasename + ".png", outimg);
    ////////////////////////////////////////////////// end of draw ///////////////////////////////////////////////////

    return EXIT_SUCCESS;
}

