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
    vector<float> pca_shape_coefficients = {-2.338609218597412, -1.2515982389450073, 0.1755952388048172, 1.033585786819458, -2.713702440261841, 1.216418981552124, 0.913641631603241, -0.3053925037384033, 0.7316317558288574, -1.1612648963928223, -1.2642675638198853, -0.3228277564048767, 0.6099429726600647, -0.6541866064071655, 0.40998780727386475, -0.13651646673679352, -1.7231858968734741, -2.158867835998535, 0.5417099595069885, 0.9988842010498047, 0.5702272653579712, 2.1864211559295654, 0.5520375370979309, 1.177590250968933, 0.775390625, -1.0329573154449463, 0.9921132922172546, -0.6110327243804932, -0.7987896203994751, 1.3898028135299683, -1.849816918373108, 0.2090795338153839, -0.07065387815237045, -0.4063229560852051, -0.17549015581607819, -0.6966468095779419, -0.3037428557872772, -0.16357864439487457, 0.0004648771428037435, 0.05237540230154991, 0.15677115321159363, -0.8662497997283936, 0.10988561809062958, -0.34472766518592834, -0.6893541812896729, -0.3189617097377777, 0.892291784286499, -0.7957141995429993, 0.5989113450050354, -0.23037709295749664, -0.3147507905960083, 0.10487079620361328, -0.09231892973184586, 0.045785605907440186, -0.5401266813278198, -0.5128871202468872, 0.34904050827026367, -0.03161684423685074, -0.07072675228118896, -0.27109166979789734, -0.5444401502609253, -0.2180466204881668, 0.12787102162837982};

    vector<float> blendshape_coefficients = {0.0, 0.0, 0.549663245677948, 0.19894109666347504, 0.0, 0.6341454386711121};
    
    float yaw = -19.548917770385742;     // yaw
    float roll = -0.662237823009491;   // roll
    float pitch = -11.274361610412598;   // pitch
    
    float tx = 85.71695709228516;
    float ty = 100.56600952148438;
    float scale = 1.271665334701538;
    ////////////////////////////////////////////////////////// end of data ///////////////////////////////////////////////////////////////////

    int image_width = 240;
    int image_height = 240;
    
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
    cv::Mat outimg(image_height,image_width,CV_8UC3);
    outimg = 0;
    // Draw the fitted mesh as wireframe, and save the image:
    render::draw_wireframe(outimg, current_mesh, rendering_params.get_modelview(), rendering_params.get_projection(),
                           fitting::get_opencv_viewport(image_width, image_height));
    
    cv::imwrite(outputbasename + ".png", outimg);
    ////////////////////////////////////////////////// end of draw ///////////////////////////////////////////////////

    // Save the mesh as textured obj:
    core::write_textured_obj(current_mesh, outputbasename + ".obj");

    return EXIT_SUCCESS;
}

