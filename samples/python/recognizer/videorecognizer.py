'''
    * Copyright (C) 2011-2020 Doubango Telecom <https://www.doubango.org>
    * File author: Mamadou DIOP (Doubango Telecom, France).
    * License: For non commercial use only.
    * Source code: https://github.com/DoubangoTelecom/ultimateALPR-SDK
    * WebSite: https://www.doubango.org/webapps/alpr/


    https://github.com/DoubangoTelecom/ultimateALPR/blob/master/SDK_dist/samples/c++/recognizer/README.md
	Usage: 
		recognizer.py \
			--image <path-to-image-with-plate-to-recognize> \
			[--assets <path-to-assets-folder>] \
            [--charset <recognition-charset:latin/korean/chinese>] \
			[--tokenfile <path-to-license-token-file>] \
			[--tokendata <base64-license-token-data>]
	Example:
		recognizer.py \
			--image C:/Projects/GitHub/ultimate/ultimateALPR/SDK_dist/assets/images/lic_us_1280x720.jpg \
            --charset "latin" \
			--assets C:/Projects/GitHub/ultimate/ultimateALPR/SDK_dist/assets \
			--tokenfile C:/Projects/GitHub/ultimate/ultimateALPR/SDK_dev/tokens/windows-iMac.lic
'''

import ultimateAlprSdk
import sys
import argparse
import json
import platform
import os.path
from PIL import Image, ExifTags, ImageDraw
import cv2
import os
import json
import numpy as np
import time
import cProfile, pstats

# EXIF orientation TAG
ORIENTATION_TAG = [orient for orient in ExifTags.TAGS.keys() if ExifTags.TAGS[orient] == 'Orientation']

# Tag for logging
TAG = "[UltAlprSdk] "

# Defines the default JSON configuration. More information at https://www.doubango.org/SDKs/anpr/docs/Configuration_options.html
JSON_CONFIG = {
    "debug_level": "info",
    "debug_write_input_image_enabled": False,
    "debug_internal_data_path": ".",
    
    "num_threads": -1,
    "gpgpu_enabled": True,
    "max_latency": -1,

    "klass_vcr_gamma": 1.5,
    
    "detect_roi": [0, 0, 0, 0],
    "detect_minscore": 0.1,

    "car_noplate_detect_min_score": 0.8,
    
    "pyramidal_search_enabled": True,
    "pyramidal_search_sensitivity": 0.28,
    "pyramidal_search_minscore": 0.3,
    "pyramidal_search_min_image_size_inpixels": 800,
    
    "recogn_minscore": 0.3,
    "recogn_score_type": "min"
}

# These are the strips on which if vehicle comes, gets counted
checkBoxout = (0.554,0.60)
checkBoxin = (0.36,0.41)

video_address = "/home/guco/Software/LPR/ultimateALPR-SDK/assets/images/traffic.mp4"
video = cv2.VideoCapture(video_address)
savedVideo = None  # This will be video created after processing the frames.

address = "https://192.168.43.1:8080/video"
# #If you want to capture images from webcam, uncoment this
#video.open(address)

# Some initializations
count = 0
format = ultimateAlprSdk.ULTALPR_SDK_IMAGE_TYPE_RGB24

# detectedCars stores objects of all cars detected till now. lastFrameCars and currFrameCars store cars detected in last and current frame resp.
# These get updated time to time.
detectedCars = {}
lastFrameCars = {}
currFrameCars = {}
imageSize = (720,1280,3)

# Object of this class are detected cars only. Car has id, plate-coordinates, car-coordinates, speed, frame-number atributes.
# speed of car will be updated frame by frame. In count and out count will be set only once.
class Cars():
    id = 1
    imageSize = (None,None)
    incomingCount = 0
    outgoingCount = 0
    checkBoxout = None
    checkBoxin = None
    def __init__(self,i,frameNo):
        self.text = i['text']
        self.plateOrdinates = i['warpedBox']
        self.carOrdinates = i['car']['warpedBox']
        self.id = Cars.id
        Cars.id += 1
        self.speed = 0
        self.countSet = False
        self.frameNo = frameNo
        self.setCount()
        
    def isCountSet(self):
        return(self.countSet)
    def getSpeed(self):
        return(self.speed)
    def getText(self):
        return(self.text)
    def getCarOrdinates(self):
        return(self.carOrdinates)
    def getPlateOrdinates(self):
        return(self.plateOrdinates)
    # When car is detected, corresponding object will be created. Speed is not set at that time. Speed will be set from next frame.
    # Here i am defining speed as change in y co-ordinate per frame. processing this further with actual measurements could give us actual speed.
    def setSpeed(self,i,frameNo):
        newCarOrdinates = i['car']['warpedBox']
        v1 = (self.carOrdinates[1]+self.carOrdinates[7])/2
        v2 = (newCarOrdinates[1]+newCarOrdinates[7])/2
        t = self.frameNo-frameNo
        speed = abs((v1-v2)/t)
        if speed!=0 or speed<1e6:
            self.speed = speed
    # setting count. If car is found in left half, and is in the detection strip, then update outgoing count. Accordingly for incoming cars.
    def setCount(self,i=None):
        if self.countSet:
            return
        a1 = Cars.checkBoxout[0]
        b1 = Cars.checkBoxout[1]
        a2 = Cars.checkBoxin[0]
        b2 = Cars.checkBoxin[1]
        
        if i is not None:
            self.carOrdinates = i['car']['warpedBox']
            #self.frameNo = frameNo
        
        if (self.carOrdinates[0]+self.carOrdinates[2])/2 > Cars.imageSize[1]/2:
            if(self.carOrdinates[1]+self.carOrdinates[7])/2 > Cars.imageSize[0]*a2 and (self.carOrdinates[1]+self.carOrdinates[7])/2 < Cars.imageSize[0]*b2:
                Cars.incomingCount += 1
                self.countSet = True
        else:
            if(self.carOrdinates[1]+self.carOrdinates[7])/2 > Cars.imageSize[0]*a1 and (self.carOrdinates[1]+self.carOrdinates[7])/2 < Cars.imageSize[0]*b1:
                Cars.outgoingCount += 1
                self.countSet = True

# Intersection over Union of two bounding boxes.
# Required while tracking the same car.
def IOU(boxA, boxB):
    # determine the (x, y)-coordinates of the intersection rectangle
    xA = max(boxA[0], boxB[0])
    yA = max(boxA[1], boxB[1])
    xB = min(boxA[4], boxB[4])
    yB = min(boxA[5], boxB[5])
    # compute the area of intersection rectangle
    interArea = max(0, xB - xA) * max(0, yB - yA)
    # compute the area of both the prediction and ground-truth
    # rectangles
    boxAArea = (boxA[2] - boxA[0]) * (boxA[7] - boxA[1])
    boxBArea = (boxB[2] - boxB[0]) * (boxB[7] - boxB[1])
    # compute the intersection over union by taking the intersection
    # area and dividing it by the sum of prediction + ground-truth
    # areas - the interesection area
    iou = interArea / float(boxAArea + boxBArea - interArea)
    # return the intersection over union value
    return iou

# Operate function takes bounding boxes, text, frame number detected of a car, and creates new car object or updates earlier one.
# It also sets incoming, outgoing count and speed of car too.
def operate(i,frameNo):
    global detectedCars
    global lastFrameCars
    global currFrameCars
    text = i["text"]
    # It might happen that model detects same car but with different number case in next frame. That care is taken here.
    # If this text is already detected, then just update that prev. entry.
    if text in detectedCars:
        c1 = detectedCars[text]
        c1.setSpeed(i,frameNo)
        if not c1.isCountSet():
            c1.setCount(i)
        c1.frameNo = frameNo
        c1.carOrdinates = i['car']['warpedBox']
        c1.plateOrdinates = i['warpedBox']
        detectedCars[text] = c1
        currFrameCars[text] = c1
        
    # else, check if this car has IOU>0.58 with any other prev. detected cars(in last frame only). If yes, it means this is same car. else, create new entry.
    else:
        flag = False
        carOrdinates = i['car']['warpedBox']
        for ltext in lastFrameCars.copy():
            lfc = lastFrameCars[ltext]
            iou = IOU(lfc.carOrdinates, carOrdinates)
            if iou>=0.58:
                lfc.setSpeed(i,frameNo)
                lfc.frameNo = frameNo
                lfc.carOrdinates = carOrdinates
                lfc.plateOrdinates = i['warpedBox']
                lastFrameCars.pop(ltext)
                detectedCars.pop(ltext)
                modifiedText = max(ltext,i['text'])
                lfc.text = modifiedText
                detectedCars[modifiedText] = lfc
                currFrameCars[modifiedText] = lfc
                if not lfc.isCountSet():
                    lfc.setCount()
                flag = True
                break
        if not flag:
            c1 = Cars(i,frameNo)
            detectedCars[text] = c1
            currFrameCars[text] = c1

# Returns texts and bounding boxes of cars in current frame added after detection and processing
def getTW():
    global currFrameCars
    texts_lst =  []
    warpedBox = []
    for text in currFrameCars:
        c1 = currFrameCars[text]
        texts_lst.append(c1.getText())
        warpedBox.append((c1.getPlateOrdinates(),c1.getCarOrdinates()))        
    return(texts_lst,warpedBox)
    
    
    
# Check result. Helper function
def checkResult(operation, result):
    warpedBoxes = []
    texts_lst = []
    if not result.isOK():
        print(TAG + operation + ": failed -> " + result.phrase())
    else:
        data = json.loads(result.json())
        if 'plates' in data:
            print(data['frame_id'])
            for i in data['plates']:
                if 'car' in i:
                    print("{} : {}".format('car',i['text']))
                    #lst.append((i['warpedBox'],i['car']['warpedBox']))
                    operate(i,data['frame_id'])
        texts_lst, warpedBoxes = getTW()
        print(texts_lst)
                    # i['wrappedBox] is number plate. i['car']['wrappedBox'] is car
    return(warpedBoxes, texts_lst)

# Main predict function that runs the model for given frame and calls other helper functions for processing and data updating.
def predict(args,frame):

    # Check if image exist
    if not os.path.isfile(args.image):
        print(TAG + "File doesn't exist: %s" % args.image)
        assert False

    # Decode the image
    image = Image.open(args.image)
    #image = frame

    width, height = image.size
    # Read the EXIF orientation value
    exif = image._getexif()
    exifOrientation = exif[ORIENTATION_TAG[0]] if len(ORIENTATION_TAG) == 1 and exif != None else 1

    # Update JSON options using values from the command args
    
    global count
    if count==0:
        JSON_CONFIG["assets_folder"] = args.assets
        JSON_CONFIG["charset"] = args.charset
        JSON_CONFIG["car_noplate_detect_enabled"] = (args.car_noplate_detect_enabled == "True")
        JSON_CONFIG["ienv_enabled"] = (args.ienv_enabled == "True")
        JSON_CONFIG["openvino_enabled"] = (args.openvino_enabled == "True")
        JSON_CONFIG["openvino_device"] = args.openvino_device
        JSON_CONFIG["klass_lpci_enabled"] = (args.klass_lpci_enabled == "True")
        JSON_CONFIG["klass_vcr_enabled"] = (args.klass_vcr_enabled == "True")
        JSON_CONFIG["klass_vmmr_enabled"] = (args.klass_vmmr_enabled == "True")
        JSON_CONFIG["klass_vbsr_enabled"] = (args.klass_vbsr_enabled == "True")
        JSON_CONFIG["license_token_file"] = args.tokenfile
        JSON_CONFIG["license_token_data"] = args.tokendata

    # Initialize the engine
    
        checkResult("Init", 
                ultimateAlprSdk.UltAlprSdkEngine_init(json.dumps(JSON_CONFIG))
               )
        count=1

    # Recognize/Process
    # Please note that the first time you call this function all deep learning models will be loaded 
    # and initialized which means it will be slow. In your application you've to initialize the engine
    # once and do all the recognitions you need then, deinitialize it.
    
    warpedBox,texts_lst = checkResult("Process",
                ultimateAlprSdk.UltAlprSdkEngine_process(
                    format,
                    image.tobytes(), # type(x) == bytes
                    width,
                    height,
                    0, # stride
                    exifOrientation
                    )
        )
    return(warpedBox,texts_lst)
    
# Run initially only. To check the FPS of input video. At the same  time, it does some initializations too. So make sure you take care of them if dont want to checkFPS
def checkFPS():
    global video
    global checkBoxout
    global checkBoxin
    global imageSize
    print("Checking FPS")
    t1 = time.time()
    for i in range(300):
        check,frame = video.read()
        print(".",end="")
        #key = cv2.waitKey(0)
    t2 = time.time()
    fps = int(300/(t2-t1))
    video.release()
    video = cv2.VideoCapture(video_address)
    print("\nDone. FPS: {}".format(fps))
    Cars.imageSize = frame.shape
    Cars.checkBoxout = checkBoxout
    Cars.checkBoxin = checkBoxin
    imageSize = frame.shape
    time.sleep(1)
    print("Cars.checkBoxout:",Cars.checkBoxout,"Cars.checkBoxin:",Cars.checkBoxin," imageSize:",imageSize," Cars.imageSize:",Cars.imageSize)
    return(fps)

# Return VideoWritter object
def videoWritterSetup(output_file):
    global savedVideo
    frame_width = int(video.get(3))
    frame_height = int(video.get(4))
    size = (frame_width, frame_height)
    #size = (720,480)
    fps = checkFPS()
    savedVideo = cv2.VideoWriter(output_file,cv2.VideoWriter_fourcc('m','p','4','v'),fps,size)
    return(savedVideo, fps)

# Displays processed image
def displayInCv2(warpedBox, texts, frame):
    # CV2 supprot rgb colour code in rectangle function.
    global imageSize
    global checkBoxout
    global checkBoxin
    global currFrameCars
    # Strip on which if vehicle comes, get counted
    x1 = 0
    x2 = int(imageSize[1]/2)-1
    y1 = int(checkBoxout[0]*imageSize[0])
    y2 = int(checkBoxout[1]*imageSize[0])-1
    shape = np.zeros_like(frame)
    shape = cv2.rectangle(shape,(x1,y1),(x2,y2),(0,0,255),cv2.FILLED)
    x1 = int(imageSize[1]/2)
    x2 = imageSize[1]-1
    y1 = int(checkBoxin[0]*imageSize[0])
    y2 = int(checkBoxin[1]*imageSize[0])-1
    shape = cv2.rectangle(shape,(x1,y1),(x2,y2),(0,0,255),cv2.FILLED)
    
    alpha = 0.5
    mask = shape.astype(bool)
    frame[mask] = cv2.addWeighted(frame, alpha, shape, 1 - alpha, 0)[mask]
    # base for printing in and out counts
    x1 = 30
    x2 = 200
    y1 = 20
    y2 = 100
    shape = np.zeros_like(frame)
    shape = cv2.rectangle(shape,(x1,y1),(x2,y2),(255,255,255),cv2.FILLED)
    mask = shape.astype(bool)
    alpha = 0.2 #for base only if you reduce alpha, base will become less transparent i.e. more opaque
    frame[mask] = cv2.addWeighted(frame, alpha, shape, 1 - alpha, 0)[mask]
    font = cv2.FONT_HERSHEY_DUPLEX
    fontSpeed = cv2.FONT_HERSHEY_TRIPLEX
    fontScale = 0.7
    fontScaleN = 0.9
    color = (0,200,255)
    colorN = (200,0,200)
    thickness= 1
    thicknessN = 2
    if warpedBox is not None:
        for obj,text in zip(warpedBox,texts):
            box1 = list(map(int,obj[0]))
            box2 = list(map(int,obj[1]))
            color1 = (255,0,0)
            color2 = (0,255,0)
            thickness1 = 1
            thickness2 = 2
            frame = cv2.rectangle(frame, (box1[0], box1[1]), (box1[4], box1[5]), color1, thickness1) #blue plate
            frame = cv2.putText(frame, text, (box1[0]-30,box1[1]), font, fontScaleN, color, thicknessN, cv2.LINE_AA) # number
            frame = cv2.rectangle(frame, (box2[0], box2[1]), (box2[4], box2[5]), color2, thickness2) #green vehicle
            org = (box2[0],box2[1])
            c1 = currFrameCars[text]
            # displaying speed just over bounding box
            frame = cv2.putText(frame,str("{0:.2f}".format(c1.getSpeed())),org,fontSpeed,fontScale,color,thickness,cv2.LINE_AA)
    # displaying in and out number
    org1 = (50, 50)
    org2 = (50,80)
    fontScale = 1
    color = (255, 0, 0)
    thickness = 2
    frame = cv2.putText(frame, "out:"+str(Cars.outgoingCount), org1, font, fontScale, color, thickness, cv2.LINE_AA)
    frame  = cv2.putText(frame, "in:"+str(Cars.incomingCount), org2, font, fontScale, color, thickness, cv2.LINE_AA)

    cv2.imshow("IPWebcam",frame)
    return(frame)
    


# Entry point
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="""
    This is the recognizer sample using python language
    """)

    parser.add_argument("--image", required=False, default="../../../assets/images/frame2.jpg", help="Path to the image with ALPR data to recognize")
    parser.add_argument("--video", required=True, help="Path to the video with ALPR data to recognize")
    parser.add_argument("--assets", required=False, default="../../../assets", help="Path to the assets folder")
    parser.add_argument("--duration", required=False, type=int, help="Maximum duration to process in seconds (default: entire video)")
    parser.add_argument("--charset", required=False, default="latin", help="Defines the recognition charset (a.k.a alphabet) value (latin, korean, chinese...)")
    parser.add_argument("--car_noplate_detect_enabled", required=False, default=False, help="Whether to detect and return cars with no plate")
    parser.add_argument("--ienv_enabled", required=False, default=platform.processor()=='i386', help="Whether to enable Image Enhancement for Night-Vision (IENV). More info about IENV at https://www.doubango.org/SDKs/anpr/docs/Features.html#image-enhancement-for-night-vision-ienv. Default: true for x86-64 and false for ARM.")
    parser.add_argument("--openvino_enabled", required=False, default=True, help="Whether to enable OpenVINO. Tensorflow will be used when OpenVINO is disabled")
    parser.add_argument("--openvino_device", required=False, default="CPU", help="Defines the OpenVINO device to use (CPU, GPU, FPGA...). More info at https://www.doubango.org/SDKs/anpr/docs/Configuration_options.html#openvino-device")
    parser.add_argument("--klass_lpci_enabled", required=False, default=False, help="Whether to enable License Plate Country Identification (LPCI). More info at https://www.doubango.org/SDKs/anpr/docs/Features.html#license-plate-country-identification-lpci")
    parser.add_argument("--klass_vcr_enabled", required=False, default=False, help="Whether to enable Vehicle Color Recognition (VCR). More info at https://www.doubango.org/SDKs/anpr/docs/Features.html#vehicle-color-recognition-vcr")
    parser.add_argument("--klass_vmmr_enabled", required=False, default=False, help="Whether to enable Vehicle Make Model Recognition (VMMR). More info at https://www.doubango.org/SDKs/anpr/docs/Features.html#vehicle-make-model-recognition-vmmr")
    parser.add_argument("--klass_vbsr_enabled", required=False, default=False, help="Whether to enable Vehicle Body Style Recognition (VBSR). More info at https://www.doubango.org/SDKs/anpr/docs/Features.html#vehicle-body-style-recognition-vbsr")
    parser.add_argument("--tokenfile", required=False, default="", help="Path to license token file")
    parser.add_argument("--tokendata", required=False, default="", help="Base64 license token data")

    args = parser.parse_args()

    video_address = args.video
    directory, filename = os.path.split(video_address)
    name, extension = os.path.splitext(filename)
    annotated_video_address = os.path.join(directory, f"{name}_annotated{extension}")

    print(f"Processing video file {video_address}")
    print(f"Output will be written to {annotated_video_address}")
    print(f"Using temp file {args.image}")


    pr = cProfile.Profile()

    pr.enable()

    image = Image.open(args.image)

    savedVideo, fps = videoWritterSetup(annotated_video_address)
    
    # Calculate maximum frames to process if duration is specified
    max_frames = None
    if args.duration:
        max_frames = fps * args.duration
        print(f"Processing first {args.duration} seconds ({max_frames} frames)")
    else:
        print("Processing entire video")
    
    try:
        frame_count = 0
        while True:
            check, frame = video.read()
            if not check:  # End of video
                break
                
            # Check if we've reached the maximum frames limit
            if max_frames and frame_count >= max_frames:
                print(f"Reached maximum duration limit ({args.duration} seconds)")
                break
                
            key = cv2.waitKey(1)
            frame_count += 1
            
            # Write frame to temp file for processing
            if os.path.isfile(args.image):
                os.remove(args.image)
            cv2.imwrite(args.image, frame)
            
            warpedBox, texts = predict(args, frame)
            
            frame = displayInCv2(warpedBox, texts, frame)
            #frame = cv2.resize(frame,(720,480),fx=0,fy=0,interpolation=cv2.INTER_CUBIC)
            savedVideo.write(frame)
            
            lastFrameCars = currFrameCars.copy()
            currFrameCars.clear()
            
            # Optional: Print progress every 100 frames
            if frame_count % 100 == 0:
                print(f"Processed {frame_count} frames...")
        
        print(f"Completed processing {frame_count} frames")

        pr.disable()

        stats = pstats.Stats(pr).sort_stats('cumtime')
        stats.dump_stats("pstats.prof")

    except KeyboardInterrupt:
        print("mission abort")
    #except Exception as e:
        #print(e)
    finally:
        cv2.destroyAllWindows
        video.release()
        savedVideo.release()
        checkResult("DeInit", 
                ultimateAlprSdk.UltAlprSdkEngine_deInit()
               )
        # save number plates of detected vehicles
        numberplates = []
        for i in detectedCars:
            numberplates.append(i)
        with open("numberplates.txt",'w') as f:
            f.write(str(numberplates))
        print(numberplates)
    
