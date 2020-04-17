# -*-coding: utf-8-*-

"""
采用68个关键点进行批量图片中人脸对齐操作
"""
import os
import sys
import dlib
import time
import numpy as np
from cv2 import cv2 as cv2

def load_model():
    '''
    1. 加载需要对齐的人脸照片；
    2. 加载特征点检测模型
    '''
    model_path = 'shape_predictor_68_face_landmarks.dat'

    detector = dlib.get_frontal_face_detector()
    landmark = dlib.shape_predictor(model_path)

    return detector, landmark

def generate_landmarks(file_path, save_path):
    imgs = os.listdir(file_path)
    # imgs.remove('.DS_Store')
    for img_name in imgs:
        img_full_path = file_path + '/' + img_name
        img = cv2.imread(img_full_path)

        # 取灰度
        img_gray = cv2.cvtColor(img, cv2.COLOR_RGB2GRAY)

        # 人脸数rects
        rects = detector(img_gray, 0)
        for i in range(len(rects)):
            landmarks = np.matrix([[p.x, p.y] for p in landmark(img,rects[i]).parts()])

        np.save(save_path + str(img_name.split('.')[0]), landmarks)

if __name__ == '__main__':

    detector, landmark = load_model()
    save_path = 'pts_result/'
    if not os.path.exists(save_path):
        os.mkdir(save_path)
    start = time.time()
    generate_landmarks(file_path='faces_result',
                                       save_path=save_path)
    end = time.time()
    print('Operate Finished | Costed time:{} s'.format(end-start))
