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
    std::string modelfile, imgfile, blendshapesfile, outputbasename;
    
    {
        modelfile = std::string("../../../share/sfm_shape_3448.bin");
        imgfile = std::string("../../../share/3330.png");
        blendshapesfile = std::string("../../../share/expression_blendshapes_3448.bin");
        outputbasename = std::string("../../../share/3330out");
    }
    
    ////////////////////////////////////////////////////////// data ///////////////////////////////////////////////////////////////////
    vector<float> pca_shape_coefficients = {-8.7994e-02, -2.1412e-01,  1.9334e-01, -9.0814e-02,
         1.0191e-01, -1.1960e-01,  1.5348e-01, -2.3314e-03, -4.4998e-02,
         1.1687e-01,  1.1270e-01, -7.0829e-02, -2.8223e-02, -7.1665e-02,
        -1.0576e-01, -5.3201e-02, -1.5072e-01, -1.0871e-01,  6.5697e-02,
         6.7229e-02,  2.9475e-02,  1.3967e-01,  8.0471e-02, -1.1813e-02,
         1.4631e-02,  6.1900e-02,  4.7204e-02,  1.7153e-01, -6.4420e-03,
         2.8472e-02, -3.2023e-02,  6.8777e-02,  1.7580e-01, -6.1445e-02,
        -3.3121e-02,  6.1041e-02,  1.5347e-02, -8.8514e-02,  6.1835e-02,
        -1.4587e-01, -4.7661e-03,  4.8044e-02,  6.8582e-04, -5.4018e-02,
         2.6654e-02, -3.9256e-02,  4.7049e-02,  2.0279e-02,  4.2696e-02,
        -4.9311e-03,  6.1114e-02, -4.3168e-02, -5.0442e-02, -4.8499e-03,
         9.7734e-03, -1.1798e-01, -2.4533e-02, -2.3651e-02,  8.2454e-03,
        -4.1688e-02,  1.4737e-02,  3.9384e-02,  2.2273e-02};

    vector<float> blendshape_coefficients = {-9.0485e-02,
        2.1778e-01,  2.3190e-01, -1.3723e-01,  6.1431e-01, -1.6431e-01};
    
    float yaw = 8.2123e+00;     // yaw
    float roll = -4.3990e+00;   // roll
    float pitch = -5.9803e+00;   // pitch
    
    float tx = 1.2004e+02;
    float ty = 1.2098e+02;
    float scale = 2.7409e-01;
    ////////////////////////////////////////////////////////// end of data ///////////////////////////////////////////////////////////////////
    
    Mat image = cv::imread(imgfile);
    Mat outimg = image.clone();

    int image_width = image.cols;
    int image_height = image.rows;
    
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
    /////////////////////////////////////////////////////////// end of  loading model //////////////////////////////////////////////////////////////////////
  
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
    
    ////////////////////////////////////////////////// draw /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Draw the fitted mesh as wireframe, and save the image:
    render::draw_wireframe(outimg, current_mesh, rendering_params.get_modelview(), rendering_params.get_projection(),
                           fitting::get_opencv_viewport(image_width, image_height));
    
    cv::imwrite(outputbasename + ".png", outimg);
    ////////////////////////////////////////////////// end of draw ///////////////////////////////////////////////////

    // Save the mesh as textured obj:
//    core::write_textured_obj(current_mesh, outputbasename + ".obj");

    return EXIT_SUCCESS;
}

